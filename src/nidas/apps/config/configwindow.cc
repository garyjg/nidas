/*
 ********************************************************************
    Copyright 2009 UCAR, NCAR, All Rights Reserved

    $LastChangedDate:  $

    $LastChangedRevision: $

    $LastChangedBy:  $

    $HeadURL: http://svn/svn/nidas/trunk/src/nidas/apps/config/configwindow.cc $
 ********************************************************************
*/

#include <QtGui>
#include <ctime>
#include <list>
#include <iostream>
#include <fstream>
#include "sys/stat.h"

#include "configwindow.h"
#include "exceptions/exceptions.h"
#include "exceptions/QtExceptionHandler.h"
#include "exceptions/CuteLoggingExceptionHandler.h"
#include "exceptions/CuteLoggingStreamHandler.h"

using namespace nidas::core;
using namespace nidas::util;


ConfigWindow::ConfigWindow() :
   _noProjDir(false),
   _gvDefault("/Configuration/raf/GV_N677F/default.xml"),
   _c130Default("/Configuration/raf/C130_N130AR/default.xml"),
   _a2dCalDir("/Configuration/raf/cal_files/A2D/"),
   _pmsSpecsFile("/Configuration/raf/PMSspecs"),
   _filename(""), _curProjDir(""), _varDBfile(""), _fileOpen(false)
{
try {
    //if (!(exceptionHandler = new QtExceptionHandler()))
    //if (!(exceptionHandler = new CuteLoggingExceptionHandler(this)))
    if (!(exceptionHandler = new CuteLoggingStreamHandler(std::cerr,0)))
        throw 0;

    XMLPlatformUtils::Initialize();
    _errorMessage = new QMessageBox(this);
    setupDefaultDir();
    buildMenus();
    sensorComboDialog = new AddSensorComboDialog(_projDir+_a2dCalDir, _projDir+_pmsSpecsFile, this);
    dsmComboDialog = new AddDSMComboDialog(this);
    a2dVariableComboDialog = new AddA2DVariableComboDialog(this);
    variableComboDialog = new VariableComboDialog(this);
    newProjDialog = new NewProjectDialog(this);

    } catch (...) {
        InitializationException e("Initialization of the Configuration Viewer failed");
        throw e;
    }
}



void ConfigWindow::buildMenus()
{
    buildFileMenu();
    buildProjectMenu();
    buildWindowMenu();
    buildDSMMenu();
    buildSensorMenu();
    buildA2DVariableMenu();
    buildVariableMenu();
}


void ConfigWindow::buildFileMenu()
{
    QAction * ProjAct = new QAction(tr("New &Proj Config"), this);
    ProjAct->setShortcut(tr("Ctrl+P"));
    ProjAct->setStatusTip(tr("Create a new ADS configuration file"));
    connect(ProjAct, SIGNAL(triggered()), this, SLOT(newProj()));

    QAction * openAct = new QAction(tr("&Open"), this);
    openAct->setShortcut(tr("Ctrl+O"));
    openAct->setStatusTip(tr("Open an existing configuration file"));
    connect(openAct, SIGNAL(triggered()), this, SLOT(newFile()));

    QAction * saveAct = new QAction(tr("&Save"), this);
    saveAct->setShortcut(tr("Ctrl+S"));
    saveAct->setStatusTip(tr("Save a configuration file"));
    connect(saveAct, SIGNAL(triggered()), this, SLOT(saveOldFile()));

    QAction * saveAsAct = new QAction(tr("Save &As..."), this);
    saveAsAct->setShortcut(tr("Ctrl+A"));
    saveAsAct->setStatusTip(tr("Save configuration as a new file"));
    connect(saveAsAct, SIGNAL(triggered()), this, SLOT(saveAsFile()));

    QAction * exitAct = new QAction(tr("E&xit"), this);
    exitAct->setShortcut(tr("Ctrl+Q"));
    exitAct->setStatusTip(tr("Exit the application"));
    connect(exitAct, SIGNAL(triggered()), this, SLOT(quit()));

    QMenu * fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(ProjAct);
    fileMenu->addAction(openAct);
    fileMenu->addAction(saveAct);
    fileMenu->addAction(saveAsAct);
    fileMenu->addAction(exitAct);
}


void ConfigWindow::buildProjectMenu()
{
    QMenu * menu = menuBar()->addMenu(tr("&Project"));
    QAction * projEditAct = new QAction(tr("&Edit Name"), this);
    projEditAct->setShortcut(tr("Ctrl+E"));
    projEditAct->setStatusTip(tr("Edit the Project Name"));
    connect(projEditAct, SIGNAL(triggered()), this, SLOT(editProjName()));
    menu->addAction(projEditAct);
}


void ConfigWindow::quit()
{
cerr<<"ConfigWindow::quit() called \n";
    if (_fileOpen) 
        if (!askSaveFileAndContinue()) return;

    QCoreApplication::quit();
}



void ConfigWindow::buildWindowMenu()
{
    QMenu * menu = menuBar()->addMenu(tr("&Windows"));
    QAction * act;

    act = new QAction(tr("&Errors"), this);
    act->setStatusTip(tr("Toggle errors window"));
    act->setCheckable(true);
    act->setChecked(false);
    connect(act, SIGNAL(toggled(bool)), this, SLOT(toggleErrorsWindow(bool)));
    menu->addAction(act);
}



void ConfigWindow::buildSensorMenu()
{
    buildSensorActions();

    sensorMenu = menuBar()->addMenu(tr("&Sensor"));
    sensorMenu->addAction(addSensorAction);
    sensorMenu->addAction(editSensorAction);
    sensorMenu->addAction(deleteSensorAction);
    sensorMenu->setEnabled(false);
}

void ConfigWindow::buildDSMMenu()
{
    buildDSMActions();

    dsmMenu = menuBar()->addMenu(tr("&DSM"));
    dsmMenu->addAction(addDSMAction);
    dsmMenu->addAction(editDSMAction);
    dsmMenu->addAction(deleteDSMAction);
    dsmMenu->setEnabled(false);
}

void ConfigWindow::buildA2DVariableMenu()
{
    buildA2DVariableActions();

    a2dVariableMenu = menuBar()->addMenu(tr("&A2DVariable"));
    a2dVariableMenu->addAction(editA2DVariableAction);
    a2dVariableMenu->addAction(addA2DVariableAction);
    a2dVariableMenu->addAction(deleteA2DVariableAction);
    a2dVariableMenu->setEnabled(false);
}

void ConfigWindow::buildVariableMenu()
{
    buildVariableActions();

    variableMenu = menuBar()->addMenu(tr("&Variable"));
    variableMenu->addAction(editVariableAction);
    variableMenu->setEnabled(false);
}

void ConfigWindow::buildSensorActions()
{
    addSensorAction = new QAction(tr("&Add Sensor"), this);
    connect(addSensorAction, SIGNAL(triggered()), this, SLOT(addSensorCombo()));

    editSensorAction = new QAction(tr("&Edit Sensor"), this);
    connect(editSensorAction, SIGNAL(triggered()), this, 
            SLOT(editSensorCombo()));

    deleteSensorAction = new QAction(tr("&Delete Sensor"), this);
    connect(deleteSensorAction, SIGNAL(triggered()), this, SLOT(deleteSensor()));
}

void ConfigWindow::buildDSMActions()
{
    addDSMAction = new QAction(tr("&Add DSM"), this);
    connect(addDSMAction, SIGNAL(triggered()), this,  SLOT(addDSMCombo()));

    editDSMAction = new QAction(tr("&Edit DSM"), this);
    connect(editDSMAction, SIGNAL(triggered()), this, SLOT(editDSMCombo()));

    deleteDSMAction = new QAction(tr("&Delete DSM"), this);
    connect(deleteDSMAction, SIGNAL(triggered()), this, SLOT(deleteDSM()));
}

void ConfigWindow::buildA2DVariableActions()
{
    editA2DVariableAction = new QAction(tr("&Edit A2DVariable"), this);
    connect(editA2DVariableAction, SIGNAL(triggered()), this,  
            SLOT(editA2DVariableCombo()));

    addA2DVariableAction = new QAction(tr("&Add A2DVariable"), this);
    connect(addA2DVariableAction, SIGNAL(triggered()), this,  
            SLOT(addA2DVariableCombo()));

    deleteA2DVariableAction = new QAction(tr("&Delete A2DVariable"), this);
    connect(deleteA2DVariableAction, SIGNAL(triggered()), this, 
            SLOT(deleteA2DVariable()));
}

void ConfigWindow::buildVariableActions()
{
    editVariableAction = new QAction(tr("&Edit Variable"), this);
    connect(editVariableAction, SIGNAL(triggered()), this,  
            SLOT(editVariableCombo()));
}

void ConfigWindow::toggleErrorsWindow(bool checked)
{
    exceptionHandler->setVisible(checked);
}

void ConfigWindow::addSensorCombo()
{
    QModelIndexList indexList; // create an empty list
    sensorComboDialog->setModal(true);
    sensorComboDialog->show(model, indexList);
cerr<<"after call to addSensorCombo->show\n";
    tableview->resizeColumnsToContents();
}

void ConfigWindow::editSensorCombo()
{
  // Get selected index list and make sure it's only one 
  //    (see note in editA2DVariableCombo)
  QModelIndexList indexList = tableview->selectionModel()->selectedIndexes();
  if (indexList.size() > 6) {
    cerr << "ConfigWindow::editSensorCombo - found more than " <<
            "one row to edit\n";
    cerr << "indexList.size() = " << indexList.size() << "\n";
    return;
  }

  // allow user to edit variable
  sensorComboDialog->setModal(true);
  sensorComboDialog->show(model, indexList);
  tableview->resizeColumnsToContents ();
}

void ConfigWindow::deleteSensor()
{
  model->removeIndexes(tableview->selectionModel()->selectedIndexes());
  cerr << "ConfigWindow::deleteSensor after removeIndexes\n";
}

void ConfigWindow::addDSMCombo()
{
  QModelIndexList indexList; // create an empty list
  dsmComboDialog->setModal(true);
  dsmComboDialog->show(model, indexList);
  tableview->resizeColumnsToContents ();
}

void ConfigWindow::editDSMCombo()
{
  // Get selected index list and make sure it's only one 
  //    (see note in editA2DVariableCombo)
  QModelIndexList indexList = tableview->selectionModel()->selectedIndexes();
  if (indexList.size() > 2) {
    cerr << "ConfigWindow::editSensorCombo - found more than " <<
            "one row to edit\n";
    cerr << "indexList.size() = " << indexList.size() << "\n";
    return;
  }

  // allow user to edit DSM
  dsmComboDialog->setModal(true);
  dsmComboDialog->show(model, indexList);
  tableview->resizeColumnsToContents();
}

void ConfigWindow::deleteDSM()
{
  model->removeIndexes(tableview->selectionModel()->selectedIndexes());
  cerr << "ConfigWindow::deleteDSM after removeIndexes\n";
}

void ConfigWindow::addA2DVariableCombo()
{
  QModelIndexList indexList; // create an empty list
  a2dVariableComboDialog->setModal(true);
  a2dVariableComboDialog->show(model,indexList);
  tableview->resizeColumnsToContents ();
}

void ConfigWindow::editA2DVariableCombo()
{
  // Get selected indexes and make sure it's only one
  //   NOTE: properties should force this, but if it comes up may need to 
  //         provide a GUI indication.
  QModelIndexList indexList = tableview->selectionModel()->selectedIndexes();
  if (indexList.size() > 6) {
    cerr << "ConfigWindow::editA2DVariableCombo - found more than " <<
            "one row to edit \n";
    cerr << "indexList.size() = " << indexList.size() << "\n";
    return;
  }

  // allow user to edit/add variable
  a2dVariableComboDialog->setModal(true);
  a2dVariableComboDialog->show(model, indexList);
  tableview->resizeColumnsToContents ();
}

void ConfigWindow::deleteA2DVariable()
{
  model->removeIndexes(tableview->selectionModel()->selectedIndexes());
  cerr << "ConfigWindow::deleteA2DVariable after removeIndexes\n";
}

void ConfigWindow::editVariableCombo()
{
  // Get selected indexes and make sure it's only one
  //   NOTE: properties should force this, but if it comes up may need to 
  //         provide a GUI indication.
  QModelIndexList indexList = tableview->selectionModel()->selectedIndexes();
  if (indexList.size() > 4) {
    cerr << "ConfigWindow::editVariableCombo - found more than " <<
            "one row to edit \n";
    cerr << "indexList.size() = " << indexList.size() << "\n";
    return;
  }

  // allow user to edit/add variable
  variableComboDialog->setModal(true);
  variableComboDialog->show(model, indexList);
  tableview->resizeColumnsToContents ();
}

/*
 *  Setup _defaultDir and _defaultCaption class variables for use in opening/viewing files.
 */
void ConfigWindow::setupDefaultDir()
{
    char * _tmpStr;

    _tmpStr = getenv("PROJ_DIR");
    if (_tmpStr) {
       _defaultDir.append(_tmpStr);
       _projDir.append(_tmpStr);
    } else { // No $PROJ_DIR - warn user, set default to current and bail.
       _defaultCaption.append("No $PROJ_DIR!! ");
       QString firstPart("No $PROJ_DIR Environment Variable Defined.\n");
       QString secondPart("Configuration Editor will be missing some functionality.\n");
       QString thirdPart("Proceedinng using current working directory.\n");
       _errorMessage->setText(firstPart+secondPart+thirdPart);
       _errorMessage->exec();
       _tmpStr = getenv("PWD");
       _defaultDir.append(_tmpStr);
       _noProjDir = true;

       return;
    }

    if (_tmpStr) {
        _tmpStr = NULL;
        _tmpStr = getenv("PROJECT");
        if (_tmpStr)
        {
            _defaultDir.append("/");
            _defaultDir.append(_tmpStr);
        }
        else
            _defaultCaption.append("No $PROJECT.");
    }

    if (_tmpStr) {
        _tmpStr = NULL;
        _tmpStr = getenv("AIRCRAFT");
        if (_tmpStr)
        {
            _defaultDir.append("/");
            _defaultDir.append(_tmpStr);
            _defaultDir.append("/nidas");
        }
        else
            _defaultCaption.append(" No $AIRCRAFT.");
    }

    return;
}

bool ConfigWindow::fileExists(QString filename)
{
  struct stat buffer;
  if (stat(filename.toStdString().c_str(), &buffer) == 0) return true;
  return false;
}

void ConfigWindow::newProj()
{
  QString oldFileName = _filename;
  std::string projDir;
  char * tmpStr;

  tmpStr = getenv("PROJ_DIR"); 
  if (tmpStr) {
    projDir.append(tmpStr);
  } else {
    QString firstPart("No $PROJ_DIR Environment Variable Defined.\n");
    QString secondPart("Cannot create a new project.\n");
    _errorMessage->setText(firstPart+secondPart);
    _errorMessage->exec();
    return;
  }
  
  newProjDialog->show(projDir, &_filename);  // create new project
 
printf("after call to newprojdialog show\n");
cerr<<"ConfigWindow:: new project filename = " <<
         _filename.toStdString() << "\n";

  if (_filename != oldFileName) {  // user has created new project

    if (fileExists(_filename)) {
      openFile();
    } else {
      _errorMessage->setText("Unable to find newly created config file:" 
				+ _filename + ".  Something went wrong" +
				+ " in new Proj Generation!");
      _errorMessage->exec();
      _filename = oldFileName;
    }
  }

  return;
}

void ConfigWindow::newFile()
{
    if (_fileOpen) 
        if (!askSaveFileAndContinue()) return;
          
    getFile();
    openFile();
}

void ConfigWindow::getFile()
{
    QString caption;

    caption = _defaultCaption;
    caption.append(" Choose a file...");

    _filename = QFileDialog::getOpenFileName(
                0,
                caption,
                QString::fromStdString(_defaultDir),
                "Config Files (*.xml)");

    return;
}

bool ConfigWindow::openVarDB(std::string filename)
{

    extern long VarDB_RecLength, VarDB_nRecords;
    std::cerr<<"Filename = "<<_filename.toStdString()<<"\n";
    std::string temp = _filename.toStdString();
    size_t found;
    found=temp.find_last_of("/\\");
    temp = temp.substr(0,found); 
    found = temp.find_last_of("/\\");
    _curProjDir  = temp.substr(0,found); 
    _varDBfile=_curProjDir + "/VarDB";
    QString QsNcVarDBFile(QString::fromStdString(_varDBfile+".nc"));

    if (fileExists(QsNcVarDBFile)) {
        cerr << "Removing VarDB.nc \n";
        int i = unlink(QsNcVarDBFile.toStdString().c_str());
        if (i == -1 && errno != ENOENT) throw InternalProcessingException("Unable to remove VarDB.nc file!");
    }

    if (InitializeVarDB(_varDBfile.c_str()) == ERR)
    {
        _errorMessage->setText(QString::fromStdString
                 ("Could not initialize VarDB file: "
                  + _varDBfile + ".  Does it exist?"));
        _errorMessage->exec();
        return false;
    }

    if (VarDB_isNcML() == true)
    {
        QString msg("Configuration Editor needs the non-netCDF VARDB.");
        msg.append("We could not delete the netCDF version.");
        _errorMessage->setText(msg);
        _errorMessage->exec();
        return false;
    }

    SortVarDB();

    std::cerr<<"*******************  nrecs = "<<VarDB_nRecords<<"\n";
    return true;
}

void ConfigWindow::openFile()
{
    QString winTitle("Configview:  ");

    if (_filename.isNull() || _filename.isEmpty()) {
        cerr << "filename null/empty ; not opening" << endl;
        _errorMessage->setText("No File Selected...");
        _errorMessage->exec();
        winTitle.append("(no file selected)");
        setWindowTitle(winTitle);
        return;
        }
    else if (!openVarDB(_filename.toStdString())) 
        {
            cerr << "could not open varDB\n";
            winTitle.append("(could not open VarDB)");
            setWindowTitle(winTitle);
            return;
        }
    else {


        doc = new Document(this);
        doc->setFilename(_filename.toStdString());
        try {
            doc->parseFile();
        }
        catch (nidas::util::InvalidParameterException &e) {
            cerr<<"caught Exception InvalidParam: " << e.toString() << "\n";
            _errorMessage->setText(QString::fromStdString
                     ("Caught nidas InvalidParameterException: " 
                      + e.toString()));
            _errorMessage->exec();
            return;
         }

         try {
cerr<<"printSiteNames\n";
            doc->printSiteNames();

            QWidget *oldCentral = centralWidget();
            if (oldCentral) {
                cerr << "got an old central widget\n";
                cerr << "NAME: " << oldCentral->objectName().toStdString() << "\n";
                cerr << "INFO::\n";
                oldCentral->dumpObjectInfo();
                cerr << "\n\nTREE:\n";
                oldCentral->dumpObjectTree();
                cerr << "\n\n";

                    /* qt docs say call setCentralWidget() only once,
                       but we need to dump the old one to make for a new file
                     */
                setCentralWidget(0);
                show();
                }

            mainSplitter = new QSplitter(this);
            mainSplitter->setObjectName(QString("the horizontal splitter!!!"));

            buildSensorCatalog();
            buildA2DVarDB();
            dsmComboDialog->setDocument(doc);
            //sampleComboDialog->setDocument(doc);
            a2dVariableComboDialog->setDocument(doc);
            variableComboDialog->setDocument(doc);
            setupModelView(mainSplitter);

            setCentralWidget(mainSplitter);

            show(); // XXX

            winTitle.append(_filename);
            setWindowTitle(winTitle);  

        }
        catch (const CancelProcessingException & cpe) {
            // stop processing, show blank window
            _errorMessage->setText(QString::fromStdString
                     ("Caught CancelProcessingException" + cpe.toString()));
            _errorMessage->exec();
            return;

            QStatusBar *sb = statusBar();
            if (sb) sb->showMessage(QString::fromAscii(cpe.what()));
        }
        catch(...) {
            // stop processing, show blank window
            _errorMessage->setText(QString::fromStdString
                     ("Caught Unknown Exception"));
            _errorMessage->exec();

            exceptionHandler->handle("Project configuration file");
            return;
        }
    }

    QList<int> sizes = mainSplitter->sizes();
    sizes[0] = 300;
    sizes[1] = 700;
    mainSplitter->setSizes(sizes);

    tableview->resizeColumnsToContents ();
    _fileOpen = true;
    show();
    return;
}

void ConfigWindow::show()
{
  resize(1000,600);

  QMainWindow::show();
}

void ConfigWindow::editProjName()
{
cerr<<"In ConfigWindow::editProjName.  \n";
    string projName = doc->getProjectName();
cerr<<"In ConfigWindow::editProjName.  projName = " << projName << "\n";
    bool ok;
    QString text = QInputDialog::getText(this, tr("Edit Project Name"),
                                         tr("Project Name:"), QLineEdit::Normal,
                                         QString::fromStdString(projName), &ok);
cerr<< "after call to QInputDialog::getText\n";
    if (ok && !text.isEmpty()) {
      doc->setProjectName(text.toStdString());

      // Now put the project name into a file in the Project Directory  
      //      (needed by nimbus)
      std::string dir =  doc->getDirectory();
      std::string projDir(dir);
      size_t found;
      found = projDir.rfind('/');
      if (found!=string::npos)
        projDir.erase(found);
      else {
        string error;
        error = "Could not find Project Directory (parent of " +
             dir + " )" + "\nUnable to write ProjectName file." +
             "This will cause problems with nimbus.";
        _errorMessage->setText(QString::fromStdString(error));
        _errorMessage->exec();
        return;
      }
      std::string projFName;
      projFName = projDir + "/ProjectName";
      ofstream projFile(projFName.c_str());
      if (projFile)
        projFile.close();
        if (remove(projFName.c_str()) != 0) {
          string error;
          error = "Could not remove ProjectName file:" + projFName +
               + "\nUnable to write ProjectName file." +
               "This will cause problems with nimbus.";
          _errorMessage->setText(QString::fromStdString(error));
          _errorMessage->exec();
          return;
        }
      projFile.open(projFName.c_str(), ios::out);
      if (projFile.is_open()) {
        projFile << text.toStdString().c_str() << "\n";
        projFile.close();
      }
      else {
          string error;
          error = "Could not open file:" + projFName +
               + "\nUnable to write ProjectName file." +
               "This will cause problems with nimbus.";
          _errorMessage->setText(QString::fromStdString(error));
          _errorMessage->exec();
          return;
      }
   }

   return;
}

// QT oddity wrt argument passing forces this hack
void ConfigWindow::saveOldFile()
{
    saveFile("");
}

bool ConfigWindow::saveFile(string origFile)
{
    cerr << __func__ << endl;
    if (_filename == _projDir+_c130Default || _filename == _projDir+_gvDefault)
    {
      _errorMessage->setText(
             "Attempting to save default aircraft file - please use Save As");
      _errorMessage->exec();
      return false;
    }
    if (!saveFileCopy(origFile)) {
      _errorMessage->setText("FAILED to write copy of file.\n No backups");
      _errorMessage->exec();
    }
    if (!doc->writeDocument()) {
      _errorMessage->setText("FAILED TO WRITE FILE! Check permissions");
      _errorMessage->exec();
      return false;
    }
    // Now clean up xercesc's bizzare comment formatting
    std::string syscmd;
    std::string filename = doc->getFilename();
    std::string tmpfilename = filename + ".tmp";
    // remove all blank lines
    syscmd = "sed '/^ *$/d' " + filename + " > " + tmpfilename;
    system(syscmd.c_str());
    syscmd.clear();
    // move all start xml comments to first column
    syscmd = "sed 's/^ *<\\!/<\\!/' " + tmpfilename + " > " + filename;
    system(syscmd.c_str());
    syscmd.clear();
    // insert a blank line ahead of xml comments
    syscmd = "sed '/^ *<\\!/{x;p;x;}' " + filename + " > " + tmpfilename;
    system(syscmd.c_str());
    syscmd.clear();
    // final step in cleanup
    syscmd = "mv -f " + tmpfilename + " " + filename;
    system(syscmd.c_str());
    return true;
}

bool ConfigWindow::saveAsFile()
{
    QString qfilename;
    QString _caption;
    const std::string curFileName=doc->getFilename();

    string filename(doc->getDirectory());
    filename.append("/default.xml");

    qfilename = QFileDialog::getSaveFileName(
                0,
                _caption,
                QString::fromStdString(filename),
                "Config Files (*.xml)");

    cerr << "saveAs dialog returns " << qfilename.toStdString() << endl;

    if (qfilename.isNull() || qfilename.isEmpty()) {
        cerr << "qfilename null/empty ; not saving" << endl;
        _errorMessage->setText("No Filename chosen - not saving");
        _errorMessage->exec();
        return(false);
    }

    if (!qfilename.endsWith(".xml"))
      qfilename.append(".xml");

    doc->setFilename(qfilename.toStdString().c_str());
    _filename=qfilename;

    if (saveFile(curFileName)) {
      QString winTitle("Configview:  ");
      winTitle.append(_filename);
      setWindowTitle(winTitle);  
      return true;
    } else {
      doc->setFilename(curFileName);
      _filename=QString::fromStdString(curFileName);
      return false;
    }
}

bool ConfigWindow::saveFileCopy(string origFile)
{
  std::string saveFile = doc->getFilename();
  size_t fn = saveFile.rfind("/");
  std::string saveFname = saveFile.substr(fn+1);
  std::string saveDir = saveFile.substr(0, fn+1);
  std::string copyDir = saveDir + ".confedit";
  std::string copyFname;
  std::string copyFile;
  std::string fromFile;

  // Make copy directory if it doesn't already exist
  umask(0);
  struct stat st;
  if (stat(copyDir.c_str(), &st) != 0) { // create copy dir
    if (mkdir(copyDir.c_str(), S_IRWXU|S_IRWXG|S_IROTH|S_IXOTH) == -1) {
      cerr << "Could not create directory " << copyDir << " to save copies\n";
      return false;
    }
  }

  // set up the copy filename
  time_t now;
  char dateTime[64];
  dateTime[0] = '\0';
  now = time(NULL);
  if (now != -1) 
    strftime(dateTime, 64, "%Y%m%d-%H%M%S", gmtime(&now));
  else 
    cerr << "couldn't get timestamp for copy filename\n";
  copyFname = saveFname+"."+std::string(dateTime);
  copyFile = copyDir + "/" +copyFname;

  if (origFile.length() == 0) 
    fromFile = saveFile;
  else
    fromFile = origFile;

  ifstream src(fromFile.c_str(), ifstream::in);
  if (!src) {
    // see if a .xml was added by configwindow
    size_t found;
    found = fromFile.rfind(".xml");
    if (found!=string::npos) {
      string::iterator it;
      it = fromFile.begin()+found;
      fromFile.erase(it,fromFile.end());
    }
    else
      return false;

    src.open(fromFile.c_str(), ifstream::in);
    if (!src) {
      cerr << "Could not open source file : " << fromFile << "\n";
      return false;
    }
  }
  ofstream dest(copyFile.c_str(), ifstream::out);
  if (!dest) {
    cerr << "Could not open destination file: " << copyFile << "\n";
    return false;
  }

  dest << src.rdbuf();
  if (!dest)
  {
     cerr << "Error while copying from: \n" << fromFile << 
             "\n to: \n" << copyFile << "\n";
     return false;
  }

  cerr << "copied from: \n" << fromFile << 
          "\n to: \n" << copyFile << "\n";

  return true;
}

void ConfigWindow::setupModelView(QSplitter *splitter)
{
  model = new NidasModel(Project::getInstance(), doc->getDomDocument(), this);

  treeview = new QTreeView(splitter);
  treeview->setModel(model);
  treeview->header()->hide();

  tableview = new QTableView(splitter);
  tableview->setModel( model );
  tableview->setSelectionModel( treeview->selectionModel() );  /* common selection model */
  tableview->setSelectionBehavior( QAbstractItemView::SelectRows );
  tableview->setSelectionMode( QAbstractItemView::SingleSelection );

  //connect(treeview, SIGNAL(pressed(const QModelIndex &)), this, SLOT(changeToIndex(const QModelIndex &)));
  connect(treeview->selectionModel(), SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)), this, SLOT(changeToIndex(const QItemSelection &)));

  treeview->setCurrentIndex(treeview->rootIndex().child(0,0));

  splitter->addWidget(treeview);
  splitter->addWidget(tableview);
}


/*!
 * \brief Display and setup the correct actions for the index in \a selections.
 *
 * This version is for selectionModel's selectionChanged signal.
 * We use only the first element of \a selections,
 * expecting that the view(s) allow only one selection at a time.
 */
void ConfigWindow::changeToIndex(const QItemSelection & selections)
{
  QModelIndexList il = selections.indexes();
  if (il.size()) {
    changeToIndex(il.at(0));
    tableview->resizeColumnsToContents ();
  }
  else throw InternalProcessingException("selectionChanged signal provided no selections");
}


/*!
 * \brief Display and setup the correct actions for the current \a index.
 *
 * Set the table view's root index to the parent of \a index
 * and tell the model same so it returns correct headerData.
 * En/dis-able appropriate actions, e.g. add or delete choices...
 */
void ConfigWindow::changeToIndex(const QModelIndex & index)
{
  tableview->setRootIndex(index.parent());
  tableview->scrollTo(index);

  model->setCurrentRootIndex(index.parent());


  NidasItem *parentItem = model->getItem(index.parent());

//parentItem->setupyouractions(ahelper);
  //ahelper->addSensor(true);

    tableview->setSortingEnabled(true);
    tableview->sortByColumn(0, Qt::AscendingOrder);
    tableview->sortByColumn(1, Qt::AscendingOrder);

  if (dynamic_cast<DSMItem*>(parentItem))  sensorMenu->setEnabled(true); 
  else sensorMenu->setEnabled(false);
  
  if (dynamic_cast<SiteItem*>(parentItem)) dsmMenu->setEnabled(true);
  else dsmMenu->setEnabled(false);

  if (dynamic_cast<A2DSensorItem*>(parentItem)) {
    a2dVariableMenu->setEnabled(true);
    tableview->setSortingEnabled(true);
    tableview->sortByColumn(0, Qt::AscendingOrder);
  }
  else {
    a2dVariableMenu->setEnabled(false);
    tableview->setSortingEnabled(false);
  }

  if (dynamic_cast<SensorItem*>(parentItem) &&
      !dynamic_cast<A2DSensorItem*>(parentItem)) 
    variableMenu->setEnabled(true);
  else variableMenu->setEnabled(false);

  // fiddle with context-dependent menus/toolbars
  /*
  XXX
  ask model: what are you
  (dis)able menu/toolbars
  
  NidasItem *item = model->data(index)
  qt::install(item->menus());
  */
}


void ConfigWindow::buildSensorCatalog()
//  Construct the Sensor Catalog drop-down
{
Project *project = Project::getInstance();

    if(!project->getSensorCatalog()) {
        cerr<<"Configuration file doesn't contain a Sensor catalog!!"<<endl;
        return;
    }

    cerr<<"Putting together sensor Catalog"<<endl;

    sensorComboDialog->SensorBox->clear();
    sensorComboDialog->SensorBox->addItem("Analog");

    map<string,xercesc::DOMElement*>::const_iterator mi;
    const map<std::string,xercesc::DOMElement*>& scMap = project->getSensorCatalog()->getMap();
    for (mi = scMap.begin(); mi != scMap.end(); mi++) {
        cerr<<"   - adding sensor:"<<(*mi).first<<endl;
        sensorComboDialog->SensorBox->addItem(QString::fromStdString(mi->first));
    }

    sensorComboDialog->setDocument(doc);
    return;
}

void ConfigWindow::buildA2DVarDB()
//  Construct the A2D Variable Drop Down list from analog VarDB elements
{
    extern long VarDB_RecLength, VarDB_nRecords;

    cerr<<__func__<<": Putting together A2D Variable list\n";
    cerr<< "    - number of vardb records = " << VarDB_nRecords << "\n";
    map<string,xercesc::DOMElement*>::const_iterator mi;

    a2dVariableComboDialog->VariableBox->clear();
    a2dVariableComboDialog->VariableBox->addItem("New");

    for (int i = 0; i < VarDB_nRecords; ++i)
    {
        if ((((struct var_v2 *)VarDB)[i].is_analog) != 0) {
            QString temp(((struct var_v2 *)VarDB)[i].Name);
            a2dVariableComboDialog->VariableBox->addItem(temp);
        } 
    }
    return;
}

bool ConfigWindow::askSaveFileAndContinue()
// Check to see if user would like current file saved
{
    QMessageBox msgBox;
    msgBox.setText("You may have modified the configuration.");
    msgBox.setInformativeText("Do you want to save your changes?");
    msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard 
                              | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Save);

    int ret = msgBox.exec();

    switch (ret) {
        case QMessageBox::Save:
            saveFile(doc->getFilename());
            return true;
            break;
        case QMessageBox::Discard:
            return true;
            break;
        case QMessageBox::Cancel:
            return false;
            break;
        default:
            // should never be reached
            return false;
            break;
    }
}
