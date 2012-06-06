/*=Plus=header=begin======================================================
  Program: Plus
  Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
  See License.txt for details.
=========================================================Plus=header=end*/ 

#include "ConfigurationToolbox.h"

#include "fCalMainWindow.h"
#include "vtkObjectVisualizer.h"
#include "vtkPhantomRegistrationAlgo.h"
#include "FidPatternRecognition.h"

#include "DeviceSetSelectorWidget.h"
#include "ToolStateDisplayWidget.h"

#include <QDialog>
#include <QFileDialog>
#include <QFile>

#include "vtkXMLUtilities.h"
#include "vtkXMLDataElement.h"
#include "vtksys/SystemTools.hxx"
#include "vtkLineSource.h"

//-----------------------------------------------------------------------------

ConfigurationToolbox::ConfigurationToolbox(fCalMainWindow* aParentMainWindow, Qt::WFlags aFlags)
	: AbstractToolbox(aParentMainWindow)
	, QWidget(aParentMainWindow, aFlags)
	, m_ToolStatePopOutWindow(NULL)
  , m_IsToolDisplayDetached(false)
{
	ui.setupUi(this);

	// Create and setup device set selector widget
	m_DeviceSetSelectorWidget = new DeviceSetSelectorWidget(this);

  QFile styleSheetFile(":/styles/Resources/style_DeviceSetComboBox.qss");
  styleSheetFile.open(QIODevice::ReadOnly);
  QByteArray styleByteArray = styleSheetFile.readAll();
  QString styleSheet(styleByteArray.data());
  m_DeviceSetSelectorWidget->setStyleSheet(styleSheet);

  // Create tool box state widget
	m_ToolStateDisplayWidget = new ToolStateDisplayWidget(this);
	m_ToolStateDisplayWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

	// Make connections
	connect( m_DeviceSetSelectorWidget, SIGNAL( ConnectToDevicesByConfigFileInvoked(std::string) ), this, SLOT( ConnectToDevicesByConfigFile(std::string) ) );
	connect( ui.pushButton_PopOut, SIGNAL( toggled(bool) ), this, SLOT( PopOutToggled(bool) ) );
	connect( ui.comboBox_LogLevel, SIGNAL( currentIndexChanged(int) ), this, SLOT( LogLevelChanged(int) ) );
	connect( ui.pushButton_SelectEditorApplicationExecutable, SIGNAL( clicked() ), this, SLOT( SelectEditorApplicationExecutable() ) );
	connect( ui.pushButton_SelectImageDirectory, SIGNAL( clicked() ), this, SLOT( SelectImageDirectory() ) );

	// Insert widgets into placeholders
	QGridLayout* gridDeviceSetSelection = new QGridLayout(ui.deviceSetSelectionWidget, 1, 1, 0, 4, "");
	gridDeviceSetSelection->addWidget(m_DeviceSetSelectorWidget);
	ui.deviceSetSelectionWidget->setMinimumHeight(196);
	ui.deviceSetSelectionWidget->setLayout(gridDeviceSetSelection);

	QGridLayout* gridToolStateDisplay = new QGridLayout(ui.toolStateDisplayWidget, 1, 1, 0, 4, "");
	gridToolStateDisplay->addWidget(m_ToolStateDisplayWidget);
	ui.toolStateDisplayWidget->setLayout(gridToolStateDisplay);
	ui.toolStateDisplayWidget->setMinimumHeight(m_ToolStateDisplayWidget->GetDesiredHeight());
	ui.toolStateDisplayWidget->setMaximumHeight(m_ToolStateDisplayWidget->GetDesiredHeight());

  // Set application configuration
  ui.comboBox_LogLevel->blockSignals(true);
  ui.comboBox_LogLevel->setCurrentIndex(vtkPlusLogger::Instance()->GetLogLevel() - 1);
  ui.comboBox_LogLevel->blockSignals(false);

  ui.lineEdit_EditorApplicationExecutable->setText( QDir::toNativeSeparators(QString(vtkPlusConfig::GetInstance()->GetEditorApplicationExecutable())) );
  ui.lineEdit_ImageDirectory->setText( QDir::toNativeSeparators(QString(vtkPlusConfig::GetInstance()->GetImageDirectory())) );
}

//-----------------------------------------------------------------------------

ConfigurationToolbox::~ConfigurationToolbox()
{
}

//-----------------------------------------------------------------------------

void ConfigurationToolbox::Initialize()
{
	LOG_TRACE("ConfigurationToolbox::Initialize"); 

  SetDisplayAccordingToState();
}

//-----------------------------------------------------------------------------

void ConfigurationToolbox::RefreshContent()
{
	//LOG_TRACE("ConfigurationToolbox::RefreshToolboxContent"); 

	if (m_ToolStateDisplayWidget->IsInitialized()) {
		m_ToolStateDisplayWidget->Update();
	}
}

//-----------------------------------------------------------------------------

void ConfigurationToolbox::RefreshToolDisplayIfDetached()
{
	//LOG_TRACE("ConfigurationToolbox::RefreshToolDisplayIfDetached"); 

	if (m_IsToolDisplayDetached && m_ToolStateDisplayWidget->IsInitialized()) {
		m_ToolStateDisplayWidget->Update();
	}
}

//-----------------------------------------------------------------------------

void ConfigurationToolbox::SetDisplayAccordingToState()
{
  LOG_TRACE("ConfigurationToolbox::SetDisplayAccordingToState");

  m_ParentMainWindow->SetImageManipulationEnabled(false);

  // No state handling in this toolbox
  if (m_ParentMainWindow->AreDevicesShown() == false)
  {
    m_ParentMainWindow->GetObjectVisualizer()->HideAll();
  }
}

//-----------------------------------------------------------------------------

void ConfigurationToolbox::ConnectToDevicesByConfigFile(std::string aConfigFile)
{
	LOG_TRACE("ConfigurationToolbox::ConnectToDevicesByConfigFile");

  QApplication::setOverrideCursor(QCursor(Qt::BusyCursor));

  // If not empty, then try to connect; empty parameter string means disconnect
  if (STRCASECMP(aConfigFile.c_str(), "") != 0)
  {
    // Read configuration
    vtkSmartPointer<vtkXMLDataElement> configRootElement = vtkSmartPointer<vtkXMLDataElement>::Take(vtkXMLUtilities::ReadElementFromFile(aConfigFile.c_str()));
    if (configRootElement == NULL)
    {	
      LOG_ERROR("Unable to read configuration from file " << aConfigFile); 
      return;
    }

    vtkPlusConfig::GetInstance()->SetDeviceSetConfigurationData(configRootElement); 

	  // If connection has been successfully created then start data collection
	  if (! m_DeviceSetSelectorWidget->GetConnectionSuccessful())
    {
		  LOG_INFO("Connect to devices"); 

      // Disable main window
     	m_ParentMainWindow->setEnabled(false);

		  // Create dialog
		  QDialog* connectDialog = new QDialog(this, Qt::Dialog);
		  connectDialog->setMinimumSize(QSize(360,80));
		  connectDialog->setCaption(tr("fCal"));
		  connectDialog->setBackgroundColor(QColor(224, 224, 224));

		  QLabel* connectLabel = new QLabel(QString("Connecting to devices, please wait..."), connectDialog);
		  connectLabel->setFont(QFont("SansSerif", 16));

		  QHBoxLayout* layout = new QHBoxLayout();
		  layout->addWidget(connectLabel);

		  connectDialog->setLayout(layout);
		  connectDialog->show();

		  QApplication::processEvents();

		  // Connect to devices
		  if (m_ParentMainWindow->GetObjectVisualizer()->StartDataCollection() != PLUS_SUCCESS)
      {
			  LOG_ERROR("Unable to start collecting data!");
			  m_DeviceSetSelectorWidget->SetConnectionSuccessful(false);
			  m_ToolStateDisplayWidget->InitializeTools(NULL, false);
		  }
      else
      {
        // Read configuration
        if (ReadConfiguration(vtkPlusConfig::GetInstance()->GetDeviceSetConfigurationData()) != PLUS_SUCCESS)
        {
          LOG_ERROR("Failed to read fCal configuration");
        }

        // Successful connection
			  m_DeviceSetSelectorWidget->SetConnectionSuccessful(true);

        // Load device models based on the new configuration
        m_ParentMainWindow->GetObjectVisualizer()->ReadConfiguration(vtkPlusConfig::GetInstance()->GetDeviceSetConfigurationData());
        m_ParentMainWindow->GetObjectVisualizer()->InitializeObjectVisualization();

        vtkPlusConfig::GetInstance()->SaveApplicationConfigurationToFile();

			  if (m_ToolStateDisplayWidget->InitializeTools(m_ParentMainWindow->GetObjectVisualizer()->GetDataCollector(), true))
        {
				  ui.toolStateDisplayWidget->setMinimumHeight(m_ToolStateDisplayWidget->GetDesiredHeight());
				  ui.toolStateDisplayWidget->setMaximumHeight(m_ToolStateDisplayWidget->GetDesiredHeight());
			  }

        if (ReadAndAddPhantomWiresToVisualization() != PLUS_SUCCESS)
        {
          LOG_WARNING("Unable to initialize phantom wires visualization");
        }
      }

		  // Close dialog
		  connectDialog->done(0);
      connectDialog->hide();
      delete connectDialog;

      // Re-enable main window
     	m_ParentMainWindow->setEnabled(true);
    }
  }
  else // Disconnect
  {
		vtkDataCollector* dataCollector = m_ParentMainWindow->GetObjectVisualizer()->GetDataCollector();
		if ((dataCollector != NULL) && (dataCollector->GetConnected()))
    {
			dataCollector->Stop();
			dataCollector->Disconnect();

			m_DeviceSetSelectorWidget->SetConnectionSuccessful(false);
			m_ToolStateDisplayWidget->InitializeTools(NULL, false);

      m_ParentMainWindow->ResetShowDevices();
      m_ParentMainWindow->ResetAllToolboxes();

      m_ParentMainWindow->GetObjectVisualizer()->ClearDisplayableObjects();
		}
	}

  QApplication::restoreOverrideCursor();
}

//-----------------------------------------------------------------------------

void ConfigurationToolbox::PopOutToggled(bool aOn)
{
	LOG_TRACE("ConfigurationToolbox::PopOutToggled");

	if (aOn)
  {
		// Create pop out window
		m_ToolStatePopOutWindow = new QWidget(this, Qt::Tool);
		m_ToolStatePopOutWindow->setMinimumSize(QSize(180, m_ToolStateDisplayWidget->GetDesiredHeight()));
		m_ToolStatePopOutWindow->setMaximumSize(QSize(180, m_ToolStateDisplayWidget->GetDesiredHeight()));
		m_ToolStatePopOutWindow->setCaption(tr("Tool state display"));
		m_ToolStatePopOutWindow->setBackgroundColor(QColor::fromRgb(255, 255, 255));

		QGridLayout* gridToolStateDisplay = new QGridLayout(m_ToolStatePopOutWindow, 1, 1, 0, 4, "");
		gridToolStateDisplay->addWidget(m_ToolStateDisplayWidget);
		m_ToolStatePopOutWindow->setLayout(gridToolStateDisplay);
		m_ToolStatePopOutWindow->move( mapToGlobal( QPoint( ui.pushButton_PopOut->x() + ui.pushButton_PopOut->width(), ui.pushButton_PopOut->y() ) ) );
		m_ToolStatePopOutWindow->show();

		// Install event filter that is called on closing the window
		m_ToolStatePopOutWindow->installEventFilter(this);

		// Delete layout from the toolbox (to be able to add again)
		delete ui.toolStateDisplayWidget->layout();

		// Reduce size of empty space in toolbox
		ui.toolStateDisplayWidget->setMinimumHeight(0);
		ui.toolStateDisplayWidget->setMaximumHeight(0);
	}
  else
  {
		// Insert tool state display back in toolbox
		ui.toolStateDisplayWidget->setMinimumHeight(m_ToolStateDisplayWidget->GetDesiredHeight());
		ui.toolStateDisplayWidget->setMaximumHeight(m_ToolStateDisplayWidget->GetDesiredHeight());
		QGridLayout* gridToolStateDisplay = new QGridLayout(ui.toolStateDisplayWidget, 1, 1, 0, 4, "");
		gridToolStateDisplay->addWidget(m_ToolStateDisplayWidget);
		ui.toolStateDisplayWidget->setLayout(gridToolStateDisplay);

		// Delete pop out window
		if (m_ToolStatePopOutWindow)
    {
			delete m_ToolStatePopOutWindow;
		}
		m_ToolStatePopOutWindow = NULL;
	}

	// Set detached flag
	m_IsToolDisplayDetached = aOn;
}

//-----------------------------------------------------------------------------

bool ConfigurationToolbox::eventFilter(QObject *obj, QEvent *ev)
{
	if ( obj == m_ToolStatePopOutWindow )
  {
		if ( ev->type() == QEvent::Close )
    {
			ui.pushButton_PopOut->setChecked(false);
		}
    else
    {
			// Pass the event on to the parent class
			return QWidget::eventFilter( obj, ev );
		}
	}

	return true;
}

//-----------------------------------------------------------------------------

void ConfigurationToolbox::LogLevelChanged(int aLevel)
{
	LOG_TRACE("ConfigurationToolbox::LogLevelChanged");

  vtkPlusLogger::Instance()->SetLogLevel(aLevel+1);

	LOG_INFO("Log level changed to: " << ui.comboBox_LogLevel->currentText().ascii() << " (" << aLevel+1 << ")" );

  vtkPlusConfig::GetInstance()->SaveApplicationConfigurationToFile();
}

//-----------------------------------------------------------------------------

void ConfigurationToolbox::SelectEditorApplicationExecutable()
{
	LOG_TRACE("ConfigurationToolbox::SelectEditorApplicationExecutable");

	// File open dialog for selecting phantom definition xml
	QString filter = QString( tr( "Executables ( *.exe );;" ) );
  QString fileName = QFileDialog::getOpenFileName(NULL, QString( tr( "Select XML editor application" ) ), "C:", filter);
	if (fileName.isNull()) {
		return;
	}

  vtkPlusConfig::GetInstance()->SetEditorApplicationExecutable(fileName.toAscii().data());
  vtkPlusConfig::GetInstance()->SaveApplicationConfigurationToFile();

  ui.lineEdit_EditorApplicationExecutable->setText(fileName);
}

//-----------------------------------------------------------------------------

void ConfigurationToolbox::SelectImageDirectory()
{
  LOG_TRACE("ConfigurationToolbox::SelectImageDirectory"); 

	// Directory open dialog for selecting configuration directory 
  QString dirName = QFileDialog::getExistingDirectory(NULL, QString( tr( "Select image directory" ) ), QString(vtkPlusConfig::GetInstance()->GetDeviceSetConfigurationDirectory()));
	if (dirName.isNull()) {
		return;
	}

  // Save the selected directory to config object
  vtkPlusConfig::GetInstance()->SetImageDirectory(dirName.toAscii().data());
  vtkPlusConfig::GetInstance()->SaveApplicationConfigurationToFile();

  ui.lineEdit_ImageDirectory->setText(dirName);
}

//-----------------------------------------------------------------------------

PlusStatus ConfigurationToolbox::ReadConfiguration(vtkXMLDataElement* aConfig)
{
  LOG_TRACE("ConfigurationToolbox::ReadConfiguration");

  if (aConfig == NULL)
  {
    LOG_ERROR("Unable to read configuration"); 
    return PLUS_FAIL;
  }

  // Read tracker tool names
  vtkXMLDataElement* fCalElement = aConfig->FindNestedElementWithName("fCal"); 

  if (fCalElement == NULL)
  {
    LOG_ERROR("Unable to find fCal element in XML tree!"); 
    return PLUS_FAIL;
  }

  // Image coordinate frame
  const char* imageCoordinateFrame = fCalElement->GetAttribute("ImageCoordinateFrame");
  if (imageCoordinateFrame == NULL)
  {
	  LOG_ERROR("Image coordinate frame is not specified in the fCal section of the configuration!");
    return PLUS_FAIL;
  }

  m_ParentMainWindow->SetImageCoordinateFrame(imageCoordinateFrame);

  // Probe coordinate frame
  const char* probeCoordinateFrame = fCalElement->GetAttribute("ProbeCoordinateFrame");
  if (probeCoordinateFrame == NULL)
  {
	  LOG_ERROR("Probe coordinate frame is not specified in the fCal section of the configuration!");
    return PLUS_FAIL;
  }

  m_ParentMainWindow->SetProbeCoordinateFrame(probeCoordinateFrame);

  // Reference coordinate frame
  const char* referenceCoordinateFrame = fCalElement->GetAttribute("ReferenceCoordinateFrame");
  if (referenceCoordinateFrame == NULL)
  {
	  LOG_ERROR("Reference coordinate frame not specified in the fCal section of the configuration!");
    return PLUS_FAIL;
  }

  m_ParentMainWindow->SetReferenceCoordinateFrame(referenceCoordinateFrame);

  // Transducer origin coordinate frame
  const char* transducerOriginCoordinateFrame = fCalElement->GetAttribute("TransducerOriginCoordinateFrame");
  if (transducerOriginCoordinateFrame == NULL)
  {
	  LOG_ERROR("Transducer origin coordinate frame not specified in the fCal section of the configuration!");
    return PLUS_FAIL;
  }

  m_ParentMainWindow->SetTransducerOriginCoordinateFrame(transducerOriginCoordinateFrame);

  // Transducer origin pixel coordinate frame
  const char* transducerOriginPixelCoordinateFrame = fCalElement->GetAttribute("TransducerOriginPixelCoordinateFrame");
  if (transducerOriginPixelCoordinateFrame == NULL)
  {
	  LOG_ERROR("Transducer origin pixel coordinate frame not specified in the fCal section of the configuration!");
    return PLUS_FAIL;
  }

  m_ParentMainWindow->SetTransducerOriginPixelCoordinateFrame(transducerOriginPixelCoordinateFrame);

  return PLUS_SUCCESS;
}

//-----------------------------------------------------------------------------

PlusStatus ConfigurationToolbox::ReadAndAddPhantomWiresToVisualization()
{
	LOG_TRACE("ConfigurationToolbox::ReadAndAddPhantomWiresToVisualization"); 

  // Get phantom coordinate frame name
  vtkXMLDataElement* phantomRegistrationElement = vtkPlusConfig::GetInstance()->GetDeviceSetConfigurationData()->FindNestedElementWithName( vtkPhantomRegistrationAlgo::GetConfigurationElementName().c_str() ); 
  if (phantomRegistrationElement == NULL)
  {
    LOG_INFO("No phantom registration algorithm configuration are found - no phantom will be shown");
    return PLUS_SUCCESS;
  }

  vtkSmartPointer<vtkPhantomRegistrationAlgo> phantomRegistration = vtkSmartPointer<vtkPhantomRegistrationAlgo>::New();

  if (phantomRegistration->ReadConfiguration(vtkPlusConfig::GetInstance()->GetDeviceSetConfigurationData()) != PLUS_SUCCESS)
  {
    LOG_WARNING("Reading phantom registration algorithm configuration failed!");
    return PLUS_FAIL;
  }

  // Get phantom displayable model object
  vtkDisplayableObject* phantomDisplayableObject = NULL;
  if (m_ParentMainWindow->GetObjectVisualizer()->GetDisplayableObject(phantomRegistration->GetPhantomCoordinateFrame(), phantomDisplayableObject) != PLUS_SUCCESS)
  {
    LOG_WARNING("Unable to find phantom displayable object '" << phantomRegistration->GetPhantomCoordinateFrame() << "' - phantom will not be shown");
    return PLUS_FAIL;
  }

  vtkDisplayableModel* phantomDisplayableModel = dynamic_cast<vtkDisplayableModel*>(phantomDisplayableObject);
  if (phantomDisplayableModel == NULL)
  {
    LOG_ERROR("Phantom displayable object is invalid!");
    return PLUS_FAIL;
  }

  // Get wire pattern
  FidPatternRecognition patternRecognition;
  if (patternRecognition.ReadConfiguration(vtkPlusConfig::GetInstance()->GetDeviceSetConfigurationData()) != PLUS_SUCCESS)
  {
    LOG_ERROR("Unable to read phantom wire configuration!");
    return PLUS_FAIL;
  }

  std::vector<Pattern*> patterns( patternRecognition.GetFidLineFinder()->GetPatterns() );

  vtkSmartPointer<vtkMatrix4x4> phantomModelToPhantomTransformMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
  phantomModelToPhantomTransformMatrix->DeepCopy(phantomDisplayableModel->GetModelToObjectTransform()->GetMatrix());
  vtkSmartPointer<vtkMatrix4x4> phantomToPhantomModelTransformMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
  vtkMatrix4x4::Invert(phantomModelToPhantomTransformMatrix, phantomToPhantomModelTransformMatrix);

  // Construct wires poly data
  for (std::vector<Pattern*>::iterator patternIt = patterns.begin(); patternIt != patterns.end(); ++patternIt)
  {
    for (std::vector<Wire>::iterator wireIt = (*patternIt)->Wires.begin(); wireIt != (*patternIt)->Wires.end(); ++wireIt)
    {
      double endPointFrontInPhantomFrame[4] = { wireIt->EndPointFront[0], wireIt->EndPointFront[1], wireIt->EndPointFront[2], 1.0 };
      double endPointBackInPhantomFrame[4] = { wireIt->EndPointBack[0], wireIt->EndPointBack[1], wireIt->EndPointBack[2], 1.0 };
      double endPointFrontInPhantomModelFrame[4];
      double endPointBackInPhantomModelFrame[4];
      phantomToPhantomModelTransformMatrix->MultiplyPoint(endPointFrontInPhantomFrame, endPointFrontInPhantomModelFrame);
      phantomToPhantomModelTransformMatrix->MultiplyPoint(endPointBackInPhantomFrame, endPointBackInPhantomModelFrame);

      vtkSmartPointer<vtkLineSource> wireLineSource = vtkSmartPointer<vtkLineSource>::New();
      wireLineSource->SetPoint1(endPointFrontInPhantomModelFrame);
      wireLineSource->SetPoint2(endPointBackInPhantomModelFrame);

      phantomDisplayableModel->AppendPolyData(wireLineSource->GetOutput());
    }
  }

  return PLUS_SUCCESS;
}
