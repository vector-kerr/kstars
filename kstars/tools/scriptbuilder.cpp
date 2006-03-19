/***************************************************************************
                          scriptbuilder.cpp  -  description
                             -------------------
    begin                : Thu Apr 17 2003
    copyright            : (C) 2003 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

//needed in slotSave() for chmod() syscall
#include <sys/stat.h>

#include <QTreeWidget>

#include <kdebug.h>
#include <klocale.h>
#include <kio/netaccess.h>
#include <kprocess.h>
#include <kstdguiitem.h>
#include <kstandarddirs.h>
#include <kurl.h>
#include <kiconloader.h>
#include <ktempfile.h>
#include <kmessagebox.h>

//#include <qcheckbox.h>
//#include <qspinbox.h>
//#include <q3widgetstack.h>
//#include <qwidget.h>
//#include <q3ptrlist.h>
//#include <qlayout.h>
//#include <q3datetimeedit.h>
//#include <qradiobutton.h>
//#include <q3buttongroup.h>

//#include <kpushbutton.h>
//#include <kcolorbutton.h>
//#include <kcombobox.h>
//#include <kicontheme.h>
//#include <klistbox.h>
//#include <k3listview.h>
//#include <ktextedit.h>
//#include <kdatewidget.h>
#include <kfiledialog.h>
//#include <kurlrequester.h>
//#include <knuminput.h>

#include "scriptbuilder.h"
#include "scriptfunction.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "kstarsdatetime.h"
#include "finddialog.h"
#include "locationdialog.h"
#include "widgets/dmsbox.h"
#include "widgets/timestepbox.h"
#include "libkdeedu/extdate/extdatewidget.h"

OptionsTreeViewWidget::OptionsTreeViewWidget( QWidget *p ) : QFrame( p ) {
  setupUi( this );
}

OptionsTreeView::OptionsTreeView( QWidget *p ) 
 : KDialog( p, i18n( "Options" ), KDialog::Ok|KDialog::Cancel ) {
	otvw = new OptionsTreeViewWidget( this );
	setMainWidget( otvw );
}

OptionsTreeView::~OptionsTreeView() {
  delete otvw;
}

ScriptNameWidget::ScriptNameWidget( QWidget *p ) : QFrame( p ) {
  setupUi( this );
}

ScriptNameDialog::ScriptNameDialog( QWidget *p ) 
 : KDialog( p, i18n( "Script Data" ), KDialog::Ok|KDialog::Cancel ) {
	snw = new ScriptNameWidget( this );
	setMainWidget( snw );
	connect( snw->ScriptName, SIGNAL( textChanged(const QString &) ), this, SLOT( slotEnableOkButton() ) );
}

ScriptNameDialog::~ScriptNameDialog() {
	delete snw;
}

void ScriptNameDialog::slotEnableOkButton() {
	actionButton( Ok )->setEnabled( ! snw->ScriptName->text().isEmpty() );
}

ScriptBuilderUI::ScriptBuilderUI( QWidget *p ) : QFrame( p ) {
	setupUi( this );
}

ScriptBuilder::ScriptBuilder( QWidget *parent )
	: KDialog( parent, i18n( "Script Builder" ), KDialog::Close ), 
		UnsavedChanges(false), currentFileURL(), currentDir( QDir::homePath() ), 
		currentScriptName(), currentAuthor() 
{
	ks = (KStars*)parent;
	sb = new ScriptBuilderUI(this);
	setMainWidget(sb);

	//Initialize function templates and descriptions
	KStarsFunctionList.append( new ScriptFunction( "lookTowards", i18n( "Point the display at the specified location. %1 can be the name of an object, a cardinal point on the compass, or 'zenith'." ),
			false, "QString", "dir" ) );
	KStarsFunctionList.append( new ScriptFunction( "setRaDec", i18n( "Point the display at the specified RA/Dec coordinates.  %1 is expressed in Hours; %2 is expressed in Degrees." ),
			false, "double", "ra", "double", "dec" ) );
	KStarsFunctionList.append( new ScriptFunction( "setAltAz", i18n( "Point the display at the specified Alt/Az coordinates.  %1 and %2 are expressed in Degrees." ),
			false, "double", "alt", "double", "az" ) );
	KStarsFunctionList.append( new ScriptFunction( "zoomIn", i18n( "Increase the display Zoom Level." ), false ) );
	KStarsFunctionList.append( new ScriptFunction( "zoomOut", i18n( "Decrease the display Zoom Level." ), false ) );
	KStarsFunctionList.append( new ScriptFunction( "defaultZoom", i18n( "Set the display Zoom Level to its default value." ), false ) );
	KStarsFunctionList.append( new ScriptFunction( "zoom", i18n( "Set the display Zoom Level manually." ), false, "double", "z" ) );
	KStarsFunctionList.append( new ScriptFunction( "setLocalTime", i18n( "Set the system clock to the specified Local Time." ),
			false, "int", "year", "int", "month", "int", "day", "int", "hour", "int", "minute", "int", "second" ) );
	KStarsFunctionList.append( new ScriptFunction( "waitFor", i18n( "Pause script execution for %1 seconds." ), false, "double", "sec" ) );
	KStarsFunctionList.append( new ScriptFunction( "waitForKey", i18n( "Halt script execution until the key %1 is pressed.  Only single-key strokes are possible; use 'space' for the spacebar." ),
			false, "QString", "key" ) );
	KStarsFunctionList.append( new ScriptFunction( "setTracking", i18n( "Set whether the display is tracking the current location." ), false, "bool", "track" ) );
	KStarsFunctionList.append( new ScriptFunction( "changeViewOption", i18n( "Change view option named %1 to value %2." ), false, "QString", "opName", "QString", "opValue" ) );
	KStarsFunctionList.append( new ScriptFunction( "setGeoLocation", i18n( "Set the geographic location to the city specified by %1, %2 and %3." ),
			false, "QString", "cityName", "QString", "provinceName", "QString", "countryName" ) );
	KStarsFunctionList.append( new ScriptFunction( "setColor", i18n( "Set the color named %1 to the value %2." ), false, "QString", "colorName", "QString", "value" ) );
	KStarsFunctionList.append( new ScriptFunction( "loadColorScheme", i18n( "Load the color scheme named %1." ), false, "QString", "name" ) );
	KStarsFunctionList.append( new ScriptFunction( "exportImage", i18n( "Export the sky image to the file %1, with width %2 and height %3." ), false, "QString", "fileName", "int", "width", "int", "height" ) );
	KStarsFunctionList.append( new ScriptFunction( "printImage", i18n( "Print the sky image to a printer or file.  If %1 is true, it will show the print dialog.  If %2 is true, it will use the Star Chart color scheme for printing." ), false, "bool", "usePrintDialog", "bool", "useChartColors" ) );
	KStarsFunctionList.append( new ScriptFunction( "stop", i18n( "Halt the simulation clock." ), true ) );
	KStarsFunctionList.append( new ScriptFunction( "start", i18n( "Start the simulation clock." ), true ) );
	KStarsFunctionList.append( new ScriptFunction( "setClockScale", i18n( "Set the timescale of the simulation clock to %1.  1.0 means real-time; 2.0 means twice real-time; etc." ), true, "double", "scale" ) );
	
	// INDI fuctions
	ScriptFunction *startINDIFunc(NULL), *shutdownINDIFunc(NULL), *switchINDIFunc(NULL), *setINDIPortFunc(NULL), *setINDIScopeActionFunc(NULL), *setINDITargetCoordFunc(NULL), *setINDITargetNameFunc(NULL), *setINDIGeoLocationFunc(NULL), *setINDIUTCFunc(NULL), *setINDIActionFunc(NULL), *waitForINDIActionFunc(NULL), *setINDIFocusSpeedFunc(NULL), *startINDIFocusFunc(NULL), *setINDIFocusTimeoutFunc(NULL), *setINDICCDTempFunc(NULL), *setINDIFilterNumFunc(NULL), *setINDIFrameTypeFunc(NULL), *startINDIExposureFunc(NULL), *setINDIDeviceFunc(NULL); 
	
	startINDIFunc = new ScriptFunction( "startINDI", i18n("Establish an INDI device either in local mode or server mode."), false, "QString", "deviceName", "bool", "useLocal");
	INDIFunctionList.append ( startINDIFunc );
	
        setINDIDeviceFunc = new ScriptFunction( "setINDIDevice", i18n("Change current active device. All subsequent function calls will communicate with this device until changed"), false, "QString", "deviceName");
	INDIFunctionList.append(setINDIDeviceFunc);

	shutdownINDIFunc = new ScriptFunction( "shutdownINDI", i18n("Shutdown an INDI device."), false, "QString", "deviceName");
	INDIFunctionList.append ( shutdownINDIFunc);
	
	switchINDIFunc = new ScriptFunction( "switchINDI", i18n("Connect or Disconnect an INDI device."), false, "bool", "turnOn");
	switchINDIFunc->setINDIProperty("CONNECTION");
	switchINDIFunc->setArg(0, "true");
	INDIFunctionList.append ( switchINDIFunc);
	
	setINDIPortFunc = new ScriptFunction( "setINDIPort", i18n("Set INDI's device connection port."), false, "QString", "port");
	setINDIPortFunc->setINDIProperty("DEVICE_PORT");
	INDIFunctionList.append ( setINDIPortFunc);
	
	setINDIScopeActionFunc = new ScriptFunction( "setINDIScopeAction", i18n("Set the telescope action. Available actions are SLEW, TRACK, SYNC, PARK, and ABORT."), false, "QString", "action");
	setINDIScopeActionFunc->setINDIProperty("CHECK");
	setINDIScopeActionFunc->setArg(0, "SLEW");
	INDIFunctionList.append( setINDIScopeActionFunc);
	
	setINDITargetCoordFunc = new ScriptFunction ( "setINDITargetCoord", i18n( "Set the telescope target coordinates to the RA/Dec coordinates.  RA is expressed in Hours; DEC is expressed in Degrees." ), false, "double", "RA", "double", "DEC" );
	setINDITargetCoordFunc->setINDIProperty("EQUATORIAL_EOD_COORD");
	INDIFunctionList.append ( setINDITargetCoordFunc );
	
	setINDITargetNameFunc = new ScriptFunction( "setINDITargetName", i18n("Set the telescope target coorinates to the RA/Dec coordinates of the selected object."), false, "QString", "targetName");
	setINDITargetNameFunc->setINDIProperty("EQUATORIAL_EOD_COORD");
	INDIFunctionList.append( setINDITargetNameFunc);
	
	setINDIGeoLocationFunc = new ScriptFunction ( "setINDIGeoLocation", i18n("Set the telescope longitude and latitude. The longitude is E of N."), false, "double", "long", "double", "lat");
	setINDIGeoLocationFunc->setINDIProperty("GEOGRAPHIC_COORD");
	INDIFunctionList.append( setINDIGeoLocationFunc);
	
	setINDIUTCFunc = new ScriptFunction ( "setINDIUTC", i18n("Set the device UTC time in ISO 8601 format YYYY/MM/DDTHH:MM:SS."), false, "QString", "UTCDateTime");
	setINDIUTCFunc->setINDIProperty("TIME");
	INDIFunctionList.append( setINDIUTCFunc);
	
	setINDIActionFunc = new ScriptFunction( "setINDIAction", i18n("Activate an INDI action. The action is the name of any INDI switch property element supported by the device."), false, "QString", "actionName");
	INDIFunctionList.append( setINDIActionFunc);
	
	waitForINDIActionFunc = new ScriptFunction ("waitForINDIAction", i18n("Pause script execution until action returns with OK status. The action can be the name of any INDI property supported by the device."), false, "QString", "actionName");
	INDIFunctionList.append( waitForINDIActionFunc );
	
	setINDIFocusSpeedFunc = new ScriptFunction ("setINDIFocusSpeed", i18n("Set the telescope focuser speed. Set speed to 0 to halt the focuser. 1-3 correspond to slow, medium, and fast speeds respectively."), false, "unsigned int", "speed");
	setINDIFocusSpeedFunc->setINDIProperty("FOCUS_SPEED");
	INDIFunctionList.append( setINDIFocusSpeedFunc );
	
	startINDIFocusFunc = new ScriptFunction ("startINDIFocus", i18n("Start moving the focuser in the direction Dir, and for the duration specified by setINDIFocusTimeout."), false, "QString", "Dir");
	startINDIFocusFunc->setINDIProperty("FOCUS_MOTION");
	startINDIFocusFunc->setArg(0, "IN");
	INDIFunctionList.append( startINDIFocusFunc);
	
	setINDIFocusTimeoutFunc = new ScriptFunction ( "setINDIFocusTimeout", i18n("Set the telescope focuser timer in seconds. This is the duration of any focusing procedure performed by calling startINDIFocus."), false, "int", "timeout");
	setINDIFocusTimeoutFunc->setINDIProperty("FOCUS_TIMER");
	INDIFunctionList.append( setINDIFocusTimeoutFunc);
	
	setINDICCDTempFunc = new ScriptFunction( "setINDICCDTemp", i18n("Set the target CCD chip temperature."), false, "int", "temp");
	setINDICCDTempFunc->setINDIProperty("CCD_TEMPERATURE");
	INDIFunctionList.append( setINDICCDTempFunc);

        setINDIFilterNumFunc = new ScriptFunction( "setINDIFilterNum", i18n("Set the target filter position."), false, "int", "filter_num");
	setINDIFilterNumFunc->setINDIProperty("FILTER_SLOT");
	INDIFunctionList.append ( setINDIFilterNumFunc);
	
	setINDIFrameTypeFunc = new ScriptFunction( "setINDIFrameType", i18n("Set the CCD camera frame type. Available options are FRAME_LIGHT, FRAME_BIAS, FRAME_DARK, and FRAME_FLAT."), false, "QString", "type");
	setINDIFrameTypeFunc->setINDIProperty("FRAME_TYPE");
	setINDIFrameTypeFunc->setArg(0, "FRAME_LIGHT");
	INDIFunctionList.append( setINDIFrameTypeFunc);
	
	startINDIExposureFunc = new ScriptFunction ( "startINDIExposure", i18n("Start Camera/CCD exposure. The duration is in seconds."), false, "int", "timeout");
	startINDIExposureFunc->setINDIProperty("CCD_EXPOSE_DURATION");
	INDIFunctionList.append( startINDIExposureFunc);
	
	// JM: We're using QTreeWdiget for Qt4 now
	QTreeWidgetItem *kstars_tree = new QTreeWidgetItem( sb->FunctionTree, QStringList("KStars"));
	
	for ( int i=KStarsFunctionList.size()-1; i>=0; i-- ) 
	  new QTreeWidgetItem (kstars_tree, QStringList( KStarsFunctionList[i]->prototype()) );

	
	sb->FunctionTree->setColumnCount(1);
	sb->FunctionTree->setHeaderLabels( QStringList(i18n("Functions")) );
	sb->FunctionTree->setSortingEnabled( false );

	QTreeWidgetItem *INDI_tree = new QTreeWidgetItem( sb->FunctionTree, QStringList("INDI"));	
        QTreeWidgetItem *INDI_general = new QTreeWidgetItem( INDI_tree, QStringList("General"));
	QTreeWidgetItem *INDI_telescope = new QTreeWidgetItem( INDI_tree, QStringList("Telescope"));
	QTreeWidgetItem *INDI_ccd = new QTreeWidgetItem( INDI_tree, QStringList("Camera/CCD"));
	QTreeWidgetItem *INDI_focuser = new QTreeWidgetItem( INDI_tree, QStringList("Focuser"));
	QTreeWidgetItem *INDI_filter = new QTreeWidgetItem( INDI_tree, QStringList("Filter"));

	// General
	new QTreeWidgetItem(INDI_general, QStringList(startINDIFunc->prototype()));
	new QTreeWidgetItem(INDI_general, QStringList(shutdownINDIFunc->prototype()));
	new QTreeWidgetItem(INDI_general, QStringList(setINDIDeviceFunc->prototype()));
	new QTreeWidgetItem(INDI_general, QStringList(switchINDIFunc->prototype()));
	new QTreeWidgetItem(INDI_general, QStringList(setINDIPortFunc->prototype()));
	new QTreeWidgetItem(INDI_general, QStringList(setINDIActionFunc->prototype()));
	new QTreeWidgetItem(INDI_general, QStringList(waitForINDIActionFunc->prototype()));
	
	// Telescope
	new QTreeWidgetItem(INDI_telescope, QStringList(setINDIScopeActionFunc->prototype()));
	new QTreeWidgetItem(INDI_telescope, QStringList(setINDITargetCoordFunc->prototype()));
	new QTreeWidgetItem(INDI_telescope, QStringList(setINDITargetNameFunc->prototype()));
	new QTreeWidgetItem(INDI_telescope, QStringList(setINDIGeoLocationFunc->prototype()));
	new QTreeWidgetItem(INDI_telescope, QStringList(setINDIUTCFunc->prototype()));
	
	// CCD
	new QTreeWidgetItem(INDI_ccd, QStringList(setINDICCDTempFunc->prototype()));
	new QTreeWidgetItem(INDI_ccd, QStringList(setINDIFrameTypeFunc->prototype()));
	new QTreeWidgetItem(INDI_ccd, QStringList(startINDIExposureFunc->prototype()));
	
	// Focuser
	new QTreeWidgetItem(INDI_focuser, QStringList(setINDIFocusSpeedFunc->prototype()));
	new QTreeWidgetItem(INDI_focuser, QStringList(setINDIFocusTimeoutFunc->prototype()));
	new QTreeWidgetItem(INDI_focuser, QStringList(startINDIFocusFunc->prototype()));
	
	// Filter
	new QTreeWidgetItem(INDI_filter, QStringList(setINDIFilterNumFunc->prototype()));

	//Add icons to Push Buttons
	KIconLoader *icons = KGlobal::iconLoader();
	sb->NewButton->setIconSet( icons->loadIcon( "filenew", KIcon::Toolbar ) );
	sb->OpenButton->setIconSet( icons->loadIcon( "fileopen", KIcon::Toolbar ) );
	sb->SaveButton->setIconSet( icons->loadIconSet( "filesave", KIcon::Toolbar ) );
	sb->SaveAsButton->setIconSet( icons->loadIconSet( "filesaveas", KIcon::Toolbar ) );
	sb->RunButton->setIconSet( icons->loadIconSet( "launch", KIcon::Toolbar ) );
	sb->CopyButton->setIconSet( icons->loadIconSet( "reload", KIcon::Toolbar ) );
	sb->AddButton->setIconSet( icons->loadIconSet( "back", KIcon::Toolbar ) );
	sb->RemoveButton->setIconSet( icons->loadIconSet( "forward", KIcon::Toolbar ) );
	sb->UpButton->setIconSet( icons->loadIconSet( "up", KIcon::Toolbar ) );
	sb->DownButton->setIconSet( icons->loadIconSet( "down", KIcon::Toolbar ) );

	//Prepare the widget stack
	argBlank = new QWidget();
	argLookToward = new ArgLookToward( sb->ArgStack );
	argSetRaDec = new ArgSetRaDec( sb->ArgStack );
	argSetAltAz = new ArgSetAltAz( sb->ArgStack );
	argSetLocalTime = new ArgSetLocalTime( sb->ArgStack );
	argWaitFor = new ArgWaitFor( sb->ArgStack );
	argWaitForKey = new ArgWaitForKey( sb->ArgStack );
	argSetTracking = new ArgSetTrack( sb->ArgStack );
	argChangeViewOption = new ArgChangeViewOption( sb->ArgStack );
	argSetGeoLocation = new ArgSetGeoLocation( sb->ArgStack );
	argTimeScale = new ArgTimeScale( sb->ArgStack );
	argZoom = new ArgZoom( sb->ArgStack );
	argExportImage = new ArgExportImage( sb->ArgStack );
	argPrintImage = new ArgPrintImage( sb->ArgStack );
	argSetColor = new ArgSetColor( sb->ArgStack );
	argLoadColorScheme = new ArgLoadColorScheme( sb->ArgStack );
	argStartINDI = new ArgStartINDI ( sb->ArgStack );
	argSetDeviceINDI = new ArgSetDeviceINDI (sb->ArgStack);
	argShutdownINDI = new ArgShutdownINDI ( sb->ArgStack );
	argSwitchINDI   = new ArgSwitchINDI ( sb->ArgStack );
	argSetPortINDI  = new ArgSetPortINDI ( sb->ArgStack );
	argSetTargetCoordINDI = new ArgSetTargetCoordINDI ( sb->ArgStack );
	argSetTargetNameINDI  = new ArgSetTargetNameINDI ( sb->ArgStack );
	argSetActionINDI      = new ArgSetActionINDI ( sb->ArgStack );
	argWaitForActionINDI  = new ArgSetActionINDI ( sb->ArgStack );
	argSetFocusSpeedINDI  = new ArgSetFocusSpeedINDI ( sb->ArgStack );
	argStartFocusINDI     = new ArgStartFocusINDI( sb->ArgStack );
	argSetFocusTimeoutINDI = new ArgSetFocusTimeoutINDI( sb->ArgStack );
	argSetGeoLocationINDI  = new ArgSetGeoLocationINDI( sb->ArgStack );
	argStartExposureINDI   = new ArgStartExposureINDI( sb->ArgStack );
	argSetUTCINDI          = new ArgSetUTCINDI( sb->ArgStack );
	argSetScopeActionINDI  = new ArgSetScopeActionINDI( sb->ArgStack );
	argSetFrameTypeINDI    = new ArgSetFrameTypeINDI ( sb->ArgStack );
	argSetCCDTempINDI      = new ArgSetCCDTempINDI( sb->ArgStack );
	argSetFilterNumINDI    = new ArgSetFilterNumINDI( sb->ArgStack );

	argStartFocusINDI->directionCombo->insertItem("IN");
	argStartFocusINDI->directionCombo->insertItem("OUT");
	
	argSetScopeActionINDI->actionCombo->insertItem("SLEW");
	argSetScopeActionINDI->actionCombo->insertItem("TRACK");
	argSetScopeActionINDI->actionCombo->insertItem("SYNC");
	argSetScopeActionINDI->actionCombo->insertItem("PARK");
	argSetScopeActionINDI->actionCombo->insertItem("ABORT");
	
	argSetFrameTypeINDI->typeCombo->insertItem("FRAME_LIGHT");
	argSetFrameTypeINDI->typeCombo->insertItem("FRAME_BIAS");
	argSetFrameTypeINDI->typeCombo->insertItem("FRAME_DARK");
	argSetFrameTypeINDI->typeCombo->insertItem("FRAME_FLAT");
	
	sb->ArgStack->addWidget( argBlank );
	sb->ArgStack->addWidget( argLookToward );
	sb->ArgStack->addWidget( argSetRaDec );
	sb->ArgStack->addWidget( argSetAltAz );
	sb->ArgStack->addWidget( argSetLocalTime );
	sb->ArgStack->addWidget( argWaitFor );
	sb->ArgStack->addWidget( argWaitForKey );
	sb->ArgStack->addWidget( argSetTracking );
	sb->ArgStack->addWidget( argChangeViewOption );
	sb->ArgStack->addWidget( argSetGeoLocation );
	sb->ArgStack->addWidget( argTimeScale );
	sb->ArgStack->addWidget( argZoom );
	sb->ArgStack->addWidget( argExportImage );
	sb->ArgStack->addWidget( argPrintImage );
	sb->ArgStack->addWidget( argSetColor );
	sb->ArgStack->addWidget( argLoadColorScheme );
	
	sb->ArgStack->addWidget( argStartINDI);
	sb->ArgStack->addWidget( argSetDeviceINDI);
	sb->ArgStack->addWidget( argShutdownINDI);
	sb->ArgStack->addWidget( argSwitchINDI);
	sb->ArgStack->addWidget( argSetPortINDI);
	sb->ArgStack->addWidget( argSetTargetCoordINDI);
	sb->ArgStack->addWidget( argSetTargetNameINDI);
	sb->ArgStack->addWidget( argSetActionINDI);
	sb->ArgStack->addWidget( argWaitForActionINDI );
	sb->ArgStack->addWidget( argSetFocusSpeedINDI );
	sb->ArgStack->addWidget( argStartFocusINDI);
	sb->ArgStack->addWidget( argSetFocusTimeoutINDI);
	sb->ArgStack->addWidget( argSetGeoLocationINDI);
	sb->ArgStack->addWidget( argStartExposureINDI);
	sb->ArgStack->addWidget( argSetUTCINDI);
	sb->ArgStack->addWidget( argSetScopeActionINDI);
	sb->ArgStack->addWidget( argSetFrameTypeINDI);
	sb->ArgStack->addWidget( argSetCCDTempINDI);
	sb->ArgStack->addWidget( argSetFilterNumINDI);
	
	sb->ArgStack->setCurrentIndex( 0 );

	snd = new ScriptNameDialog( ks );
	otv = new OptionsTreeView( ks );

	initViewOptions();

	//connect widgets in ScriptBuilderUI
        connect( this, SIGNAL(rejected()), this, SLOT(slotClose()));
	connect( sb->FunctionTree, SIGNAL( itemDoubleClicked(QTreeWidgetItem *, int )), this, SLOT( slotAddFunction() ) );
	connect( sb->FunctionTree, SIGNAL( itemClicked(QTreeWidgetItem*, int) ), this, SLOT( slotShowDoc() ) );
	connect( sb->UpButton, SIGNAL( clicked() ), this, SLOT( slotMoveFunctionUp() ) );
	connect( sb->ScriptListBox, SIGNAL( itemClicked(QListWidgetItem *) ), this, SLOT( slotArgWidget() ) );
	connect( sb->DownButton, SIGNAL( clicked() ), this, SLOT( slotMoveFunctionDown() ) );
	connect( sb->CopyButton, SIGNAL( clicked() ), this, SLOT( slotCopyFunction() ) );
	connect( sb->RemoveButton, SIGNAL( clicked() ), this, SLOT( slotRemoveFunction() ) );
	connect( sb->NewButton, SIGNAL( clicked() ), this, SLOT( slotNew() ) );
	connect( sb->OpenButton, SIGNAL( clicked() ), this, SLOT( slotOpen() ) );
	connect( sb->SaveButton, SIGNAL( clicked() ), this, SLOT( slotSave() ) );
	connect( sb->SaveAsButton, SIGNAL( clicked() ), this, SLOT( slotSaveAs() ) );
	connect( sb->AddButton, SIGNAL( clicked() ), this, SLOT( slotAddFunction() ) );
	connect( sb->RunButton, SIGNAL( clicked() ), this, SLOT( slotRunScript() ) );
	connect( sb->closeButton, SIGNAL(clicked()), this, SLOT(slotClose()));

	//Connections for Arg Widgets
	connect( argSetGeoLocation->FindCityButton, SIGNAL( clicked() ), this, SLOT( slotFindCity() ) );
	connect( argLookToward->FindButton, SIGNAL( clicked() ), this, SLOT( slotFindObject() ) );
	connect( argChangeViewOption->TreeButton, SIGNAL( clicked() ), this, SLOT( slotShowOptions() ) );

	connect( argLookToward->FocusEdit, SIGNAL( textChanged(const QString &) ), this, SLOT( slotLookToward() ) );
	connect( argSetRaDec->RABox, SIGNAL( textChanged(const QString &) ), this, SLOT( slotRa() ) );
	connect( argSetRaDec->DecBox, SIGNAL( textChanged(const QString &) ), this, SLOT( slotDec() ) );
	connect( argSetAltAz->AltBox, SIGNAL( textChanged(const QString &) ), this, SLOT( slotAlt() ) );
	connect( argSetAltAz->AzBox, SIGNAL( textChanged(const QString &) ), this, SLOT( slotAz() ) );
	connect( argSetLocalTime->DateBox, SIGNAL( changed(ExtDate) ), this, SLOT( slotChangeDate() ) );
	connect( argSetLocalTime->TimeBox, SIGNAL( valueChanged(const QTime&) ), this, SLOT( slotChangeTime() ) );
	connect( argWaitFor->DelayBox, SIGNAL( valueChanged(int) ), this, SLOT( slotWaitFor() ) );
	connect( argWaitForKey->WaitKeyEdit, SIGNAL( textChanged(const QString &) ), this, SLOT( slotWaitForKey() ) );
	connect( argSetTracking->CheckTrack, SIGNAL( stateChanged(int) ), this, SLOT( slotTracking() ) );
	connect( argChangeViewOption->OptionName, SIGNAL( activated(const QString &) ), this, SLOT( slotViewOption() ) );
	connect( argChangeViewOption->OptionValue, SIGNAL( textChanged(const QString &) ), this, SLOT( slotViewOption() ) );
	connect( argSetGeoLocation->CityName, SIGNAL( textChanged(const QString &) ), this, SLOT( slotChangeCity() ) );
	connect( argSetGeoLocation->ProvinceName, SIGNAL( textChanged(const QString &) ), this, SLOT( slotChangeProvince() ) );
	connect( argSetGeoLocation->CountryName, SIGNAL( textChanged(const QString &) ), this, SLOT( slotChangeCountry() ) );
	connect( argTimeScale->TimeScale, SIGNAL( scaleChanged(float) ), this, SLOT( slotTimeScale() ) );
	connect( argZoom->ZoomBox, SIGNAL( textChanged(const QString &) ), this, SLOT( slotZoom() ) );
	connect( argExportImage->ExportFileName, SIGNAL( textChanged(const QString &) ), this, SLOT( slotExportImage() ) );
	connect( argExportImage->ExportWidth, SIGNAL( valueChanged(int) ), this, SLOT( slotExportImage() ) );
	connect( argExportImage->ExportHeight, SIGNAL( valueChanged(int) ), this, SLOT( slotExportImage() ) );
	connect( argPrintImage->UsePrintDialog, SIGNAL( toggled(bool) ), this, SLOT( slotPrintImage() ) );
	connect( argPrintImage->UseChartColors, SIGNAL( toggled(bool) ), this, SLOT( slotPrintImage() ) );
	connect( argSetColor->ColorName, SIGNAL( activated(const QString &) ), this, SLOT( slotChangeColorName() ) );
	connect( argSetColor->ColorValue, SIGNAL( changed(const QColor &) ), this, SLOT( slotChangeColor() ) );
	connect( argLoadColorScheme->SchemeList, SIGNAL( clicked( Q3ListBoxItem* ) ), this, SLOT( slotLoadColorScheme() ) );
	
	connect( sb->AppendINDIWait, SIGNAL ( toggled(bool) ), this, SLOT(slotINDIWaitCheck(bool)));
	
	// Connections for INDI's Arg widgets
	
	// INDI Start Device
	connect (argStartINDI->deviceName, SIGNAL( textChanged(const QString &) ), this, SLOT(slotINDIStartDeviceName()));
	connect (argStartINDI->LocalButton, SIGNAL ( toggled(bool)), this, SLOT (slotINDIStartDeviceMode())); 
	
        // Set Device Name
	connect (argSetDeviceINDI->deviceName, SIGNAL( textChanged(const QString &) ), this, SLOT(slotINDISetDevice()));

	// INDI Shutdown Device
	connect (argShutdownINDI->deviceName, SIGNAL( textChanged(const QString &) ), this, SLOT(slotINDIShutdown()));
	
	// INDI Swtich Device
	connect (argSwitchINDI->OnButton, SIGNAL ( toggled( bool)), this, SLOT (slotINDISwitchDeviceConnection())); 
	
	// INDI Set Device Port
	connect (argSetPortINDI->devicePort, SIGNAL( textChanged(const QString &) ), this, SLOT(slotINDISetPortDevicePort()));
	
	// INDI Set Target Coord 
	connect( argSetTargetCoordINDI->RABox, SIGNAL( textChanged(const QString &) ), this, SLOT( slotINDISetTargetCoordDeviceRA() ) );
	connect( argSetTargetCoordINDI->DecBox, SIGNAL( textChanged(const QString &) ), this, SLOT( slotINDISetTargetCoordDeviceDEC() ) );
	
	// INDI Set Target Name
	connect (argSetTargetNameINDI->targetName, SIGNAL( textChanged(const QString &) ), this, SLOT(slotINDISetTargetNameTargetName()));
	
	// INDI Set Action
	connect (argSetActionINDI->actionName, SIGNAL( textChanged(const QString &) ), this, SLOT(slotINDISetActionName()));
	
	// INDI Wait For Action
	connect (argWaitForActionINDI->actionName, SIGNAL( textChanged(const QString &) ), this, SLOT(slotINDIWaitForActionName()));
	
	// INDI Set Focus Speed
	connect (argSetFocusSpeedINDI->speedIN, SIGNAL( valueChanged(int) ), this, SLOT(slotINDISetFocusSpeed()));
	
	// INDI Start Focus
	connect (argStartFocusINDI->directionCombo, SIGNAL( activated(const QString &) ), this, SLOT(slotINDIStartFocusDirection()));
	
	// INDI Set Focus Timeout
	connect (argSetFocusTimeoutINDI->timeOut, SIGNAL( valueChanged(int) ), this, SLOT(slotINDISetFocusTimeout()));
	
	// INDI Set Geo Location
	connect( argSetGeoLocationINDI->longBox, SIGNAL( textChanged(const QString &) ), this, SLOT( slotINDISetGeoLocationDeviceLong() ) );
	connect( argSetGeoLocationINDI->latBox, SIGNAL( textChanged(const QString &) ), this, SLOT( slotINDISetGeoLocationDeviceLat() ) );
	
	// INDI Start Exposure
	connect (argStartExposureINDI->timeOut, SIGNAL( valueChanged(int) ), this, SLOT(slotINDIStartExposureTimeout()));
	
	// INDI Set UTC
	connect (argSetUTCINDI->UTC, SIGNAL( textChanged(const QString &) ), this, SLOT(slotINDISetUTC()));
	
	// INDI Set Scope Action
	connect (argSetScopeActionINDI->actionCombo, SIGNAL( activated(const QString &) ), this, SLOT(slotINDISetScopeAction()));
	
	// INDI Set Frame type
	connect (argSetFrameTypeINDI->typeCombo, SIGNAL( activated(const QString &) ), this, SLOT(slotINDISetFrameType()));
	
	// INDI Set CCD Temp
	connect (argSetCCDTempINDI->temp, SIGNAL( valueChanged(int) ), this, SLOT(slotINDISetCCDTemp()));

	// INDI Set Filter Num
	connect (argSetFilterNumINDI->filter_num, SIGNAL( valueChanged(int) ), this, SLOT(slotINDISetFilterNum()));

	
	//disbale some buttons
	sb->CopyButton->setEnabled( false );
	sb->AddButton->setEnabled( false );
	sb->RemoveButton->setEnabled( false );
	sb->UpButton->setEnabled( false );
	sb->DownButton->setEnabled( false );
	sb->SaveButton->setEnabled( false );
	sb->SaveAsButton->setEnabled( false );
	sb->RunButton->setEnabled( false );
}

ScriptBuilder::~ScriptBuilder()
{
  while ( ! KStarsFunctionList.isEmpty() ) 
    delete KStarsFunctionList.takeFirst();

  while ( ! INDIFunctionList.isEmpty() ) 
    delete INDIFunctionList.takeFirst();
    
  while ( ! ScriptList.isEmpty() ) 
    delete ScriptList.takeFirst();
}

void ScriptBuilder::initViewOptions() {
	otv->optionsList()->setRootIsDecorated( true );
	QStringList fields;

	//InfoBoxes
	opsGUI = new QTreeWidgetItem( otv->optionsList(), QStringList(i18n( "InfoBoxes" )) );
	fields << i18n( "Toggle display of all InfoBoxes" ) << i18n( "bool" );
	new QTreeWidgetItem( opsGUI, fields );
	fields.clear();
	fields << i18n( "Toggle display of Time InfoBox" ) << i18n( "bool" );
	new QTreeWidgetItem( opsGUI, fields );
	fields.clear();
	fields << i18n( "Toggle display of Geographic InfoBox" ) << i18n( "bool" );
	new QTreeWidgetItem( opsGUI, fields );
	fields.clear();
	fields << i18n( "Toggle display of Focus InfoBox" ) << i18n( "bool" );
	new QTreeWidgetItem( opsGUI, fields );
	fields.clear();
	fields << i18n( "(un)Shade Time InfoBox" ) << i18n( "bool" );
	new QTreeWidgetItem( opsGUI, fields );
	fields.clear();
	fields << i18n( "(un)Shade Geographic InfoBox" ) << i18n( "bool" );
	new QTreeWidgetItem( opsGUI, fields );
	fields.clear();
	fields << i18n( "(un)Shade Focus InfoBox" ) << i18n( "bool" );
	new QTreeWidgetItem( opsGUI, fields );
	fields.clear();

	argChangeViewOption->OptionName->insertItem( "ShowInfoBoxes" );
	argChangeViewOption->OptionName->insertItem( "ShowTimeBox" );
	argChangeViewOption->OptionName->insertItem( "ShowGeoBox" );
	argChangeViewOption->OptionName->insertItem( "ShowFocusBox" );
	argChangeViewOption->OptionName->insertItem( "ShadeTimeBox" );
	argChangeViewOption->OptionName->insertItem( "ShadeGeoBox" );
	argChangeViewOption->OptionName->insertItem( "ShadeFocusBox" );

	//Toolbars
	opsToolbar = new QTreeWidgetItem( otv->optionsList(), QStringList(i18n( "Toolbars" )) );
	fields << i18n( "Toggle display of main toolbar" ) << i18n( "bool" );
	new QTreeWidgetItem( opsToolbar, fields );
	fields.clear();
	fields << i18n( "Toggle display of view toolbar" ) << i18n( "bool" );
	new QTreeWidgetItem( opsToolbar, fields );
	fields.clear();

	argChangeViewOption->OptionName->insertItem( "ShowMainToolBar" );
	argChangeViewOption->OptionName->insertItem( "ShowViewToolBar" );

	//Show Objects
	opsShowObj = new QTreeWidgetItem( otv->optionsList(), QStringList(i18n( "Show Objects" )) );
	fields << i18n( "Toggle display of Stars" ) << i18n( "bool" );
	new QTreeWidgetItem( opsShowObj, fields );
	fields.clear();
	fields << i18n( "Toggle display of all deep-sky objects" ) << i18n( "bool" );
	new QTreeWidgetItem( opsShowObj, fields );
	fields.clear();
	fields << i18n( "Toggle display of Messier object symbols" ) << i18n( "bool" );
	new QTreeWidgetItem( opsShowObj, fields );
	fields.clear();
	fields << i18n( "Toggle display of Messier object images" ) << i18n( "bool" );
	new QTreeWidgetItem( opsShowObj, fields );
	fields.clear();
	fields << i18n( "Toggle display of NGC objects" ) << i18n( "bool" );
	new QTreeWidgetItem( opsShowObj, fields );
	fields.clear();
	fields << i18n( "Toggle display of IC objects" ) << i18n( "bool" );
	new QTreeWidgetItem( opsShowObj, fields );
	fields.clear();
	fields << i18n( "Toggle display of all solar system bodies" ) << i18n( "bool" );
	new QTreeWidgetItem( opsShowObj, fields );
	fields.clear();
	fields << i18n( "Toggle display of Sun" ) << i18n( "bool" );
	new QTreeWidgetItem( opsShowObj, fields );
	fields.clear();
	fields << i18n( "Toggle display of Moon" ) << i18n( "bool" );
	new QTreeWidgetItem( opsShowObj, fields );
	fields.clear();
	fields << i18n( "Toggle display of Mercury" ) << i18n( "bool" );
	new QTreeWidgetItem( opsShowObj, fields );
	fields.clear();
	fields << i18n( "Toggle display of Venus" ) << i18n( "bool" );
	new QTreeWidgetItem( opsShowObj, fields );
	fields.clear();
	fields << i18n( "Toggle display of Mars" ) << i18n( "bool" );
	new QTreeWidgetItem( opsShowObj, fields );
	fields.clear();
	fields << i18n( "Toggle display of Jupiter" ) << i18n( "bool" );
	new QTreeWidgetItem( opsShowObj, fields );
	fields.clear();
	fields << i18n( "Toggle display of Saturn" ) << i18n( "bool" );
	new QTreeWidgetItem( opsShowObj, fields );
	fields.clear();
	fields << i18n( "Toggle display of Uranus" ) << i18n( "bool" );
	new QTreeWidgetItem( opsShowObj, fields );
	fields.clear();
	fields << i18n( "Toggle display of Neptune" ) << i18n( "bool" );
	new QTreeWidgetItem( opsShowObj, fields );
	fields.clear();
	fields << i18n( "Toggle display of Pluto" ) << i18n( "bool" );
	new QTreeWidgetItem( opsShowObj, fields );
	fields.clear();
	fields << i18n( "Toggle display of Asteroids" ) << i18n( "bool" );
	new QTreeWidgetItem( opsShowObj, fields );
	fields.clear();
	fields << i18n( "Toggle display of Comets" ) << i18n( "bool" );
	new QTreeWidgetItem( opsShowObj, fields );
	fields.clear();

	argChangeViewOption->OptionName->insertItem( "ShowSAO" );
	argChangeViewOption->OptionName->insertItem( "ShowDeepSky" );
	argChangeViewOption->OptionName->insertItem( "ShowMess" );
	argChangeViewOption->OptionName->insertItem( "ShowMessImages" );
	argChangeViewOption->OptionName->insertItem( "ShowNGC" );
	argChangeViewOption->OptionName->insertItem( "ShowIC" );
	argChangeViewOption->OptionName->insertItem( "ShowPlanets" );
	argChangeViewOption->OptionName->insertItem( "ShowSun" );
	argChangeViewOption->OptionName->insertItem( "ShowMoon" );
	argChangeViewOption->OptionName->insertItem( "ShowMercury" );
	argChangeViewOption->OptionName->insertItem( "ShowVenus" );
	argChangeViewOption->OptionName->insertItem( "ShowMars" );
	argChangeViewOption->OptionName->insertItem( "ShowJupiter" );
	argChangeViewOption->OptionName->insertItem( "ShowSaturn" );
	argChangeViewOption->OptionName->insertItem( "ShowUranus" );
	argChangeViewOption->OptionName->insertItem( "ShowNeptune" );
	argChangeViewOption->OptionName->insertItem( "ShowPluto" );
	argChangeViewOption->OptionName->insertItem( "ShowAsteroids" );
	argChangeViewOption->OptionName->insertItem( "ShowComets" );

	opsShowOther = new QTreeWidgetItem( otv->optionsList(), QStringList(i18n( "Show Other" )) );
	fields << i18n( "Toggle display of constellation lines" ) << i18n( "bool" );
	new QTreeWidgetItem( opsShowOther, fields );
	fields.clear();
	fields << i18n( "Toggle display of constellation boundaries" ) << i18n( "bool" );
	new QTreeWidgetItem( opsShowOther, fields );
	fields.clear();
	fields << i18n( "Toggle display of constellation names" ) << i18n( "bool" );
	new QTreeWidgetItem( opsShowOther, fields );
	fields.clear();
	fields << i18n( "Toggle display of Milky Way" ) << i18n( "bool" );
	new QTreeWidgetItem( opsShowOther, fields );
	fields.clear();
	fields << i18n( "Toggle display of the coordinate grid" ) << i18n( "bool" );
	new QTreeWidgetItem( opsShowOther, fields );
	fields.clear();
	fields << i18n( "Toggle display of the celestial equator" ) << i18n( "bool" );
	new QTreeWidgetItem( opsShowOther, fields );
	fields.clear();
	fields << i18n( "Toggle display of the ecliptic" ) << i18n( "bool" );
	new QTreeWidgetItem( opsShowOther, fields );
	fields.clear();
	fields << i18n( "Toggle display of the horizon line" ) << i18n( "bool" );
	new QTreeWidgetItem( opsShowOther, fields );
	fields.clear();
	fields << i18n( "Toggle display of the opaque ground" ) << i18n( "bool" );
	new QTreeWidgetItem( opsShowOther, fields );
	fields.clear();
	fields << i18n( "Toggle display of star name labels" ) << i18n( "bool" );
	new QTreeWidgetItem( opsShowOther, fields );
	fields.clear();
	fields << i18n( "Toggle display of star magnitude labels" ) << i18n( "bool" );
	new QTreeWidgetItem( opsShowOther, fields );
	fields.clear();
	fields << i18n( "Toggle display of asteroid name labels" ) << i18n( "bool" );
	new QTreeWidgetItem( opsShowOther, fields );
	fields.clear();
	fields << i18n( "Toggle display of comet name labels" ) << i18n( "bool" );
	new QTreeWidgetItem( opsShowOther, fields );
	fields.clear();
	fields << i18n( "Toggle display of planet name labels" ) << i18n( "bool" );
	new QTreeWidgetItem( opsShowOther, fields );
	fields.clear();
	fields << i18n( "Toggle display of planet images" ) << i18n( "bool" );
	new QTreeWidgetItem( opsShowOther, fields );
	fields.clear();

	argChangeViewOption->OptionName->insertItem( "ShowCLines" );
	argChangeViewOption->OptionName->insertItem( "ShowCBounds" );
	argChangeViewOption->OptionName->insertItem( "ShowCNames" );
	argChangeViewOption->OptionName->insertItem( "ShowMilkyWay" );
	argChangeViewOption->OptionName->insertItem( "ShowGrid" );
	argChangeViewOption->OptionName->insertItem( "ShowEquator" );
	argChangeViewOption->OptionName->insertItem( "ShowEcliptic" );
	argChangeViewOption->OptionName->insertItem( "ShowHorizon" );
	argChangeViewOption->OptionName->insertItem( "ShowGround" );
	argChangeViewOption->OptionName->insertItem( "ShowStarNames" );
	argChangeViewOption->OptionName->insertItem( "ShowStarMagnitudes" );
	argChangeViewOption->OptionName->insertItem( "ShowAsteroidNames" );
	argChangeViewOption->OptionName->insertItem( "ShowCometNames" );
	argChangeViewOption->OptionName->insertItem( "ShowPlanetNames" );
	argChangeViewOption->OptionName->insertItem( "ShowPlanetImages" );

	opsCName = new QTreeWidgetItem( otv->optionsList(), QStringList(i18n( "Constellation Names" )) );
	fields << i18n( "Show Latin constellation names" ) << i18n( "bool" );
	new QTreeWidgetItem( opsCName, fields );
	fields.clear();
	fields << i18n( "Show constellation names in local language" ) << i18n( "bool" );
	new QTreeWidgetItem( opsCName, fields );
	fields.clear();
	fields << i18n( "Show IAU-standard constellation abbreviations" ) << i18n( "bool" );
	new QTreeWidgetItem( opsCName, fields );
	fields.clear();

	argChangeViewOption->OptionName->insertItem( "UseLatinConstellNames" );
	argChangeViewOption->OptionName->insertItem( "UseLocalConstellNames" );
	argChangeViewOption->OptionName->insertItem( "UseAbbrevConstellNames" );

	opsHide = new QTreeWidgetItem( otv->optionsList(), QStringList(i18n( "Hide Items" )) );
	fields << i18n( "Toggle whether objects hidden while slewing display" ) << i18n( "bool" );
	new QTreeWidgetItem( opsHide, fields );
	fields.clear();
	fields << i18n( "Timestep threshold (in seconds) for hiding objects")  << i18n( "double" );
	new QTreeWidgetItem( opsHide, fields );
	fields.clear();
	fields << i18n( "Hide faint stars while slewing?" ) << i18n( "bool" );
	new QTreeWidgetItem( opsHide, fields );
	fields.clear();
	fields << i18n( "Hide solar system bodies while slewing?" ) << i18n( "bool" );
	new QTreeWidgetItem( opsHide, fields );
	fields.clear();
	fields << i18n( "Hide Messier objects while slewing?" ) << i18n( "bool" );
	new QTreeWidgetItem( opsHide, fields );
	fields.clear();
	fields << i18n( "Hide NGC objects while slewing?" ) << i18n( "bool" );
	new QTreeWidgetItem( opsHide, fields );
	fields.clear();
	fields << i18n( "Hide IC objects while slewing?" ) << i18n( "bool" );
	new QTreeWidgetItem( opsHide, fields );
	fields.clear();
	fields << i18n( "Hide Milky Way while slewing?" ) << i18n( "bool" );
	new QTreeWidgetItem( opsHide, fields );
	fields.clear();
	fields << i18n( "Hide constellation names while slewing?" ) << i18n( "bool" );
	new QTreeWidgetItem( opsHide, fields );
	fields.clear();
	fields << i18n( "Hide constellation lines while slewing?" ) << i18n( "bool" );
	new QTreeWidgetItem( opsHide, fields );
	fields.clear();
	fields << i18n( "Hide constellation boundaries while slewing?" ) << i18n( "bool" );
	new QTreeWidgetItem( opsHide, fields );
	fields.clear();
	fields << i18n( "Hide coordinate grid while slewing?" ) << i18n( "bool" );
	new QTreeWidgetItem( opsHide, fields );
	fields.clear();

	argChangeViewOption->OptionName->insertItem( "HideOnSlew" );
	argChangeViewOption->OptionName->insertItem( "SlewTimeScale" );
	argChangeViewOption->OptionName->insertItem( "HideStars" );
	argChangeViewOption->OptionName->insertItem( "HidePlanets" );
	argChangeViewOption->OptionName->insertItem( "HideMessier" );
	argChangeViewOption->OptionName->insertItem( "HideNGC" );
	argChangeViewOption->OptionName->insertItem( "HideIC" );
	argChangeViewOption->OptionName->insertItem( "HideMilkyWay" );
	argChangeViewOption->OptionName->insertItem( "HideCNames" );
	argChangeViewOption->OptionName->insertItem( "HideCLines" );
	argChangeViewOption->OptionName->insertItem( "HideCBounds" );
	argChangeViewOption->OptionName->insertItem( "HideGrid" );

	opsSkymap = new QTreeWidgetItem( otv->optionsList(), QStringList(i18n( "Skymap Options" )) );
	fields << i18n( "Use Horizontal coordinates? (otherwise, use Equatorial)")  << i18n( "bool" );
	new QTreeWidgetItem( opsSkymap, fields );
	fields.clear();
	fields << i18n( "Set the Zoom Factor" ) << i18n( "double" );
	new QTreeWidgetItem( opsSkymap, fields );
	fields.clear();
	fields << i18n( "Select angular size for the FOV symbol (in arcmin)")  << i18n( "double" );
	new QTreeWidgetItem( opsSkymap, fields );
	fields.clear();
	fields << i18n( "Select shape for the FOV symbol (0=Square, 1=Circle, 2=Crosshairs, 4=Bullseye)" ) << i18n( "int" );
	new QTreeWidgetItem( opsSkymap, fields );
	fields.clear();
	fields << i18n( "Select color for the FOV symbol" ) << i18n( "string" );
	new QTreeWidgetItem( opsSkymap, fields );
	fields.clear();
	fields << i18n( "Use animated slewing? (otherwise, \"snap\" to new focus)" ) << i18n( "bool" );
	new QTreeWidgetItem( opsSkymap, fields );
	fields.clear();
	fields << i18n( "Correct for atmospheric refraction?" ) << i18n( "bool" );
	new QTreeWidgetItem( opsSkymap, fields );
	fields.clear();
	fields << i18n( "Automatically attach name label to centered object?" ) << i18n( "bool" );
	new QTreeWidgetItem( opsSkymap, fields );
	fields.clear();
	fields << i18n( "Attach temporary name label when hovering mouse over an object?" ) << i18n( "bool" );
	new QTreeWidgetItem( opsSkymap, fields );
	fields.clear();
	fields << i18n( "Automatically add trail to centered solar system body?" ) << i18n( "bool" );
	new QTreeWidgetItem( opsSkymap, fields );
	fields.clear();
	fields << i18n( "Planet trails fade to sky color? (otherwise color is constant)" ) << i18n( "bool" );
	new QTreeWidgetItem( opsSkymap, fields );
	fields.clear();

	argChangeViewOption->OptionName->insertItem( "UseAltAz" );
	argChangeViewOption->OptionName->insertItem( "ZoomFactor" );
	argChangeViewOption->OptionName->insertItem( "FOVName" );
	argChangeViewOption->OptionName->insertItem( "FOVSize" );
	argChangeViewOption->OptionName->insertItem( "FOVShape" );
	argChangeViewOption->OptionName->insertItem( "FOVColor" );
	argChangeViewOption->OptionName->insertItem( "UseRefraction" );
	argChangeViewOption->OptionName->insertItem( "UseAutoLabel" );
	argChangeViewOption->OptionName->insertItem( "UseHoverLabel" );
	argChangeViewOption->OptionName->insertItem( "UseAutoTrail" );
	argChangeViewOption->OptionName->insertItem( "AnimateSlewing" );
	argChangeViewOption->OptionName->insertItem( "FadePlanetTrails" );

	opsLimit = new QTreeWidgetItem( otv->optionsList(), QStringList(i18n( "Limits" )) );
	fields << i18n( "magnitude of faintest star drawn on map when zoomed in" ) << i18n( "double" );
	new QTreeWidgetItem( opsLimit, fields );
	fields.clear();
	fields << i18n( "magnitude of faintest star drawn on map when zoomed out" ) << i18n( "double" );
	new QTreeWidgetItem( opsLimit, fields );
	fields.clear();
	fields << i18n( "magnitude of faintest nonstellar object drawn on map when zoomed in" ) << i18n( "double" );
	new QTreeWidgetItem( opsLimit, fields );
	fields.clear();
	fields << i18n( "magnitude of faintest nonstellar object drawn on map when zoomed out" ) << i18n( "double" );
	new QTreeWidgetItem( opsLimit, fields );
	fields.clear();
	fields << i18n( "magnitude of faintest star labeled on map" ) << i18n( "double" );
	new QTreeWidgetItem( opsLimit, fields );
	fields.clear();
	fields << i18n( "magnitude of brightest star hidden while slewing" ) << i18n( "double" );
	new QTreeWidgetItem( opsLimit, fields );
	fields.clear();
	fields << i18n( "magnitude of faintest asteroid drawn on map" ) << i18n( "double" );
	new QTreeWidgetItem( opsLimit, fields );
	fields.clear();
	fields << i18n( "magnitude of faintest asteroid labeled on map" ) << i18n( "double" );
	new QTreeWidgetItem( opsLimit, fields );
	fields.clear();
	fields << i18n( "comets nearer to the Sun than this (in AU) are labeled on map" ) << i18n( "double" );
	new QTreeWidgetItem( opsLimit, fields );
	fields.clear();

	argChangeViewOption->OptionName->insertItem( "magLimitDrawStar" );
	argChangeViewOption->OptionName->insertItem( "magLimitDrawStarZoomOut" );
	argChangeViewOption->OptionName->insertItem( "magLimitDrawDeepSky" );
	argChangeViewOption->OptionName->insertItem( "magLimitDrawDeepSkyZoomOut" );
	argChangeViewOption->OptionName->insertItem( "magLimitDrawStarInfo" );
	argChangeViewOption->OptionName->insertItem( "magLimitHideStar" );
	argChangeViewOption->OptionName->insertItem( "magLimitAsteroid" );
	argChangeViewOption->OptionName->insertItem( "magLimitAsteroidName" );
	argChangeViewOption->OptionName->insertItem( "maxRadCometName" );

	//init the list of color names and values
	for ( unsigned int i=0; i < ks->data()->colorScheme()->numberOfColors(); ++i ) {
		argSetColor->ColorName->insertItem( ks->data()->colorScheme()->nameAt(i) );
	}
	
	//init list of color scheme names
	argLoadColorScheme->SchemeList->insertItem( i18n( "use default color scheme", "Default Colors" ) );
	argLoadColorScheme->SchemeList->insertItem( i18n( "use 'star chart' color scheme", "Star Chart" ) );
	argLoadColorScheme->SchemeList->insertItem( i18n( "use 'night vision' color scheme", "Night Vision" ) );
	argLoadColorScheme->SchemeList->insertItem( i18n( "use 'moonless night' color scheme", "Moonless Night" ) );
	
	QFile file;
	QString line;
	file.setName( locate( "appdata", "colors.dat" ) ); //determine filename in local user KDE directory tree.
	if ( file.open( QIODevice::ReadOnly ) ) {
		QTextStream stream( &file );

  	while ( !stream.atEnd() ) {
			line = stream.readLine();
			argLoadColorScheme->SchemeList->insertItem( line.left( line.find( ':' ) ) );
		}
		file.close();
	}
}

//Slots defined in ScriptBuilderUI
void ScriptBuilder::slotNew() {
	saveWarning();
	if ( !UnsavedChanges ) {
		ScriptList.clear();
		sb->ScriptListBox->clear();
		sb->ArgStack->setCurrentWidget( argBlank );

		sb->CopyButton->setEnabled( false );
		sb->RemoveButton->setEnabled( false );
		sb->RunButton->setEnabled( false );

		currentFileURL = QString();
		currentScriptName = QString();
	}
}

void ScriptBuilder::slotOpen() {
	saveWarning();

	QString fname;
	KTempFile tmpfile;
	tmpfile.setAutoDelete(true);

	if ( !UnsavedChanges ) {
		currentFileURL = KFileDialog::getOpenURL( currentDir, "*.kstars|KStars Scripts (*.kstars)" );

		if ( currentFileURL.isValid() ) {
			currentDir = currentFileURL.directory();

			ScriptList.clear();
			sb->ScriptListBox->clear();
			sb->ArgStack->setCurrentWidget( argBlank );

			if ( currentFileURL.isLocalFile() ) {
				fname = currentFileURL.path();
			} else {
				fname = tmpfile.name();
				if ( ! KIO::NetAccess::download( currentFileURL, fname, (QWidget*) 0 ) )
					KMessageBox::sorry( 0, i18n( "Could not download remote file." ), i18n( "Download Error" ) );
			}

			QFile f( fname );
			if ( !f.open( QIODevice::ReadOnly) ) {
				QString message = i18n( "Could not open file %1." ).arg( f.name() );
				KMessageBox::sorry( 0, message, i18n( "Could Not Open File" ) );
				currentFileURL = QString();
				return;
			}

			QTextStream istream(&f);
			readScript( istream );

			f.close();
		} else if ( ! currentFileURL.url().isEmpty() ) {
			QString message = i18n( "Invalid URL: %1" ).arg( currentFileURL.url() );
			KMessageBox::sorry( 0, message, i18n( "Invalid URL" ) );
			currentFileURL = QString();
		}
	}
}

void ScriptBuilder::slotSave() {
	QString fname;
	KTempFile tmpfile;
	tmpfile.setAutoDelete(true);

	if ( currentScriptName.isEmpty() ) {
		//Get Script Name and Author info
		if ( snd->exec() == QDialog::Accepted ) {
			currentScriptName = snd->scriptName();
			currentAuthor = snd->authorName();
		} else {
			return;
		}
	}

	if ( currentFileURL.isEmpty() )
		currentFileURL = KFileDialog::getSaveURL( currentDir, "*.kstars|KStars Scripts (*.kstars)" );

	if ( currentFileURL.isValid() ) {
		currentDir = currentFileURL.directory();

		if ( currentFileURL.isLocalFile() ) {
			fname = currentFileURL.path();
			
			//Warn user if file exists
			if (QFile::exists(currentFileURL.path())) {
				int r=KMessageBox::warningContinueCancel(static_cast<QWidget *>(parent()),
						i18n( "A file named \"%1\" already exists. "
								"Overwrite it?" ).arg(currentFileURL.fileName()),
						i18n( "Overwrite File?" ),
						i18n( "&Overwrite" ) );
		
				if(r==KMessageBox::Cancel) return;
			}
		} else {
			fname = tmpfile.name();
		}
		
		if ( fname.right( 7 ).lower() != ".kstars" ) fname += ".kstars";

		QFile f( fname );
		if ( !f.open( QIODevice::WriteOnly) ) {
			QString message = i18n( "Could not open file %1." ).arg( f.name() );
			KMessageBox::sorry( 0, message, i18n( "Could Not Open File" ) );
			currentFileURL = QString();
			return;
		}

		QTextStream ostream(&f);
		writeScript( ostream );
		f.close();

		//set rwx for owner, rx for group, rx for other
		chmod( fname.ascii(), S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH );

		if ( tmpfile.name() == fname ) { //need to upload to remote location
			if ( ! KIO::NetAccess::upload( tmpfile.name(), currentFileURL, (QWidget*) 0 ) ) {
				QString message = i18n( "Could not upload image to remote location: %1" ).arg( currentFileURL.prettyURL() );
				KMessageBox::sorry( 0, message, i18n( "Could not upload file" ) );
			}
		}

		setUnsavedChanges( false );

	} else {
		QString message = i18n( "Invalid URL: %1" ).arg( currentFileURL.url() );
		KMessageBox::sorry( 0, message, i18n( "Invalid URL" ) );
		currentFileURL = QString();
	}
}

void ScriptBuilder::slotSaveAs() {
	currentFileURL = QString();
	currentScriptName = QString();
	slotSave();
}

void ScriptBuilder::saveWarning() {
	if ( UnsavedChanges ) {
		QString caption = i18n( "Save Changes to Script?" );
		QString message = i18n( "The current script has unsaved changes.  Would you like to save before closing it?" );
		int ans = KMessageBox::warningYesNoCancel( 0, message, caption, KStdGuiItem::save(), KStdGuiItem::discard() );
		if ( ans == KMessageBox::Yes ) {
			slotSave();
			setUnsavedChanges( false );
		} else if ( ans == KMessageBox::No ) {
			setUnsavedChanges( false );
		}

		//Do nothing if 'cancel' selected
	}
}

void ScriptBuilder::slotRunScript() {
	//hide window while script runs
// If this is uncommented, the program hangs before the script is executed.  Why?
//	hide();

	//Save current script to a temporary file, then execute that file.
	//For some reason, I can't use KTempFile here!  If I do, then the temporary script
	//is not executable.  Bizarre...
	//KTempFile tmpfile;
	//QString fname = tmpfile.name();
	QString fname = locateLocal( "tmp", "kstars-tempscript" );

	QFile f( fname );
	if ( f.exists() ) f.remove();
	if ( !f.open( QIODevice::WriteOnly) ) {
		QString message = i18n( "Could not open file %1." ).arg( f.name() );
		KMessageBox::sorry( 0, message, i18n( "Could Not Open File" ) );
		currentFileURL = QString();
		return;
	}

	QTextStream ostream(&f);
	writeScript( ostream );
	f.close();

	//set rwx for owner, rx for group, rx for other
	chmod( f.name().ascii(), S_IRUSR|S_IWUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH );

	KProcess p;
	p << f.name();
	if ( ! p.start( KProcess::DontCare ) )
		kDebug() << "Process did not start." << endl;

	while ( p.isRunning() ) kapp->processEvents(); //otherwise tempfile may get deleted before script completes.

	//delete temp file
	if ( f.exists() ) f.remove();

	//uncomment if 'hide()' is uncommented...
//	show();
}

void ScriptBuilder::writeScript( QTextStream &ostream ) {
	QString mainpre  = "dcop $KSTARS $MAIN  ";
	QString clockpre = "dcop $KSTARS $CLOCK ";

	//Write script header
	ostream << "#!/bin/bash" << endl;
	ostream << "#KStars DCOP script: " << currentScriptName << endl;
	ostream << "#by " << currentAuthor << endl;
	ostream << "#last modified: " << KStarsDateTime::currentDateTime().toString() << endl;
	ostream << "#" << endl;
	ostream << "KSTARS=`dcopfind -a 'kstars*'`" << endl;
	ostream << "MAIN=KStarsInterface" << endl;
	ostream << "CLOCK=clock#1" << endl;

	foreach ( ScriptFunction *sf, ScriptList )
	{
	        if (!sf->valid()) continue;
		if ( sf->isClockFunction() ) {
			ostream << clockpre << sf->scriptLine() << endl;
		} else {
			ostream << mainpre  << sf->scriptLine() << endl;
			if (sb->AppendINDIWait->isChecked() && !sf->INDIProperty().isEmpty())
			{
			  // Special case for telescope action, we need to know the parent property
			  if (sf->INDIProperty() == "CHECK")
			  {
			    if (sf->argVal(1) == "SLEW" || sf->argVal(1) == "TRACK" || sf->argVal(1) == "SYNC")
			      sf->setINDIProperty("ON_COORD_SET");
			    else if (sf->argVal(1) == "ABORT")
			      sf->setINDIProperty("ABORT_MOTION");
			    else
			      sf->setINDIProperty("PARK");
			  }
			  
			  if ( sf->argVal(0).contains(" ")) 
			    ostream << mainpre << "waitForINDIAction " << "\"" << sf->argVal(0) << "\" " << sf->INDIProperty() << endl;
			  else
			    ostream << mainpre << "waitForINDIAction " << sf->argVal(0) << " " << sf->INDIProperty() << endl;
			}
		}
	}

	//Write script footer
	ostream << "##" << endl;
}

void ScriptBuilder::readScript( QTextStream &istream ) {
	QString line;

	while ( ! istream.atEnd() ) {
		line = istream.readLine();

		//look for name of script
		if ( line.contains( "#KStars DCOP script: " ) )
			currentScriptName = line.mid( 21 ).trimmed();

		//look for author of scriptbuilder
		if ( line.contains( "#by " ) )
			currentAuthor = line.mid( 4 ).trimmed();

		//Actual script functions
		if ( line.left(4) == "dcop" ) {

		//is ClockFunction?
			bool clockfcn( false );
			if ( line.contains( "$CLOCK" ) ) clockfcn = true;

			//remove leading dcop prefix
			line = line.mid( 20 );

			//construct a stringlist that is fcn name and its arg name/value pairs
			QStringList fn = QStringList::split( " ", line );
			if ( parseFunction( fn ) )
			{
			  sb->ScriptListBox->addItem( ScriptList.last()->name() );
			  // Initially, any read script is valid!
			  ScriptList.last()->setValid(true);
			}
			else kWarning() << i18n( "Could not parse script.  Line was: %1" ).arg( line ) << endl;

		} // end if left(4) == "dcop"
	} // end while !atEnd()

	//Select first item in sb->ScriptListBox
	if ( sb->ScriptListBox->count() ) {
		sb->ScriptListBox->setCurrentItem( 0 );
		slotArgWidget();
	}
}

bool ScriptBuilder::parseFunction( QStringList &fn )
{
        // clean up the string list first if needed
        // We need to perform this in case we havea quoted string "NGC 3000" because this will counted
        // as two arguments, and it should be counted as one.
        bool foundQuote(false), quoteProcessed(false);
	QString cur, arg;
	QStringList::iterator it;
	
	for (it = fn.begin(); it != fn.end(); ++it)
	{
	  cur = (*it);
	  
	  if ( cur.startsWith("\""))
	  {
	    arg += cur.right(cur.length() - 1);
	    arg += " ";
	    foundQuote = true;
	    quoteProcessed = true;
	  }
	  else if (cur.endsWith("\""))
	  {
	    arg += cur.left(cur.length() -1);
	    arg += "'";
	    foundQuote = false;
	  }
	  else if (foundQuote)
	  {
	    arg += cur;
	    arg += " ";
	  }
	  else
	  {
	    arg += cur;
	    arg += "'";
	  }
	}
	    
	if (quoteProcessed)
	  fn = QStringList::split( "'", arg );
	
	//loop over known functions to find a name match
	foreach ( ScriptFunction *sf, KStarsFunctionList ) 
	{
		if ( fn[0] == sf->name() ) {

			if ( fn[0] == "setGeoLocation" ) {
				QString city( fn[1] ), prov( QString::null ), cntry( fn[2] );
				if ( fn.count() == 4 ) { prov = fn[2]; cntry = fn[3]; }
				if ( fn.count() == 3 || fn.count() == 4 ) {
					ScriptList.append( new ScriptFunction( sf ) );
					ScriptList.last()->setArg( 0, city );
					ScriptList.last()->setArg( 1, prov );
					ScriptList.last()->setArg( 2, cntry );
				} else return false;

			} else if ( fn.count() != sf->numArgs() + 1 ) return false;

			ScriptList.append( new ScriptFunction( sf ) );

			for ( unsigned int i=0; i<sf->numArgs(); ++i )
				ScriptList.last()->setArg( i, fn[i+1] );

			return true;
		}
		
		foreach ( ScriptFunction *sf, INDIFunctionList ) 
		{
		  if ( fn[0] == sf->name() )
		  {

		    if ( fn.count() != sf->numArgs() + 1 ) return false;

		    ScriptList.append( new ScriptFunction( sf ) );

		    for ( unsigned int i=0; i<sf->numArgs(); ++i )
		      ScriptList.last()->setArg( i, fn[i+1] );

		    return true;
		  }
		}
	}

	//if we get here, no function-name match was found
	return false;
}

void ScriptBuilder::setUnsavedChanges( bool b ) {
	UnsavedChanges = b;
	sb->SaveButton->setEnabled( b );
	sb->SaveAsButton->setEnabled( b );
}

void ScriptBuilder::slotCopyFunction() {
	if ( ! UnsavedChanges ) setUnsavedChanges( true );

	int Pos = sb->ScriptListBox->currentRow() + 1;
	ScriptList.insert( Pos, new ScriptFunction( ScriptList[ Pos-1 ] ) );
	//copy ArgVals
	for ( unsigned int i=0; i < ScriptList[ Pos-1 ]->numArgs(); ++i )
		ScriptList[Pos]->setArg(i, ScriptList[ Pos-1 ]->argVal(i) );

	sb->ScriptListBox->insertItem( Pos, ScriptList[Pos]->name());
	//sb->ScriptListBox->setSelected( Pos, true );
	  sb->ScriptListBox->setCurrentRow(Pos);
}

void ScriptBuilder::slotRemoveFunction() {
	setUnsavedChanges( true );

	int Pos = sb->ScriptListBox->currentRow();
	ScriptList.removeAt( Pos );
	sb->ScriptListBox->takeItem( Pos );
	if ( sb->ScriptListBox->count() == 0 ) {
		sb->ArgStack->setCurrentWidget( argBlank );
		sb->CopyButton->setEnabled( false );
		sb->RemoveButton->setEnabled( false );
	} else {
		//sb->ScriptListBox->setSelected( Pos, true );
		sb->ScriptListBox->setCurrentRow(Pos);
	}
}

void ScriptBuilder::slotAddFunction() {
  
        ScriptFunction *sc = NULL, *found = NULL;
	QTreeWidgetItem *currentItem = sb->FunctionTree->currentItem();
	
	if ( currentItem == NULL || currentItem->parent() == 0)
	  return;
	
	foreach ( sc, KStarsFunctionList )
	  if (sc->prototype() == currentItem->text(0))
		{
		    found = sc;
		    break;
		}
	
	 if (found == NULL)
	 {
	   foreach ( sc, INDIFunctionList )
	     if (sc->prototype() == currentItem->text(0))
		{
			found = sc;
			break;
		}
	 }
	 
	 if (found == NULL) return;
	  
	  setUnsavedChanges( true );

	  int Pos = sb->ScriptListBox->currentRow() + 1;

	  ScriptList.insert( Pos, new ScriptFunction(found) );
	  sb->ScriptListBox->insertItem(Pos,  ScriptList[Pos]->name());
	  sb->ScriptListBox->setCurrentRow(Pos);
	  slotArgWidget();
}

void ScriptBuilder::slotMoveFunctionUp() {
	if ( sb->ScriptListBox->currentRow() > 0 ) {
		setUnsavedChanges( true );

		//QString t = sb->ScriptListBox->currentItem()->text();
		QString t = sb->ScriptListBox->currentItem()->text();
		unsigned int n = sb->ScriptListBox->currentRow();

		ScriptFunction *tmp = ScriptList.takeAt( n );
		ScriptList.insert( n-1, tmp );

		sb->ScriptListBox->takeItem( n );
		sb->ScriptListBox->insertItem( n-1, t);
		sb->ScriptListBox->setCurrentRow(n-1);
	}
}

void ScriptBuilder::slotMoveFunctionDown() {
	if ( sb->ScriptListBox->currentRow() > -1 &&
				sb->ScriptListBox->currentRow() < ((int) sb->ScriptListBox->count())-1 ) {
		setUnsavedChanges( true );

		QString t = sb->ScriptListBox->currentItem()->text();
		unsigned int n = sb->ScriptListBox->currentRow();

		ScriptFunction *tmp = ScriptList.takeAt( n );
		ScriptList.insert( n+1, tmp );

		sb->ScriptListBox->takeItem( n );
		sb->ScriptListBox->insertItem( n+1 , t);
		sb->ScriptListBox->setCurrentRow( n+1);
	}
}

void ScriptBuilder::slotArgWidget() {
	//First, setEnabled on buttons that act on the selected script function
	if ( sb->ScriptListBox->currentRow() == -1 ) { //no selection
		sb->CopyButton->setEnabled( false );
		sb->RemoveButton->setEnabled( false );
		sb->UpButton->setEnabled( false );
		sb->DownButton->setEnabled( false );
	} else if ( sb->ScriptListBox->count() == 1 ) { //only one item, so disable up/down buttons
		sb->CopyButton->setEnabled( true );
		sb->RemoveButton->setEnabled( true );
		sb->UpButton->setEnabled( false );
		sb->DownButton->setEnabled( false );
	} else if ( sb->ScriptListBox->currentRow() == 0 ) { //first item selected
		sb->CopyButton->setEnabled( true );
		sb->RemoveButton->setEnabled( true );
		sb->UpButton->setEnabled( false );
		sb->DownButton->setEnabled( true );
	} else if ( sb->ScriptListBox->currentRow() == ((int) sb->ScriptListBox->count())-1 ) { //last item selected
		sb->CopyButton->setEnabled( true );
		sb->RemoveButton->setEnabled( true );
		sb->UpButton->setEnabled( true );
		sb->DownButton->setEnabled( false );
	} else { //other item selected
		sb->CopyButton->setEnabled( true );
		sb->RemoveButton->setEnabled( true );
		sb->UpButton->setEnabled( true );
		sb->DownButton->setEnabled( true );
	}

	//sb->RunButton enabled when script not empty.
	if ( sb->ScriptListBox->count() ) {
		sb->RunButton->setEnabled( true );
	} else {
		sb->RunButton->setEnabled( false );
		setUnsavedChanges( false );
	}

	//Display the function's arguments widget
	if ( sb->ScriptListBox->currentRow() > -1 &&
				sb->ScriptListBox->currentRow() < ((int) sb->ScriptListBox->count()) ) {
		QString t = sb->ScriptListBox->currentItem()->text();
		unsigned int n = sb->ScriptListBox->currentRow();
		ScriptFunction *sf = ScriptList.at( n );

		if ( sf->name() == "lookTowards" ) {
			sb->ArgStack->setCurrentWidget( argLookToward );
			QString s = sf->argVal(0);
			argLookToward->FocusEdit->setCurrentText( s );

		} else if ( sf->name() == "setRaDec" ) {
			bool ok(false);
			double r(0.0),d(0.0);
			dms ra(0.0);

			sb->ArgStack->setCurrentWidget( argSetRaDec );

			ok = !sf->argVal(0).isEmpty();
			if (ok) r = sf->argVal(0).toDouble(&ok);
			else argSetRaDec->RABox->clear();
			if (ok) { ra.setH(r); argSetRaDec->RABox->showInHours( ra ); }

			ok = !sf->argVal(1).isEmpty();
			if (ok) d = sf->argVal(1).toDouble(&ok);
			else argSetRaDec->DecBox->clear();
			if (ok) argSetRaDec->DecBox->showInDegrees( dms(d) );

		} else if ( sf->name() == "setAltAz" ) {
			bool ok(false);
			double x(0.0),y(0.0);

			sb->ArgStack->setCurrentWidget( argSetAltAz );

			ok = !sf->argVal(0).isEmpty();
			if (ok) y = sf->argVal(0).toDouble(&ok);
			else argSetAltAz->AzBox->clear();
			if (ok) argSetAltAz->AltBox->showInDegrees( dms(y) );
			else argSetAltAz->AltBox->clear();

			ok = !sf->argVal(1).isEmpty();
			x = sf->argVal(1).toDouble(&ok);
			if (ok) argSetAltAz->AzBox->showInDegrees( dms(x) );

		} else if ( sf->name() == "zoomIn" ) {
			sb->ArgStack->setCurrentWidget( argBlank );
			//no Args

		} else if ( sf->name() == "zoomOut" ) {
			sb->ArgStack->setCurrentWidget( argBlank );
			//no Args

		} else if ( sf->name() == "defaultZoom" ) {
			sb->ArgStack->setCurrentWidget( argBlank );
			//no Args

		} else if ( sf->name() == "zoom" ) {
			sb->ArgStack->setCurrentWidget( argZoom );
			bool ok(false);
			/*double z = */sf->argVal(0).toDouble(&ok);
			if (ok) argZoom->ZoomBox->setText( sf->argVal(0) );
			else argZoom->ZoomBox->setText( "2000." );

		} else if ( sf->name() == "exportImage" ) {
			sb->ArgStack->setCurrentWidget( argExportImage );
			argExportImage->ExportFileName->setURL( sf->argVal(0) );
			bool ok(false);
			int w=0, h=0;
			w = sf->argVal(1).toInt( &ok );
			if (ok) h = sf->argVal(2).toInt( &ok );
			if (ok) { 
				argExportImage->ExportWidth->setValue( w ); 
				argExportImage->ExportHeight->setValue( h );
			} else { 
				argExportImage->ExportWidth->setValue( ks->map()->width() ); 
				argExportImage->ExportHeight->setValue( ks->map()->height() );
			}

		} else if ( sf->name() == "printImage" ) {
			if ( sf->argVal(0) == i18n( "true" ) ) argPrintImage->UsePrintDialog->setChecked( true );
			else argPrintImage->UsePrintDialog->setChecked( false );
			if ( sf->argVal(1) == i18n( "true" ) ) argPrintImage->UseChartColors->setChecked( true );
			else argPrintImage->UseChartColors->setChecked( false );

		} else if ( sf->name() == "setLocalTime" ) {
			sb->ArgStack->setCurrentWidget( argSetLocalTime );
			bool ok(false);
			int year=0, month=0, day=0, hour=0, min=0, sec=0;

			year = sf->argVal(0).toInt(&ok);
			if (ok) month = sf->argVal(1).toInt(&ok);
			if (ok) day   = sf->argVal(2).toInt(&ok);
			if (ok) argSetLocalTime->DateBox->setDate( ExtDate( year, month, day ) );
			else argSetLocalTime->DateBox->setDate( ExtDate::currentDate() );

			hour = sf->argVal(3).toInt(&ok);
			if ( sf->argVal(3).isEmpty() ) ok = false;
			if (ok) min = sf->argVal(4).toInt(&ok);
			if (ok) sec = sf->argVal(5).toInt(&ok);
			if (ok) argSetLocalTime->TimeBox->setTime( QTime( hour, min, sec ) );
			else argSetLocalTime->TimeBox->setTime( QTime( QTime::currentTime() ) );

		} else if ( sf->name() == "waitFor" ) {
			sb->ArgStack->setCurrentWidget( argWaitFor );
			bool ok(false);
			int sec = sf->argVal(0).toInt(&ok);
			if (ok) argWaitFor->DelayBox->setValue( sec );
			else argWaitFor->DelayBox->setValue( 0 );

		} else if ( sf->name() == "waitForKey" ) {
			sb->ArgStack->setCurrentWidget( argWaitForKey );
			if ( sf->argVal(0).length()==1 || sf->argVal(0).lower() == "space" )
				argWaitForKey->WaitKeyEdit->setText( sf->argVal(0) );
			else argWaitForKey->WaitKeyEdit->setText( QString() );

		} else if ( sf->name() == "setTracking" ) {
			sb->ArgStack->setCurrentWidget( argSetTracking );
			if ( sf->argVal(0) == i18n( "true" ) ) argSetTracking->CheckTrack->setChecked( true  );
			else argSetTracking->CheckTrack->setChecked( false );

		} else if ( sf->name() == "changeViewOption" ) {
			sb->ArgStack->setCurrentWidget( argChangeViewOption );
			//find argVal(0) in the combobox...if it isn't there, it will select nothing
			argChangeViewOption->OptionName->setCurrentItem( sf->argVal(0) );
			argChangeViewOption->OptionValue->setText( sf->argVal(1) );

		} else if ( sf->name() == "setGeoLocation" ) {
			sb->ArgStack->setCurrentWidget( argSetGeoLocation );
			argSetGeoLocation->CityName->setText( sf->argVal(0) );
			argSetGeoLocation->ProvinceName->setText( sf->argVal(1) );
			argSetGeoLocation->CountryName->setText( sf->argVal(2) );

		} else if ( sf->name() == "setColor" ) {
			sb->ArgStack->setCurrentWidget( argSetColor );
			if ( sf->argVal(0).isEmpty() ) sf->setArg( 0, "SkyColor" );  //initialize default value
			argSetColor->ColorName->setCurrentItem( ks->data()->colorScheme()->nameFromKey( sf->argVal(0) ) );
			argSetColor->ColorValue->setColor( QColor( sf->argVal(1).remove('\\') ) );

		} else if ( sf->name() == "loadColorScheme" ) {
			sb->ArgStack->setCurrentWidget( argLoadColorScheme );
			argLoadColorScheme->SchemeList->setCurrentItem( argLoadColorScheme->SchemeList->findItem( sf->argVal(0).remove('\"'), 0 ) );

		} else if ( sf->name() == "stop" ) {
			sb->ArgStack->setCurrentWidget( argBlank );
			//no Args

		} else if ( sf->name() == "start" ) {
			sb->ArgStack->setCurrentWidget( argBlank );
			//no Args

		} else if ( sf->name() == "setClockScale" ) {
			sb->ArgStack->setCurrentWidget( argTimeScale );
			bool ok(false);
			double ts = sf->argVal(0).toDouble(&ok);
			if (ok) argTimeScale->TimeScale->tsbox()->changeScale( float(ts) );
			else argTimeScale->TimeScale->tsbox()->changeScale( 0.0 );

		}
		else if (sf->name() == "startINDI") {
		  sb->ArgStack->setCurrentWidget( argStartINDI);
		  
		  argStartINDI->deviceName->setText(sf->argVal(0));
		  if (sf->argVal(1) == "true")
		    argStartINDI->LocalButton->setChecked(true);
		  else if (! sf->argVal(1).isEmpty())
		    argStartINDI->LocalButton->setChecked(false);
		}
		else if (sf->name() == "setINDIDevice")
		{
			sb->ArgStack->setCurrentWidget( argSetDeviceINDI);
			argSetDeviceINDI->deviceName->setText(sf->argVal(0));
		}
		else if (sf->name() == "shutdownINDI") {
		  sb->ArgStack->setCurrentWidget( argShutdownINDI);
		}
		else if (sf->name() == "switchINDI") {
		  sb->ArgStack->setCurrentWidget( argSwitchINDI);
		  
		  if (sf->argVal(0) == "true" || sf->argVal(0).isEmpty())
		    argSwitchINDI->OnButton->setChecked(true);
		  else
		    argSwitchINDI->OffButton->setChecked(true);
		}
		else if (sf->name() == "setINDIPort") {
		  sb->ArgStack->setCurrentWidget( argSetPortINDI);
		  
		  argSetPortINDI->devicePort->setText(sf->argVal(0));
		  
		  
		}
		else if (sf->name() == "setINDITargetCoord") {
		  bool ok(false);
		  double r(0.0),d(0.0);
		  dms ra(0.0);
		  
		  sb->ArgStack->setCurrentWidget( argSetTargetCoordINDI);
		  
		  ok = !sf->argVal(0).isEmpty();
		  if (ok) r = sf->argVal(0).toDouble(&ok);
		  else argSetTargetCoordINDI->RABox->clear();
		  if (ok) { ra.setH(r); argSetTargetCoordINDI->RABox->showInHours( ra ); }

		  ok = !sf->argVal(1).isEmpty();
		  if (ok) d = sf->argVal(1).toDouble(&ok);
		  else argSetTargetCoordINDI->DecBox->clear();
		  if (ok) argSetTargetCoordINDI->DecBox->showInDegrees( dms(d) );
		  
		  
		}
		else if (sf->name() == "setINDITargetName") {
		  sb->ArgStack->setCurrentWidget( argSetTargetNameINDI);
		  
		  argSetTargetNameINDI->targetName->setText(sf->argVal(0));
		  
		  
		}
		else if (sf->name() == "setINDIAction") {
		  sb->ArgStack->setCurrentWidget( argSetActionINDI);
		  
		  argSetActionINDI->actionName->setText(sf->argVal(0));
		  
		  
		}
		else if (sf->name() == "waitForINDIAction") {
		  sb->ArgStack->setCurrentWidget( argWaitForActionINDI);
		  
		  argWaitForActionINDI->actionName->setText(sf->argVal(0));
		  
		  
		}
		else if (sf->name() == "setINDIFocusSpeed") {
		  int t(0);
		  bool ok(false);
		  
		  sb->ArgStack->setCurrentWidget( argSetFocusSpeedINDI);

 		  t = sf->argVal(0).toInt(&ok);
		  if (ok) argSetFocusSpeedINDI->speedIN->setValue(t);
		  else argSetFocusSpeedINDI->speedIN->setValue(0);
		  
		  
		}
		else if (sf->name() == "startINDIFocus") {
		  sb->ArgStack->setCurrentWidget( argStartFocusINDI);
		  bool itemSet(false);
		  
		  for (int i=0; i < argStartFocusINDI->directionCombo->count(); i++)
		  {
		    if (argStartFocusINDI->directionCombo->text(i) == sf->argVal(0))
		    {
		      argStartFocusINDI->directionCombo->setCurrentItem(i);
		      itemSet = true;
		      break;
		    }
		  }
		  
		  if (!itemSet) argStartFocusINDI->directionCombo->setCurrentItem(0);
		  
		  
		}
		else if (sf->name() == "setINDIFocusTimeout") {
		  int t(0);
		  bool ok(false);
		  
		  sb->ArgStack->setCurrentWidget( argSetFocusTimeoutINDI);
		  
		  t = sf->argVal(0).toInt(&ok);
		  if (ok) argSetFocusTimeoutINDI->timeOut->setValue(t);
		  else argSetFocusTimeoutINDI->timeOut->setValue(0);
		  
		  
		  }
		  else if (sf->name() == "setINDIGeoLocation") {
		    bool ok(false);
		    double lo(0.0),la(0.0);
		  
		    sb->ArgStack->setCurrentWidget( argSetGeoLocationINDI);
		  
		    ok = !sf->argVal(0).isEmpty();
		    if (ok) lo = sf->argVal(0).toDouble(&ok);
		    else argSetGeoLocationINDI->longBox->clear();
		    if (ok) { argSetGeoLocationINDI->longBox->showInDegrees( dms(lo) ); }

		    ok = !sf->argVal(1).isEmpty();
		    if (ok) la = sf->argVal(1).toDouble(&ok);
		    else argSetGeoLocationINDI->latBox->clear();
		    if (ok) argSetGeoLocationINDI->latBox->showInDegrees( dms(la) );
		    
		  }
		  else if (sf->name() == "startINDIExposure") {
		    int t(0);
		    bool ok(false);
		  
		    sb->ArgStack->setCurrentWidget( argStartExposureINDI);
		  
		    t = sf->argVal(0).toInt(&ok);
		    if (ok) argStartExposureINDI->timeOut->setValue(t);
		    else argStartExposureINDI->timeOut->setValue(0);
		    
		  }
		  else if (sf->name() == "setINDIUTC") {
		    sb->ArgStack->setCurrentWidget( argSetUTCINDI);
		  
		    argSetUTCINDI->UTC->setText(sf->argVal(0));
		    
		  }
		  else if (sf->name() == "setINDIScopeAction") {
		    sb->ArgStack->setCurrentWidget( argSetScopeActionINDI);
		    bool itemSet(false);
		  
		    for (int i=0; i < argSetScopeActionINDI->actionCombo->count(); i++)
		    {
		      if (argSetScopeActionINDI->actionCombo->text(i) == sf->argVal(0))
		      {
			argSetScopeActionINDI->actionCombo->setCurrentItem(i);
			itemSet = true;
			break;
		      }
		    }
		  
		    if (!itemSet) argSetScopeActionINDI->actionCombo->setCurrentItem(0);
		  
		  }
		  else if (sf->name() == "setINDIFrameType") {
		    sb->ArgStack->setCurrentWidget( argSetFrameTypeINDI);
		    bool itemSet(false);
		  
		    for (int i=0; i < argSetFrameTypeINDI->typeCombo->count(); i++)
		    {
		      if (argSetFrameTypeINDI->typeCombo->text(i) == sf->argVal(0))
		      {
			argSetFrameTypeINDI->typeCombo->setCurrentItem(i);
			itemSet = true;
			break;
		      }
		    }
		  
		    if (!itemSet) argSetFrameTypeINDI->typeCombo->setCurrentItem(0);
		  
		  }
		  else if (sf->name() == "setINDICCDTemp") {
		    int t(0);
		    bool ok(false);
		  
		    sb->ArgStack->setCurrentWidget( argSetCCDTempINDI);
		  
		    t = sf->argVal(0).toInt(&ok);
		    if (ok) argSetCCDTempINDI->temp->setValue(t);
		    else argSetCCDTempINDI->temp->setValue(0);
		  
		  }
		  else if (sf->name() == "setINDIFilterNum") {
		    int t(0);
		    bool ok(false);
		  
		    sb->ArgStack->setCurrentWidget( argSetFilterNumINDI);
		  
		    t = sf->argVal(0).toInt(&ok);
		    if (ok) argSetFilterNumINDI->filter_num->setValue(t);
		    else argSetFilterNumINDI->filter_num->setValue(0);
		  
		  }
	}
}

void ScriptBuilder::slotShowDoc() {
  ScriptFunction *sc = NULL, *found= NULL;
  QTreeWidgetItem *currentItem = sb->FunctionTree->currentItem();
	
  if ( currentItem == NULL || currentItem->parent() == 0)
    return;
	
  foreach ( sc, KStarsFunctionList )
    if (sc->prototype() == currentItem->text(0))
	{
		found = sc;
      		break;
	}
  
  if (found == NULL)
  {
    foreach (sc, INDIFunctionList )
      if (sc->prototype() == currentItem->text(0))
	{
		found = sc;
		break;
	}
  }
	
  if (found == NULL)
  {
    sb->AddButton->setEnabled( false );
    kWarning() << i18n( "Function index out of bounds." ) << endl;
    return;
  }

    sb->AddButton->setEnabled( true );
    sb->FuncDoc->setText( found->description() );
}

//Slots for Arg Widgets
void ScriptBuilder::slotFindCity() {
	LocationDialog ld( ks );

	if ( ld.exec() == QDialog::Accepted ) {
		if ( ld.selectedCity() ) {
			// set new location names
			argSetGeoLocation->CityName->setText( ld.selectedCityName() );
			argSetGeoLocation->ProvinceName->setText( ld.selectedProvinceName() );
			argSetGeoLocation->CountryName->setText( ld.selectedCountryName() );

			ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];
			if ( sf->name() == "setGeoLocation" ) {
				setUnsavedChanges( true );

				sf->setArg( 0, ld.selectedCityName() );
				sf->setArg( 1, ld.selectedProvinceName() );
				sf->setArg( 2, ld.selectedCountryName() );
			} else {
				kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setGeoLocation" ) << endl;
			}
		}
	}
}

void ScriptBuilder::slotFindObject() {
	FindDialog fd( ks );

	if ( fd.exec() == QDialog::Accepted && fd.currentItem() ) {
		setUnsavedChanges( true );

		argLookToward->FocusEdit->setCurrentText( fd.currentItem()->name() );
	}
}

void ScriptBuilder::slotINDIFindObject() {
  FindDialog fd( ks );

  if ( fd.exec() == QDialog::Accepted && fd.currentItem() ) {
    setUnsavedChanges( true );

    argSetTargetNameINDI->targetName->setText( fd.currentItem()->name() );
  }
}

void ScriptBuilder::slotINDIWaitCheck(bool /*toggleState*/)
{
  
   setUnsavedChanges(true);  
  
}

void ScriptBuilder::slotShowOptions() {
	//Show tree-view of view options
	if ( otv->exec() == QDialog::Accepted ) {
		argChangeViewOption->OptionName->setCurrentItem( otv->optionsList()->currentItem()->text(0) );
	}
}

void ScriptBuilder::slotLookToward() {
	ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

	if ( sf->name() == "lookTowards" ) {
		setUnsavedChanges( true );

		sf->setArg( 0, argLookToward->FocusEdit->currentText() );
		sf->setValid(true);
	} else {
		kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "lookTowards" ) << endl;
	}
}

void ScriptBuilder::slotRa() {
	ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

	if ( sf->name() == "setRaDec" ) {
		//do nothing if box is blank (because we could be clearing boxes while switcing argWidgets)
		if ( argSetRaDec->RABox->text().isEmpty() ) return;

		bool ok(false);
		dms ra = argSetRaDec->RABox->createDms(false, &ok);
		if ( ok ) {
			setUnsavedChanges( true );

			sf->setArg( 0, QString( "%1" ).arg( ra.Hours() ) );
			if ( ! sf->argVal(1).isEmpty() ) sf->setValid( true );

		} else {
			sf->setArg( 0, QString() );
			sf->setValid( false );
		}
	} else {
		kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setRaDec" ) << endl;
	}
}

void ScriptBuilder::slotDec() {
	ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

	if ( sf->name() == "setRaDec" ) {
		//do nothing if box is blank (because we could be clearing boxes while switcing argWidgets)
		if ( argSetRaDec->DecBox->text().isEmpty() ) return;

		bool ok(false);
		dms dec = argSetRaDec->DecBox->createDms(true, &ok);
		if ( ok ) {
			setUnsavedChanges( true );

			sf->setArg( 1, QString( "%1" ).arg( dec.Degrees() ) );
			if ( ! sf->argVal(0).isEmpty() ) sf->setValid( true );

		} else {
			sf->setArg( 1, QString() );
			sf->setValid( false );
		}
	} else {
		kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setRaDec" ) << endl;
	}
}

void ScriptBuilder::slotAz() {
	ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

	if ( sf->name() == "setAltAz" ) {
		//do nothing if box is blank (because we could be clearing boxes while switcing argWidgets)
		if ( argSetAltAz->AzBox->text().isEmpty() ) return;

		bool ok(false);
		dms az = argSetAltAz->AzBox->createDms(true, &ok);
		if ( ok ) {
			setUnsavedChanges( true );
			sf->setArg( 1, QString( "%1" ).arg( az.Degrees() ) );
			if ( ! sf->argVal(0).isEmpty() ) sf->setValid( true );
		} else {
			sf->setArg( 1, QString() );
			sf->setValid( false );
		}
	} else {
		kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setAltAz" ) << endl;
	}
}

void ScriptBuilder::slotAlt() {
	ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

	if ( sf->name() == "setAltAz" ) {
		//do nothing if box is blank (because we could be clearing boxes while switcing argWidgets)
		if ( argSetAltAz->AltBox->text().isEmpty() ) return;

		bool ok(false);
		dms alt = argSetAltAz->AltBox->createDms(true, &ok);
		if ( ok ) {
			setUnsavedChanges( true );

			sf->setArg( 0, QString( "%1" ).arg( alt.Degrees() ) );
			if ( ! sf->argVal(1).isEmpty() ) sf->setValid( true );
		} else {
			sf->setArg( 0, QString() );
			sf->setValid( false );
		}
	} else {
		kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setAltAz" ) << endl;
	}
}

void ScriptBuilder::slotChangeDate() {
	ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

	if ( sf->name() == "setLocalTime" ) {
		setUnsavedChanges( true );

		ExtDate date = argSetLocalTime->DateBox->date();

		sf->setArg( 0, QString( "%1" ).arg( date.year()   ) );
		sf->setArg( 1, QString( "%1" ).arg( date.month()  ) );
		sf->setArg( 2, QString( "%1" ).arg( date.day()    ) );
		if ( ! sf->argVal(3).isEmpty() ) sf->setValid( true );
	} else {
		kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setLocalTime" ) << endl;
	}
}

void ScriptBuilder::slotChangeTime() {
	ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

	if ( sf->name() == "setLocalTime" ) {
		setUnsavedChanges( true );

		QTime time = argSetLocalTime->TimeBox->time();

		sf->setArg( 3, QString( "%1" ).arg( time.hour()   ) );
		sf->setArg( 4, QString( "%1" ).arg( time.minute() ) );
		sf->setArg( 5, QString( "%1" ).arg( time.second() ) );
		if ( ! sf->argVal(0).isEmpty() ) sf->setValid( true );
	} else {
		kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setLocalTime" ) << endl;
	}
}

void ScriptBuilder::slotWaitFor() {
	ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

	if ( sf->name() == "waitFor" ) {
		bool ok(false);
		int delay = argWaitFor->DelayBox->text().toInt( &ok );

		if ( ok ) {
			setUnsavedChanges( true );

			sf->setArg( 0, QString( "%1" ).arg( delay ) );
			sf->setValid( true );
		} else {
			sf->setValid( false );
		}
	} else {
		kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "waitFor" ) << endl;
	}
}

void ScriptBuilder::slotWaitForKey() {
	ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

	if ( sf->name() == "waitForKey" ) {
		QString sKey = argWaitForKey->WaitKeyEdit->text().trimmed();

		//DCOP script can only use single keystrokes; make sure entry is either one character,
		//or the word 'space'
		if ( sKey.length() == 1 || sKey == "space" ) {
			setUnsavedChanges( true );

			sf->setArg( 0, sKey );
			sf->setValid( true );
		} else {
			sf->setValid( false );
		}
	} else {
		kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "waitForKey" ) << endl;
	}
}

void ScriptBuilder::slotTracking() {
	ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

	if ( sf->name() == "setTracking" ) {
		setUnsavedChanges( true );

		sf->setArg( 0, ( argSetTracking->CheckTrack->isChecked() ? i18n( "true" ) : i18n( "false" ) ) );
		sf->setValid( true );
	} else {
		kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setTracking" ) << endl;
	}
}

void ScriptBuilder::slotViewOption() {
	ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

	if ( sf->name() == "changeViewOption" ) {
		if ( argChangeViewOption->OptionName->currentItem() >= 0
				&& argChangeViewOption->OptionValue->text().length() ) {
			setUnsavedChanges( true );

			sf->setArg( 0, argChangeViewOption->OptionName->currentText() );
			sf->setArg( 1, argChangeViewOption->OptionValue->text() );
			sf->setValid( true );
		} else {
			sf->setValid( false );
		}
	} else {
		kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "changeViewOption" ) << endl;
	}
}

void ScriptBuilder::slotChangeCity() {
	ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

	if ( sf->name() == "setGeoLocation" ) {
		QString city =     argSetGeoLocation->CityName->text();

		if ( city.length() ) {
			setUnsavedChanges( true );

			sf->setArg( 0, city );
			if ( sf->argVal(2).length() ) sf->setValid( true );
		} else {
			sf->setArg( 0, QString() );
			sf->setValid( false );
		}
	} else {
		kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setGeoLocation" ) << endl;
	}
}

void ScriptBuilder::slotChangeProvince() {
	ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

	if ( sf->name() == "setGeoLocation" ) {
		QString province = argSetGeoLocation->ProvinceName->text();

		if ( province.length() ) {
			setUnsavedChanges( true );

			sf->setArg( 1, province );
			if ( sf->argVal(0).length() && sf->argVal(2).length() ) sf->setValid( true );
		} else {
			sf->setArg( 1, QString() );
			//might not be invalid
		}
	} else {
		kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setGeoLocation" ) << endl;
	}
}

void ScriptBuilder::slotChangeCountry() {
	ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

	if ( sf->name() == "setGeoLocation" ) {
		QString country =  argSetGeoLocation->CountryName->text();

		if ( country.length() ) {
			setUnsavedChanges( true );

			sf->setArg( 2, country );
			if ( sf->argVal(0).length() ) sf->setValid( true );
		} else {
			sf->setArg( 2, QString() );
			sf->setValid( false );
		}
	} else {
		kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setGeoLocation" ) << endl;
	}
}

void ScriptBuilder::slotTimeScale() {
	ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

	if ( sf->name() == "setClockScale" ) {
		setUnsavedChanges( true );

		sf->setArg( 0, QString( "%1" ).arg( argTimeScale->TimeScale->tsbox()->timeScale() ) );
		sf->setValid( true );
	} else {
		kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setClockScale" ) << endl;
	}
}

void ScriptBuilder::slotZoom() {
	ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

	if ( sf->name() == "zoom" ) {
		setUnsavedChanges( true );

		bool ok(false);
		argZoom->ZoomBox->text().toDouble(&ok);
		if ( ok ) {
			sf->setArg( 0, argZoom->ZoomBox->text() );
			sf->setValid( true );
		}
	} else {
		kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "zoom" ) << endl;
	}
}

void ScriptBuilder::slotExportImage() {
	ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

	if ( sf->name() == "exportImage" ) {
		setUnsavedChanges( true );
		
		sf->setArg( 0, argExportImage->ExportFileName->url() );
		sf->setArg( 1, QString("%1").arg( argExportImage->ExportWidth->value() ) );
		sf->setArg( 2, QString("%1").arg( argExportImage->ExportHeight->value() ) );
		sf->setValid( true );
	} else {
		kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "exportImage" ) << endl;
	}
}

void ScriptBuilder::slotPrintImage() {
	ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

	if ( sf->name() == "printImage" ) {
		setUnsavedChanges( true );
		
		sf->setArg( 0, ( argPrintImage->UsePrintDialog->isChecked() ? i18n( "true" ) : i18n( "false" ) ) );
		sf->setArg( 1, ( argPrintImage->UseChartColors->isChecked() ? i18n( "true" ) : i18n( "false" ) ) );
		sf->setValid( true );
	} else {
		kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "exportImage" ) << endl;
	}
}

void ScriptBuilder::slotChangeColorName() {
	ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

	if ( sf->name() == "setColor" ) {
		setUnsavedChanges( true );
		
		argSetColor->ColorValue->setColor( ks->data()->colorScheme()->colorAt( argSetColor->ColorName->currentItem() ) );
		sf->setArg( 0, ks->data()->colorScheme()->keyAt( argSetColor->ColorName->currentItem() ) );
		QString cname( argSetColor->ColorValue->color().name() );
		if ( cname.at(0) == '#' ) cname = "\\" + cname; //prepend a "\" so bash doesn't think we have a comment
		sf->setArg( 1, cname );
		sf->setValid( true );
	} else {
		kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setColor" ) << endl;
	}
}

void ScriptBuilder::slotChangeColor() {
	ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

	if ( sf->name() == "setColor" ) {
		setUnsavedChanges( true );
		
		sf->setArg( 0, ks->data()->colorScheme()->keyAt( argSetColor->ColorName->currentItem() ) );
		QString cname( argSetColor->ColorValue->color().name() );
		if ( cname.at(0) == '#' ) cname = "\\" + cname; //prepend a "\" so bash doesn't think we have a comment
		sf->setArg( 1, cname );
		sf->setValid( true );
	} else {
		kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setColor" ) << endl;
	}
}

void ScriptBuilder::slotLoadColorScheme() {
	ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

	if ( sf->name() == "loadColorScheme" ) {
		setUnsavedChanges( true );
		
		sf->setArg( 0, "\"" + argLoadColorScheme->SchemeList->currentText() + "\"" );
		sf->setValid( true );
	} else {
		kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "loadColorScheme" ) << endl;
	}
}

void ScriptBuilder::slotClose()
{
	saveWarning();

	if ( !UnsavedChanges ) 
		close();
	
}

void ScriptBuilder::slotINDIStartDeviceName()
{
  ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

  if ( sf->name() == "startINDI" )
  {
    setUnsavedChanges( true );
    
    sf->setArg(0, argStartINDI->deviceName->text());
    sf->setArg(1, argStartINDI->LocalButton->isChecked() ? "true" : "false");
    sf->setValid(true);
  }
  else
  {
    kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "startINDI" ) << endl;
  }
  
}

void ScriptBuilder::slotINDIStartDeviceMode()
{
  
  ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

  if ( sf->name() == "startINDI" )
  {
    setUnsavedChanges( true );
    
    sf->setArg(1, argStartINDI->LocalButton->isChecked() ? "true" : "false");
    sf->setValid(true);
  }
  else
  {
    kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "startINDI" ) << endl;
  }
  
}

void ScriptBuilder::slotINDISetDevice()
{

ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

  if ( sf->name() == "setINDIDevice" )
  {
    setUnsavedChanges( true );
    
    sf->setArg(0, argSetDeviceINDI->deviceName->text());
    sf->setValid(true);
  }
  else
  {
    kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "startINDI" ) << endl;
  }
}

void ScriptBuilder::slotINDIShutdown()
{
  
  ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

  if ( sf->name() == "shutdownINDI" )
  {
    if (argShutdownINDI->deviceName->text().isEmpty())
    {
      sf->setValid(false);
      return;
    }
    
    if (sf->argVal(0) != argShutdownINDI->deviceName->text())
    	setUnsavedChanges( true );
    
    sf->setArg(0, argShutdownINDI->deviceName->text());
    sf->setValid(true);
  }
  else
  {
    kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "shutdownINDI" ) << endl;
  }
  
}

void ScriptBuilder::slotINDISwitchDeviceConnection()
{
  
  ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

  if ( sf->name() == "switchINDI" )
  {
    
    if (sf->argVal(0) != (argSwitchINDI->OnButton->isChecked() ? "true" : "false"))
    setUnsavedChanges( true );
    
    sf->setArg(0, argSwitchINDI->OnButton->isChecked() ? "true" : "false");
    sf->setValid(true);
  }
  else
  {
    kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "switchINDI" ) << endl;
  }
  
}

void ScriptBuilder::slotINDISetPortDevicePort()
{
  ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

  if ( sf->name() == "setINDIPort" )
  {
    
    if (argSetPortINDI->devicePort->text().isEmpty())
    {
      sf->setValid(false);
      return;
    }
    
    if (sf->argVal(0) != argSetPortINDI->devicePort->text())
    	setUnsavedChanges( true );
    
    sf->setArg(0, argSetPortINDI->devicePort->text());
    sf->setValid(true);
  }
  else
  {
    kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setINDIPort" ) << endl;
  }
  
}

void ScriptBuilder::slotINDISetTargetCoordDeviceRA()
{
  ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

  if ( sf->name() == "setINDITargetCoord" ) {
    //do nothing if box is blank (because we could be clearing boxes while switcing argWidgets)
    if ( argSetTargetCoordINDI->RABox->text().isEmpty() )
    {
      sf->setValid(false);
      return;
    }

    bool ok(false);
    dms ra = argSetTargetCoordINDI->RABox->createDms(false, &ok);
    if ( ok )
    {
      
      if (sf->argVal(0) != QString( "%1" ).arg( ra.Hours() ))
      	setUnsavedChanges( true );

      sf->setArg( 0, QString( "%1" ).arg( ra.Hours() ) );
      if ( ( ! sf->argVal(1).isEmpty() ))
	 sf->setValid( true );
      else
	 sf->setValid(false);

    } else
    {
      sf->setArg( 0, QString() );
      sf->setValid( false );
    }
  } else {
    kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setINDITargetCoord" ) << endl;
  }
  
}

void ScriptBuilder::slotINDISetTargetCoordDeviceDEC()
{
  ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

  if ( sf->name() == "setINDITargetCoord" ) {
   //do nothing if box is blank (because we could be clearing boxes while switcing argWidgets)
    if ( argSetTargetCoordINDI->DecBox->text().isEmpty() )
    {
      sf->setValid(false);
      return;
    }

    bool ok(false);
    dms dec = argSetTargetCoordINDI->DecBox->createDms(true, &ok);
    if ( ok )
    {
      
      if (sf->argVal(1) != QString( "%1" ).arg( dec.Degrees() ))
      	setUnsavedChanges( true );

      sf->setArg( 1, QString( "%1" ).arg( dec.Degrees() ) );
      if ( ( ! sf->argVal(0).isEmpty() ))
	 sf->setValid( true );
      else
	 sf->setValid(false);
      
    } else
    {
      sf->setArg( 1, QString() );
      sf->setValid( false );
    }
  } else {
    kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setINDITargetCoord" ) << endl;
  }
  
}

void ScriptBuilder::slotINDISetTargetNameTargetName()
{
  
  ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

  if ( sf->name() == "setINDITargetName" )
  {
    if (argSetTargetNameINDI->targetName->text().isEmpty())
    {
      sf->setValid(false);
      return;
    }
    
    if (sf->argVal(0) != argSetTargetNameINDI->targetName->text())
    	setUnsavedChanges( true );
    
    sf->setArg(0, argSetTargetNameINDI->targetName->text());
    sf->setValid(true);
  }
  else
  {
    kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setINDITargetName" ) << endl;
  }
  
}

void ScriptBuilder::slotINDISetActionName()
{
  ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

  if ( sf->name() == "setINDIAction" )
  {
    if (argSetActionINDI->actionName->text().isEmpty())
    {
      sf->setValid(false);
      return;
    }
    
    if (sf->argVal(0) != argSetActionINDI->actionName->text())
    	setUnsavedChanges( true );
    
    sf->setArg(0, argSetActionINDI->actionName->text());
    sf->setValid(true);
  }
  else
  {
    kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setINDIAction") << endl;
  }

}

void ScriptBuilder::slotINDIWaitForActionName()
{
  ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

  if ( sf->name() == "waitForINDIAction" )
  {
    if (argWaitForActionINDI->actionName->text().isEmpty())
    {
      sf->setValid(false);
      return;
    }
    
    if (sf->argVal(0) != argWaitForActionINDI->actionName->text())
    	setUnsavedChanges( true );
    
    sf->setArg(0, argWaitForActionINDI->actionName->text());
    sf->setValid(true);
  }
  else
  {
    kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "waitForINDIAction") << endl;
  }
  
}

void ScriptBuilder::slotINDISetFocusSpeed()
{
  ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

  if ( sf->name() == "setINDIFocusSpeed" )
  {
    
    if (sf->argVal(0).toInt() != argSetFocusSpeedINDI->speedIN->value())
    	setUnsavedChanges( true );
    
    sf->setArg(0, QString("%1").arg(argSetFocusSpeedINDI->speedIN->value()));
    sf->setValid(true);
  }
  else
  {
    kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setINDIFocusSpeed") << endl;
  }
  
}

void ScriptBuilder::slotINDIStartFocusDirection()
{
  ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

  if ( sf->name() == "startINDIFocus" )
  {
    if (sf->argVal(0) != argStartFocusINDI->directionCombo->currentText())
    	setUnsavedChanges( true );
    
    sf->setArg(0, argStartFocusINDI->directionCombo->currentText());
    sf->setValid(true);
  }
  else
  {
    kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "startINDIFocus") << endl;
  }
  
}

void ScriptBuilder::slotINDISetFocusTimeout()
{
  ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

  if ( sf->name() == "setINDIFocusTimeout" )
  {
    if (sf->argVal(0).toInt() != argSetFocusTimeoutINDI->timeOut->value())
    	setUnsavedChanges( true );
    
    sf->setArg(0, QString("%1").arg(argSetFocusTimeoutINDI->timeOut->value()));
    sf->setValid(true);
  }
  else
  {
    kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setINDIFocusTimeout") << endl;
  }
  
}

void ScriptBuilder::slotINDISetGeoLocationDeviceLong()
{
  ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

  if ( sf->name() == "setINDIGeoLocation" ) {
   //do nothing if box is blank (because we could be clearing boxes while switcing argWidgets)
    if ( argSetGeoLocationINDI->longBox->text().isEmpty())
    {
      sf->setValid(false);
      return;
    }

    bool ok(false);
    dms longitude = argSetGeoLocationINDI->longBox->createDms(true, &ok);
    if ( ok ) {
      
      if (sf->argVal(0) != QString( "%1" ).arg( longitude.Degrees()))
      	setUnsavedChanges( true );

      sf->setArg( 0, QString( "%1" ).arg( longitude.Degrees() ) );
      if ( ! sf->argVal(1).isEmpty() )
	 sf->setValid( true );
      else
	 sf->setValid(false);

    } else
      {
      sf->setArg( 0, QString() );
      sf->setValid( false );
    }
  } else {
    kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setINDIGeoLocation" ) << endl;
  }
  
}

void ScriptBuilder::slotINDISetGeoLocationDeviceLat()
{
  ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

  if ( sf->name() == "setINDIGeoLocation" ) {
     //do nothing if box is blank (because we could be clearing boxes while switcing argWidgets)
    if ( argSetGeoLocationINDI->latBox->text().isEmpty() )
    {
      sf->setValid(false);
      return;
    }

    bool ok(false);
    dms latitude = argSetGeoLocationINDI->latBox->createDms(true, &ok);
    if ( ok ) {
      
      if (sf->argVal(1) != QString( "%1" ).arg( latitude.Degrees()))
      	setUnsavedChanges( true );

      sf->setArg( 1, QString( "%1" ).arg( latitude.Degrees() ) );
      if ( ! sf->argVal(0).isEmpty() )
	 sf->setValid( true );
      else
	 sf->setValid(false);

    } else 
    {
      sf->setArg( 1, QString() );
      sf->setValid( false );
    }
  } else {
    kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setINDIGeoLocation" ) << endl;
  }
  
}

void ScriptBuilder::slotINDIStartExposureTimeout()
{
  ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

  if ( sf->name() == "startINDIExposure" )
  {
    
    if (sf->argVal(0).toInt() != argStartExposureINDI->timeOut->value())
    	setUnsavedChanges( true );
    
    sf->setArg(0, QString("%1").arg(argStartExposureINDI->timeOut->value()));
    sf->setValid(true);
  }
  else
  {
    kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "startINDIExposure") << endl;
  }
  
}

void ScriptBuilder::slotINDISetUTC()
{
  ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

  if ( sf->name() == "setINDIUTC" )
  {
    
    if (argSetUTCINDI->UTC->text().isEmpty())
    {
      sf->setValid(false);
      return;
    }
    
    if (sf->argVal(0) != argSetUTCINDI->UTC->text())
    setUnsavedChanges( true );
    
    sf->setArg(0, argSetUTCINDI->UTC->text());
    sf->setValid(true);
  }
  else
  {
    kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setINDIUTC" ) << endl;
  }
  
}

void ScriptBuilder::slotINDISetScopeAction()
{
  ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

  if ( sf->name() == "setINDIScopeAction" )
  {
    
    if (sf->argVal(0) != argSetScopeActionINDI->actionCombo->currentText())
    	setUnsavedChanges( true );
    
    sf->setArg(0, argSetScopeActionINDI->actionCombo->currentText());
    sf->setINDIProperty("CHECK");
    sf->setValid(true);
  }
  else
  {
    kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setINDIScopeAction") << endl;
  }
  
}

void ScriptBuilder::slotINDISetFrameType()
{
  ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

  if ( sf->name() == "setINDIFrameType" )
  {
    
    if (sf->argVal(0) != argSetFrameTypeINDI->typeCombo->currentText())
    	setUnsavedChanges( true );
    
    sf->setArg(0, argSetFrameTypeINDI->typeCombo->currentText());
    sf->setValid(true);
  }
  else
  {
    kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setINDIFrameType") << endl;
  }
  
}

void ScriptBuilder::slotINDISetCCDTemp()
{
  ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

  if ( sf->name() == "setINDICCDTemp" )
  {
    
    if (sf->argVal(0).toInt() != argSetCCDTempINDI->temp->value())
    	setUnsavedChanges( true );
    
    sf->setArg(0, QString("%1").arg(argSetCCDTempINDI->temp->value()));
    sf->setValid(true);
  }
  else
  {
    kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setINDICCDTemp") << endl;
  }
  
}

void ScriptBuilder::slotINDISetFilterNum()
{

 ScriptFunction *sf = ScriptList[ sb->ScriptListBox->currentRow() ];

  if ( sf->name() == "setINDIFilterNum" )
  {
    
    if (sf->argVal(0).toInt() != argSetFilterNumINDI->filter_num->value())
    	setUnsavedChanges( true );
    
    sf->setArg(0, QString("%1").arg(argSetFilterNumINDI->filter_num->value()));
    sf->setValid(true);
  }
  else
  {
    kWarning() << i18n( "Mismatch between function and Arg widget (expected %1.)" ).arg( "setINDIFilterNum") << endl;
  }


}
	
#include "scriptbuilder.moc"
