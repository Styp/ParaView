/*=========================================================================

  Program:   ParaView
  Module:    vtkPVApplication.h

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkPVApplication - Subclass of vtkKWApplication specific to the application
//
// .SECTION Description
// A subclass of vtkKWApplication specific to this application.

#ifndef __vtkPVApplication_h
#define __vtkPVApplication_h

#include "vtkKWApplication.h"
class vtkPVProcessModule;

class vtkDataSet;
class vtkKWMessageDialog;
class vtkKWPushButton;
class vtkMapper;
class vtkMultiProcessController;
class vtkSocketController;
class vtkPVOutputWindow;
class vtkPVSource;
class vtkPVWindow;
class vtkPVRenderView;
class vtkPolyDataMapper;
class vtkProbeFilter;
class vtkProcessObject;
class vtkPVApplicationObserver;
class vtkPVProgressHandler;
class vtkKWLoadSaveDialog;
class vtkSMApplication;
class vtkPVGUIClientOptions;
class vtkPVOptions;

class VTK_EXPORT vtkPVApplication : public vtkKWApplication
{
public:
  static vtkPVApplication* New();
  vtkTypeRevisionMacro(vtkPVApplication,vtkKWApplication);
  void PrintSelf(ostream& os, vtkIndent indent);
  
  // Description:
  // Parses the command line arguments and modifies the applications
  // ivars appropriately.
  // Return error (1) if the arguments are not formed properly.
  // Returns 0 if all went well.
  int ParseCommandLineArguments(int argc, char*argv[]);
  virtual void SetOptions(vtkPVGUIClientOptions* op);

  // Description:
  // Returns the server manager application.
  vtkGetObjectMacro(SMApplication, vtkSMApplication);

  //BTX
  // Description:
  // Process module contains all methods for managing 
  // processes and communication.
  void SetProcessModule(vtkPVProcessModule *module);
  vtkPVProcessModule* GetProcessModule() { return this->ProcessModule;}
  //ETX

  // Description:
  // Get a file selection dialog instance.
  virtual vtkKWLoadSaveDialog* NewLoadSaveDialog();


  // Description:
  // Start running the main application.
  virtual void Start(int argc, char *argv[]);
  virtual void Start()
    { this->vtkKWApplication::Start(); }
  virtual void Start(char* arg)
    { this->vtkKWApplication::Start(arg); }

  // Description:
  // We need to keep the controller in a prominent spot because there is no
  // more "RegisterAndGetGlobalController" method.
//BTX
  vtkMultiProcessController *GetController();
  
  // Description:
  // If ParaView is running in client server mode, then this returns
  // the socket controller used for client server communication.
  // It will only be set on the client and process 0 of the server.
  vtkSocketController *GetSocketController();
//ETX

  // Description:
  // No licence required.
  int AcceptLicense();
  int AcceptEvaluation();

  // Description:
  // This method is invoked when a window closes
  virtual void Close(vtkKWWindow *);

  // Description:
  // We need to kill the slave processes
  virtual void Exit();

  // Description:
  // Destroy the main window and associated objects without exiting.
  // This simply calls superclass' Exit() but does not tell remote
  // nodes to stop processing RMIs.
  virtual void DestroyGUI();
  
  // Description:
  // Initialize Tcl/Tk
  // Return NULL on error (eventually provides an ostream where detailed
  // error messages will be stored).
  //BTX
  static Tcl_Interp *InitializeTcl(int argc, char *argv[], ostream *err = 0);
  //ETX
  
  // Description:
  // Perform internal PV Application initialization.
  void Initialize();

//BTX
#ifdef PV_HAVE_TRAPS_FOR_SIGNALS
  // Description:
  // Setup traps for signals that may kill ParaView.
  void SetupTrapsForSignals(int nodeid);
  static void TrapsForSignals(int signal);

  // Description:
  // When error happens, try to exit as gracefully as you can.
  static void ErrorExit();
#endif // PV_HAVE_TRAPS_FOR_SIGNALS
//ETX

  // Description:
  // A start at recording macros in ParaView.  Create a custom trace file
  // that can be loaded back into paraview.  Window variables get
  // initialized when the file is opened.
  // Note: The trace entries get diverted to this file.  This is only used
  // for testing at the moment.  It is restricted to using sources created
  // after the recording is started.  The macro also cannot use the glyph
  // sources.  To make mocro recording available to the user, then there
  // must be a way of setting arguments (existing sources) to the macro,
  // and a way of prompting the user to set the arguments when the
  // macro/script is loaded.
  void StartRecordingScript(char *filename);
  void StopRecordingScript();

  // Description:
  // Since ParaView has only one window, we might as well provide access to it.
  vtkPVWindow *GetMainWindow();
  vtkPVRenderView *GetMainView();

  // Description:
  // Display the on-line help and about dialog for this application.
  // Over-writing vtkKWApplication defaults.
  void DisplayHelp(vtkKWWindow* master);

  // For locating help (.chm) on Windows.
  virtual int GetApplicationKey() 
    {
      return 15;
    };

  // Description:
  // Tells the process modules whether to start the main
  // event loop. Mainly used by command line argument parsing code
  // when an argument requires not starting the GUI
  vtkSetMacro(StartGUI, int);
  vtkGetMacro(StartGUI, int);

  //BTX
  static const char* const ExitProc;
  //ETX

  void DisplayTCLError(const char* message);

  // Description: 
  // Set or get the display 3D widgets flag.  When this flag is set,
  // the 3D widgets will be displayed when they are created. Otherwise
  // user has to enable them. User will still be able to disable the
  // 3D widget.
  vtkSetClampMacro(Display3DWidgets, int, 0, 1);
  vtkBooleanMacro(Display3DWidgets, int);
  vtkGetMacro(Display3DWidgets, int);

  // Description:
  // This is used internally for specifying how many pipes
  // to use for rendering when UseRenderingGroup is defined.
  // All processes have this set to the same value.
  vtkSetMacro(NumberOfPipes, int);
  vtkGetMacro(NumberOfPipes, int);

  // Description:
  // This is used (Unix only) to obtain the path of the executable.
  // This path is used to locate demos etc.
  vtkGetStringMacro(Argv0);

  // Description:
  // The name of the trace file.
  vtkGetStringMacro(TraceFileName);

  // Descrition:
  // Show/Hide the sources long help.
  virtual void SetShowSourcesLongHelp(int);
  vtkGetMacro(ShowSourcesLongHelp, int);
  vtkBooleanMacro(ShowSourcesLongHelp, int);

  // Descrition:
  // Show/Hide the sources long help.
  virtual void SetSourcesBrowserAlwaysShowName(int);
  vtkGetMacro(SourcesBrowserAlwaysShowName, int);
  vtkBooleanMacro(SourcesBrowserAlwaysShowName, int);

  // Descrition:
  // Get those application settings that are stored in the registery
  // Should be called once the application name is known (and the registery
  // level set).
  virtual void GetApplicationSettingsFromRegistery();

  // Description:
  // Enable or disable test errors. This refers to wether errors make test fail
  // or not.
  void EnableTestErrors();
  void DisableTestErrors();

  // Description:
  // Abort execution and display errors.
  static void Abort();

  // Description:
  // Execute event on callback
  void ExecuteEvent(vtkObject *o, unsigned long event, void* calldata);

  // Description:
  // Get number of partitions.
  int GetNumberOfPartitions();
  
  // Description:
  // Play the demo
  void PlayDemo(int fromDashboard);
  
  // Description:
  // Return the textual representation of the composite (i.e. its name and/or
  // its description. Memory is allocated, a pointer is return, it's up to
  // the caller to delete it.
  char* GetTextRepresentation(vtkPVSource* comp);

  // Description:
  // Send the stream represented by the given string to the client,
  // server, or both.  This should not be called by C++ code and is
  // provided only for debugging and testing purposes.  Returns 1 if
  // the string is successfully parsed and 0 otherwise.
  virtual int SendStringToClient(const char*);
  virtual int SendStringToClientAndServer(const char*);
  virtual int SendStringToClientAndServerRoot(const char*);
  virtual int SendStringToServer(const char*);
  virtual int SendStringToServerRoot(const char*);

  // Description:
  // Get a result stream represented by a string.  This should not be
  // called by C++ code and is provided only for debugging and testing
  // purposes.
  virtual const char* GetStringFromServer();
  virtual const char* GetStringFromClient();

  //BTX
  // Description:
  // Get application options.
  vtkPVGUIClientOptions* GetGUIClientOptions();
  vtkPVOptions* GetOptions();
  //ETX

  // Description:
  // Get/Set the global VTK garbage collection debugging flag.
  // Methods provided here to allow use from test scripts.
  void SetGarbageCollectionGlobalDebugFlag(int flag);
  int GetGarbageCollectionGlobalDebugFlag();

  // Description:
  // Push/Pop deferred garbage collection.
  void DeferredGarbageCollectionPush();
  void DeferredGarbageCollectionPop();

  // Description:
  // Save to a file the information available in the "About ParaView" dialog.
  void SaveRuntimeInformation();

protected:
  vtkPVApplication();
  ~vtkPVApplication();

  virtual void CreateSplashScreen();
  virtual void ConfigureAbout();
  virtual void AddAboutText(ostream &);

  vtkKWPushButton *SaveRuntimeInfoButton;

  void CreateButtonPhotos();
  void CreatePhoto(const char *name, 
                   const unsigned char *data, 
                   int width, int height, int pixel_size, 
                   unsigned long buffer_length = 0,
                   const char *filename = 0);
  int CheckRegistration();
  int PromptRegistration(char *,char *);

  virtual void FindApplicationInstallationDirectory();

  vtkPVProcessModule *ProcessModule;

  // For running with SGI pipes.
  int NumberOfPipes;

  int Display3DWidgets;

  int StartGUI;

  vtkPVOutputWindow *OutputWindow;

  static int CheckForExtension(const char* arg, const char* ext);
  char* CreateHelpString();
  int CheckForTraceFile(char* name, unsigned int len);
  void DeleteTraceFiles(char* name, int all);
  void SaveTraceFile(const char* fname);

  vtkSetStringMacro(TraceFileName);
  char* TraceFileName;
  char* Argv0;
  vtkSetStringMacro(Argv0);

  //BTX
  enum
  {
    NUM_ARGS=100
  };
  static const char ArgumentList[vtkPVApplication::NUM_ARGS][128];
  //ETX

  static vtkPVApplication* MainApplication;  

  int ShowSourcesLongHelp;
  int SourcesBrowserAlwaysShowName;

  vtkPVApplicationObserver* Observer;

  int ApplicationInitialized;

  vtkSMApplication* SMApplication;

  vtkPVGUIClientOptions* Options;

private:  
  vtkPVApplication(const vtkPVApplication&); // Not implemented
  void operator=(const vtkPVApplication&); // Not implemented
};

#endif


