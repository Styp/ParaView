/*=========================================================================

  Program:   ParaView
  Module:    $RCSfile$

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkIceTSynchronizedRenderers.h"

#include "vtkCamera.h"
#include "vtkCameraPass.h"
#include "vtkCullerCollection.h"
#include "vtkImageProcessingPass.h"
#include "vtkMultiProcessController.h"
#include "vtkObjectFactory.h"
#include "vtkPVDefaultPass.h"
#include "vtkRenderer.h"
#include "vtkRenderState.h"
#include "vtkRenderWindow.h"
#include "vtkSmartPointer.h"
#include "vtkTilesHelper.h"
#include "vtkTimerLog.h"

#include <assert.h>
#include <vtkgl.h>
#include <GL/ice-t.h>

#include <vtkstd/map>

// This pass is used to simply render an image onto the frame buffer. Used when
// an ImageProcessingPass is set to paste the IceT composited image into the
// frame buffer for th ImageProcessingPass.
class vtkMyImagePasterPass : public vtkRenderPass
{
public:
  static vtkMyImagePasterPass* New();
  vtkTypeMacro(vtkMyImagePasterPass, vtkRenderPass);

  virtual void Render(const vtkRenderState*)
    {
    if (this->Image.IsValid())
      {
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      this->Image.PushToFrameBuffer();
      }
    }

  void SetImage(const vtkSynchronizedRenderers::vtkRawImage& image)
    {
    this->Image = image;
    }

protected:
  vtkMyImagePasterPass()
    {
    }
  ~vtkMyImagePasterPass()
    {
    }
  vtkSynchronizedRenderers::vtkRawImage Image;
};
vtkStandardNewMacro(vtkMyImagePasterPass);

namespace
{
  class vtkMyCameraPass : public vtkCameraPass
  {
  vtkIceTCompositePass* IceTCompositePass;
public:
  vtkTypeMacro(vtkMyCameraPass, vtkCameraPass);
  static vtkMyCameraPass* New();

  // Description:
  // Set the icet composite pass.
  vtkSetObjectMacro(IceTCompositePass, vtkIceTCompositePass);

  virtual void GetTiledSizeAndOrigin(
    const vtkRenderState* render_state,
    int* width, int* height, int *originX,
    int* originY)
    {
    assert (this->IceTCompositePass != NULL);
    int tile_dims[2];
    this->IceTCompositePass->GetTileDimensions(tile_dims);
    if (tile_dims[0] > 1 || tile_dims[1]  > 1)
      {
      // we have a complicated relationship with tile-scale when we are in
      // tile-display mode :).
      // vtkPVSynchronizedRenderWindows sets up the tile-scale and origin on the
      // window so that 2D annotations work just fine. However that messes up
      // when we are using IceT since IceT will do the camera translations. So
      // for IceT's sake, we reset the tile_scale/tile_viewport when doing the
      // camera transformations. Of course, this is only an issue when rendering
      // for tile-displays.
      int tile_scale[2];
      double tile_viewport[4];
      render_state->GetRenderer()->GetRenderWindow()->GetTileScale(tile_scale);
      render_state->GetRenderer()->GetRenderWindow()->GetTileViewport(tile_viewport);
      render_state->GetRenderer()->GetRenderWindow()->SetTileScale(1, 1);
      render_state->GetRenderer()->GetRenderWindow()->SetTileViewport(0,0,1,1);
      this->Superclass::GetTiledSizeAndOrigin(render_state, width, height, originX, originY);
      render_state->GetRenderer()->GetRenderWindow()->SetTileScale(tile_scale);
      render_state->GetRenderer()->GetRenderWindow()->SetTileViewport(tile_viewport);

      *originX *= this->IceTCompositePass->GetTileDimensions()[0];
      *originY *= this->IceTCompositePass->GetTileDimensions()[1];
      *width *= this->IceTCompositePass->GetTileDimensions()[0];
      *height *= this->IceTCompositePass->GetTileDimensions()[1];
      }
    else
      {
      this->Superclass::GetTiledSizeAndOrigin(render_state, width, height, originX, originY);
      }
    }
protected:
  vtkMyCameraPass() {this->IceTCompositePass = NULL; }
  ~vtkMyCameraPass() { this->SetIceTCompositePass(0); }
  };
  vtkStandardNewMacro(vtkMyCameraPass);

  // vtkPVIceTCompositePass extends vtkIceTCompositePass to add some ParaView
  // specific rendering tweaks eg.
  // * render to full viewport
  // * don't let IceT paste back rendered images to the active frame buffer.
  class vtkPVIceTCompositePass : public vtkIceTCompositePass
  {
public:
  vtkTypeMacro(vtkPVIceTCompositePass, vtkIceTCompositePass);
  static vtkPVIceTCompositePass* New();

  // Description:
  // Updates some IceT context parameters to suit ParaView's need esp. in
  // multi-view configuration.
  virtual void SetupContext(const vtkRenderState* render_state)
    {
    this->Superclass::SetupContext(render_state);

    // Don't make icet render the composited image to the screen. We'll paste it
    // explicitly if needed. This is required since IceT/Viewport interactions
    // lead to weird results in multi-view configurations. Much easier to simply
    // paste back the image to the correct region after icet has rendered.
    icetDisable(ICET_DISPLAY);
    icetDisable(ICET_DISPLAY_INFLATE);
    icetDisable(ICET_CORRECT_COLORED_BACKGROUND);

    vtkRenderWindow* window = render_state->GetRenderer()->GetRenderWindow();
    int *size = window->GetActualSize();
    glViewport(0, 0, size[0], size[1]);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0, 0, 0, 0);
    }
protected:

  vtkPVIceTCompositePass()
    {
    vtkPVDefaultPass* defaultPass = vtkPVDefaultPass::New();
    this->SetRenderPass(defaultPass);
    defaultPass->Delete();
    }

  ~vtkPVIceTCompositePass()
    {
    }
  };
  vtkStandardNewMacro(vtkPVIceTCompositePass);

  // We didn't want to have a singleton for vtkIceTSynchronizedRenderers to
  // manage multi-view configurations. But, in tile-display mode, after each
  // view is rendered, the tiles end up with the residue of that rendered view
  // on all tiles. Which is not what is expected -- one would expect the views
  // that are present on those tiles to be drawn back. That becomes tricky
  // without a singleton. So we have a internal map that tracks all rendered
  // tiles.
  class vtkTile
    {
  public:
    vtkSynchronizedRenderers::vtkRawImage TileImage;

    // PhysicalViewport is the viewport where the TileImage maps into the tile
    // rendered by this processes i.e. the render window for this process.
    double PhysicalViewport[4];

    // GlobalViewport is the viewport for this image treating all tiles as a
    // single large display.
    double GlobalViewport[4];
    };

  typedef vtkstd::map<vtkIceTSynchronizedRenderers*, vtkTile> TilesMapType;
  static TilesMapType TilesMap;

  // Iterates over all valid tiles in the TilesMap and flush the images to the
  // screen.
  void FlushTiles(vtkRenderer* renderer)
    {
    for (TilesMapType::iterator iter = TilesMap.begin();
      iter != TilesMap.end(); ++iter)
      {
      vtkTile& tile = iter->second;
      if (tile.TileImage.IsValid())
        {
        double viewport[4];
        renderer->GetViewport(viewport);
        renderer->SetViewport(tile.PhysicalViewport);
        int tile_scale[2];
        renderer->GetVTKWindow()->GetTileScale(tile_scale);
        renderer->GetVTKWindow()->SetTileScale(1, 1);
        tile.TileImage.PushToViewport(renderer);
        renderer->GetVTKWindow()->SetTileScale(tile_scale);
        renderer->SetViewport(viewport);
        }
      }
    }

  void EraseTile(vtkIceTSynchronizedRenderers* ptr)
    {
    TilesMapType::iterator iter = TilesMap.find(ptr);
    if (iter != TilesMap.end())
      {
      TilesMap.erase(iter);
      }
    }
};

vtkStandardNewMacro(vtkIceTSynchronizedRenderers);
vtkCxxSetObjectMacro(vtkIceTSynchronizedRenderers, ImageProcessingPass, vtkImageProcessingPass);
//----------------------------------------------------------------------------
vtkIceTSynchronizedRenderers::vtkIceTSynchronizedRenderers()
{
  // First thing we do is create the ice-t render pass. This is essential since
  // most methods calls on this class simply forward it to the ice-t render
  // pass.
  this->IceTCompositePass = vtkPVIceTCompositePass::New();

  vtkMyCameraPass* cameraPass = vtkMyCameraPass::New();
  cameraPass->SetDelegatePass(this->IceTCompositePass);
  cameraPass->SetIceTCompositePass(this->IceTCompositePass);
  this->CameraRenderPass = cameraPass;
  this->SetParallelController(vtkMultiProcessController::GetGlobalController());

  this->ImagePastingPass = vtkMyImagePasterPass::New();

  this->ImageProcessingPass = NULL;
  this->RenderPass = NULL;
}

//----------------------------------------------------------------------------
vtkIceTSynchronizedRenderers::~vtkIceTSynchronizedRenderers()
{
  EraseTile(this);

  this->ImagePastingPass->Delete();
  this->IceTCompositePass->Delete();
  this->IceTCompositePass = 0;
  this->CameraRenderPass->Delete();
  this->CameraRenderPass = 0;
  this->SetImageProcessingPass(0);
  this->SetRenderPass(0);
}

//----------------------------------------------------------------------------
void vtkIceTSynchronizedRenderers::SetRenderPass(vtkRenderPass *pass)
{
  vtkSetObjectBodyMacro(RenderPass, vtkRenderPass, pass);
  if (this->IceTCompositePass)
    {
    if (pass)
      {
      this->IceTCompositePass->SetRenderPass(pass);
      }
    else
      {
      vtkPVDefaultPass* defaultPass = vtkPVDefaultPass::New();
      this->IceTCompositePass->SetRenderPass(defaultPass);
      defaultPass->Delete();
      }
    }
}

//----------------------------------------------------------------------------
void vtkIceTSynchronizedRenderers::HandleEndRender()
{
  if (this->WriteBackImages)
    {
    this->WriteBackImages = false;
    this->Superclass::HandleEndRender();
    this->WriteBackImages = true;
    }
  else
    {
    this->Superclass::HandleEndRender();
    }

  if (this->WriteBackImages)
    {
    vtkSynchronizedRenderers::vtkRawImage lastRenderedImage =
      this->CaptureRenderedImage();
    if (lastRenderedImage.IsValid())
      {
      vtkTile& tile = TilesMap[this];
      tile.TileImage = lastRenderedImage;
      this->IceTCompositePass->GetPhysicalViewport(tile.PhysicalViewport);
      }

    // Write-back either the freshly rendered tile or what was most recently
    // rendered.
    ::FlushTiles(this->Renderer);
    }
}

//----------------------------------------------------------------------------
void vtkIceTSynchronizedRenderers::SetRenderer(vtkRenderer* ren)
{
  if (this->Renderer && this->Renderer->GetPass() == this->CameraRenderPass)
    {
    this->Renderer->SetPass(NULL);
    }
  this->Superclass::SetRenderer(ren);
  if (ren)
    {
    ren->SetPass(this->CameraRenderPass);
    // icet cannot work correctly in tile-display mode is software culling is
    // applied in vtkRenderer inself. vtkPVIceTCompositePass will cull out-of-frustum
    // props using icet-model-view matrix later.
    ren->GetCullers()->RemoveAllItems();

    }
}

//----------------------------------------------------------------------------
void vtkIceTSynchronizedRenderers::SetImageReductionFactor(int val)
{
  // Don't call superclass. Since ice-t has better mechanisms for dealing with
  // image reduction factor rather than simply reducing the viewport. This
  // ensures that it works nicely in tile-display mode as well.
  // this->Superclass::SetImageReductionFactor(val);
  this->IceTCompositePass->SetImageReductionFactor(val);
}

//----------------------------------------------------------------------------
vtkSynchronizedRenderers::vtkRawImage&
vtkIceTSynchronizedRenderers::CaptureRenderedImage()
{
  // We capture the image from IceTCompositePass. This avoids the capture of
  // buffer from screen when not necessary.
  vtkRawImage& rawImage =
    (this->GetImageReductionFactor() == 1)?
    this->FullImage : this->ReducedImage;

  if (!rawImage.IsValid())
    {
    this->IceTCompositePass->GetLastRenderedTile(rawImage);
    if (!rawImage.IsValid())
      {
      // cout << "no image captured " << endl;
      //vtkErrorMacro("IceT couldn't provide a tile on this process.");
      }
    else if (this->ImageProcessingPass)
      {
      // process the rendered image using the image-processing pass.
      this->ImageProcessingPass->SetDelegatePass(this->ImagePastingPass);
      this->ImagePastingPass->SetImage(rawImage);

      double viewport[4];
      this->Renderer->GetViewport(viewport);
      int tile_scale[2];
      double tile_viewport[4];
      this->Renderer->GetVTKWindow()->GetTileScale(tile_scale);
      this->Renderer->GetVTKWindow()->GetTileViewport(tile_viewport);

      double physical_viewport[4];
      this->IceTCompositePass->GetPhysicalViewport(physical_viewport);

      physical_viewport[2]-=physical_viewport[0];
      physical_viewport[3]-=physical_viewport[1];
      physical_viewport[0] = physical_viewport[1] = 0;
      this->Renderer->SetViewport(physical_viewport);
      this->Renderer->GetVTKWindow()->SetTileScale(1, 1);
      this->Renderer->GetVTKWindow()->SetTileViewport(0,0, 1, 1);

      // update the glViewport and glScissor settings based on newly set
      // viewport.
      this->Renderer->GetActiveCamera()->UpdateViewport(this->Renderer);

      vtkRenderState state(this->Renderer);
      state.SetPropArrayAndCount(NULL, 0);
      state.SetFrameBuffer(0);
      glPushAttrib(GL_ENABLE_BIT);
      this->ImageProcessingPass->Render(&state);
      this->ImageProcessingPass->ReleaseGraphicsResources(
        this->Renderer->GetRenderWindow());
      glPopAttrib();

      // capture the framebuffer from the image processes pass and return that.
      rawImage.Capture(this->Renderer);

      this->Renderer->GetVTKWindow()->SetTileScale(tile_scale);
      this->Renderer->GetVTKWindow()->SetTileViewport(tile_viewport);
      this->Renderer->SetViewport(viewport);
      }
    }
  return rawImage;
}

//----------------------------------------------------------------------------
void vtkIceTSynchronizedRenderers::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
}
