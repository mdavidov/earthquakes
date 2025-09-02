// EarthquakeMainWindow.cpp
#include "EarthquakeMainWindow.h"
#include "SpatialUtils.h"
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QSizePolicy>
#include <QtCore/QStandardPaths>
#include <QtCore/QDir>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QTextStream>
#include <QtCore/QDebug>

EarthquakeMainWindow::EarthquakeMainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_mapWidget(nullptr)
    , m_networkManager(nullptr)
    , m_currentReply(nullptr)
    , m_refreshTimer(nullptr)
    , m_alertTimer(nullptr)
    , m_trayIcon(nullptr)
    , m_alertSound(nullptr)
    , m_refreshIntervalMinutes(5)
    , m_alertsEnabled(true)
    , m_soundEnabled(true)
    , m_alertThreshold(5.0)
    , m_startMinimized(false)
{
    setupUI();
    setupMenuBar();
    setupToolBar();
    setupStatusBar();
    setupDockWidgets();
    setupSystemTray();
    connectSignals();
    loadSettings();
    
    // Initialize network manager
    m_networkManager = new QNetworkAccessManager(this);
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &EarthquakeMainWindow::onDataReceived);
    
    // Setup timers
    m_refreshTimer = new QTimer(this);
    connect(m_refreshTimer, &QTimer::timeout, this, &EarthquakeMainWindow::fetchEarthquakeData);
    m_refreshTimer->start(m_refreshIntervalMinutes * 60 * 1000);
    
    m_alertTimer = new QTimer(this);
    connect(m_alertTimer, &QTimer::timeout, this, &EarthquakeMainWindow::checkForAlerts);
    m_alertTimer->start(30000); // Check every 30 seconds
    
    // Load initial data
    fetchEarthquakeData();
    
    setWindowTitle("Earthquake Alert System v2.1");
    resize(1400, 900);
}

EarthquakeMainWindow::~EarthquakeMainWindow()
{
    saveSettings();
    if (m_currentReply) {
        m_currentReply->abort();
    }
}

void EarthquakeMainWindow::setupUI()
{
    // Central widget with splitter layout
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    setCentralWidget(m_mainSplitter);
    
    // Map widget (left side, main area)
    m_mapWidget = new EarthquakeMapWidget;
    m_mapWidget->setMinimumSize(600, 400);
    m_mainSplitter->addWidget(m_mapWidget);
    
    // Right side splitter for controls and data
    m_rightSplitter = new QSplitter(Qt::Vertical);
    m_mainSplitter->addWidget(m_rightSplitter);
    
    // Set splitter proportions (map takes 70%, controls take 30%)
    m_mainSplitter->setSizes({700, 300});
    
    setupControlPanels();
    setupEarthquakeList();
    setupDetailsPane();
}

void EarthquakeMainWindow::setupControlPanels()
{
    auto *controlsWidget = new QWidget;
    auto *layout = new QVBoxLayout(controlsWidget);
    
    // Map Controls Group
    m_mapControlsGroup = new QGroupBox("Map Controls");
    auto *mapLayout = new QGridLayout(m_mapControlsGroup);
    
    mapLayout->addWidget(new QLabel("Latitude:"), 0, 0);
    m_latSpinBox = new QDoubleSpinBox;
    m_latSpinBox->setRange(-90.0, 90.0);
    m_latSpinBox->setDecimals(4);
    m_latSpinBox->setValue(0.0);
    mapLayout->addWidget(m_latSpinBox, 0, 1);
    
    mapLayout->addWidget(new QLabel("Longitude:"), 1, 0);
    m_lonSpinBox = new QDoubleSpinBox;
    m_lonSpinBox->setRange(-180.0, 180.0);
    m_lonSpinBox->setDecimals(4);
    m_lonSpinBox->setValue(0.0);
    mapLayout->addWidget(m_lonSpinBox, 1, 1);
    
    mapLayout->addWidget(new QLabel("Zoom:"), 2, 0);
    m_zoomSlider = new QSlider(Qt::Horizontal);
    m_zoomSlider->setRange(10, 200); // 0.1x to 20.0x zoom
    m_zoomSlider->setValue(100);
    mapLayout->addWidget(m_zoomSlider, 2, 1);
    
    m_showGridCheckBox = new QCheckBox("Show Grid");
    m_showGridCheckBox->setChecked(true);
    mapLayout->addWidget(m_showGridCheckBox, 3, 0);
    
    m_showLegendCheckBox = new QCheckBox("Show Legend");
    m_showLegendCheckBox->setChecked(true);
    mapLayout->addWidget(m_showLegendCheckBox, 3, 1);
    
    m_resetViewBtn = new QPushButton("Reset View");
    mapLayout->addWidget(m_resetViewBtn, 4, 0);
    
    m_fullscreenBtn = new QPushButton("Fullscreen");
    mapLayout->addWidget(m_fullscreenBtn, 4, 1);
    
    layout->addWidget(m_mapControlsGroup);
    
    // Filter Controls Group
    m_filterGroup = new QGroupBox("Filters");
    auto *filterLayout = new QGridLayout(m_filterGroup);
    
    filterLayout->addWidget(new QLabel("Min Magnitude:"), 0, 0);
    m_minMagnitudeSpinBox = new QDoubleSpinBox;
    m_minMagnitudeSpinBox->setRange(0.0, 10.0);
    m_minMagnitudeSpinBox->setDecimals(1);
    m_minMagnitudeSpinBox->setValue(2.0);
    filterLayout->addWidget(m_minMagnitudeSpinBox, 0, 1);
    
    filterLayout->addWidget(new QLabel("Max Magnitude:"), 1, 0);
    m_maxMagnitudeSpinBox = new QDoubleSpinBox;
    m_maxMagnitudeSpinBox->setRange(0.0, 10.0);
    m_maxMagnitudeSpinBox->setDecimals(1);
    m_maxMagnitudeSpinBox->setValue(10.0);
    filterLayout->addWidget(m_maxMagnitudeSpinBox, 1, 1);
    
    filterLayout->addWidget(new QLabel("Max Age (hours):"), 2, 0);
    m_maxAgeHoursSpinBox = new QSpinBox;
    m_maxAgeHoursSpinBox->setRange(1, 8760); // 1 hour to 1 year
    m_maxAgeHoursSpinBox->setValue(168); // 1 week default
    filterLayout->addWidget(m_maxAgeHoursSpinBox, 2, 1);
    
    filterLayout->addWidget(new QLabel("Alert Level:"), 3, 0);
    m_alertLevelCombo = new QComboBox;
    m_alertLevelCombo->addItems({"All", "Info+", "Minor+", "Moderate+", "Major+", "Critical"});
    filterLayout->addWidget(m_alertLevelCombo, 3, 1);
    
    m_recentOnlyCheckBox = new QCheckBox("Recent Only (24h)");
    filterLayout->addWidget(m_recentOnlyCheckBox, 4, 0, 1, 2);
    
    layout->addWidget(m_filterGroup);
    
    // Alert Settings Group
    m_alertGroup = new QGroupBox("Alert Settings");
    auto *alertLayout = new QGridLayout(m_alertGroup);
    
    m_alertsEnabledCheckBox = new QCheckBox("Alerts Enabled");
    m_alertsEnabledCheckBox->setChecked(true);
    alertLayout->addWidget(m_alertsEnabledCheckBox, 0, 0, 1, 2);
    
    m_soundEnabledCheckBox = new QCheckBox("Sound Alerts");
    m_soundEnabledCheckBox->setChecked(true);
    alertLayout->addWidget(m_soundEnabledCheckBox, 1, 0, 1, 2);
    
    alertLayout->addWidget(new QLabel("Alert Threshold:"), 2, 0);
    m_alertThresholdCombo = new QComboBox;
    m_alertThresholdCombo->addItems({"3.0+", "4.0+", "5.0+", "6.0+", "7.0+"});
    m_alertThresholdCombo->setCurrentText("5.0+");
    alertLayout->addWidget(m_alertThresholdCombo, 2, 1);
    
    alertLayout->addWidget(new QLabel("Alert Status:"), 3, 0);
    m_alertIndicator = new QProgressBar;
    m_alertIndicator->setRange(0, 4);
    m_alertIndicator->setValue(0);
    m_alertIndicator->setFormat("Normal");
    alertLayout->addWidget(m_alertIndicator, 3, 1);
    
    layout->addWidget(m_alertGroup);
    
    // Statistics Group
    auto *statsGroup = new QGroupBox("Statistics");
    auto *statsLayout = new QVBoxLayout(statsGroup);
    
    m_totalEarthquakesLabel = new QLabel("Total: 0");
    m_recentEarthquakesLabel = new QLabel("Last 24h: 0");
    m_highestMagnitudeLabel = new QLabel("Highest: N/A");
    m_lastUpdateLabel = new QLabel("Last Update: Never");
    
    statsLayout->addWidget(m_totalEarthquakesLabel);
    statsLayout->addWidget(m_recentEarthquakesLabel);
    statsLayout->addWidget(m_highestMagnitudeLabel);
    statsLayout->addWidget(m_lastUpdateLabel);
    
    layout->addWidget(statsGroup);
    layout->addStretch();
    
    m_rightSplitter->addWidget(controlsWidget);
}

void EarthquakeMainWindow::setupEarthquakeList()
{
    m_earthquakeTable = new QTableWidget;
    m_earthquakeTable->setColumnCount(6);
    QStringList headers = {"Time", "Location", "Magnitude", "Depth", "Alert", "Distance"};
    m_earthquakeTable->setHorizontalHeaderLabels(headers);
    
    // Configure table
    m_earthquakeTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_earthquakeTable->setAlternatingRowColors(true);
    m_earthquakeTable->setSortingEnabled(true);
    m_earthquakeTable->horizontalHeader()->setStretchLastSection(true);
    m_earthquakeTable->verticalHeader()->setVisible(false);
    
    // Set column widths
    m_earthquakeTable->setColumnWidth(0, 120); // Time
    m_earthquakeTable->setColumnWidth(1, 200); // Location
    m_earthquakeTable->setColumnWidth(2, 80);  // Magnitude
    m_earthquakeTable->setColumnWidth(3, 80);  // Depth
    m_earthquakeTable->setColumnWidth(4, 80);  // Alert
    
    m_rightSplitter->addWidget(m_earthquakeTable);
}

void EarthquakeMainWindow::setupDetailsPane()
{
    m_detailsPane = new QTextEdit;
    m_detailsPane->setReadOnly(true);
    m_detailsPane->setMaximumHeight(150);
    m_detailsPane->setPlainText("Select an earthquake to view details...");
    
    m_rightSplitter->addWidget(m_detailsPane);
    
    // Set splitter proportions
    m_rightSplitter->setSizes({200, 400, 100});
}

void EarthquakeMainWindow::setupMenuBar()
{
    // File Menu
    auto *fileMenu = menuBar()->addMenu("&File");
    
    auto *refreshAction = fileMenu->addAction("&Refresh Data");
    refreshAction->setShortcut(QKeySequence::Refresh);
    connect(refreshAction, &QAction::triggered, this, &EarthquakeMainWindow::refreshData);
    
    fileMenu->addSeparator();
    
    auto *exportAction = fileMenu->addAction("&Export Data...");
    exportAction->setShortcut(QKeySequence::SaveAs);
    connect(exportAction, &QAction::triggered, this, &EarthquakeMainWindow::exportData);
    
    auto *importAction = fileMenu->addAction("&Import Data...");
    importAction->setShortcut(QKeySequence::Open);
    connect(importAction, &QAction::triggered, this, &EarthquakeMainWindow::importData);
    
    fileMenu->addSeparator();
    
    auto *exitAction = fileMenu->addAction("E&xit");
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);
    
    // View Menu
    auto *viewMenu = menuBar()->addMenu("&View");
    
    auto *fullscreenAction = viewMenu->addAction("&Fullscreen");
    fullscreenAction->setShortcut(QKeySequence::FullScreen);
    connect(fullscreenAction, &QAction::triggered, this, &EarthquakeMainWindow::toggleFullscreen);
    
    auto *resetViewAction = viewMenu->addAction("&Reset Map View");
    resetViewAction->setShortcut(QKeySequence("Ctrl+R"));
    connect(resetViewAction, &QAction::triggered, this, &EarthquakeMainWindow::resetMapView);
    
    // Tools Menu
    auto *toolsMenu = menuBar()->addMenu("&Tools");
    
    auto *settingsAction = toolsMenu->addAction("&Settings...");
    settingsAction->setShortcut(QKeySequence::Preferences);
    connect(settingsAction, &QAction::triggered, this, &EarthquakeMainWindow::showSettingsDialog);
    
    // Help Menu
    auto *helpMenu = menuBar()->addMenu("&Help");
    
    auto *helpAction = helpMenu->addAction("&Help");
    helpAction->setShortcut(QKeySequence::HelpContents);
    connect(helpAction, &QAction::triggered, this, &EarthquakeMainWindow::showHelpDialog);
    
    auto *aboutAction = helpMenu->addAction("&About");
    connect(aboutAction, &QAction::triggered, this, &EarthquakeMainWindow::showAboutDialog);
}

void EarthquakeMainWindow::setupToolBar()
{
    auto *toolBar = addToolBar("Main");
    
    auto *refreshAction = toolBar->addAction("Refresh");
    connect(refreshAction, &QAction::triggered, this, &EarthquakeMainWindow::refreshData);
    
    toolBar->addSeparator();
    
    auto *resetViewAction = toolBar->addAction("Reset View");
    connect(resetViewAction, &QAction::triggered, this, &EarthquakeMainWindow::resetMapView);
    
    auto *fullscreenAction = toolBar->addAction("Fullscreen");
    connect(fullscreenAction, &QAction::triggered, this, &EarthquakeMainWindow::toggleFullscreen);
    
    toolBar->addSeparator();
    
    auto *settingsAction = toolBar->addAction("Settings");
    connect(settingsAction, &QAction::triggered, this, &EarthquakeMainWindow::showSettingsDialog);
}

void EarthquakeMainWindow::setupStatusBar()
{
    statusBar()->showMessage("Ready");
    
    // Add permanent widgets to status bar
    auto *connectionLabel = new QLabel("Connected");
    connectionLabel->setStyleSheet("color: green; font-weight: bold;");
    statusBar()->addPermanentWidget(connectionLabel);
    
    auto *updateLabel = new QLabel("Last update: Never");
    statusBar()->addPermanentWidget(updateLabel);
}

void EarthquakeMainWindow::setupDockWidgets()
{
    // Alerts dock widget
    auto *alertsDock = new QDockWidget("Recent Alerts", this);
    m_alertsList = new QListWidget;
    alertsDock->setWidget(m_alertsList);
    addDockWidget(Qt::BottomDockWidgetArea, alertsDock);
    
    // Initially hide the alerts dock
    alertsDock->hide();
}

void EarthquakeMainWindow::setupSystemTray()
{
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        m_trayIcon = new QSystemTrayIcon(this);
        m_trayIcon->setIcon(QIcon(":/icons/earthquake.png")); // Add icon resource
        m_trayIcon->setToolTip("Earthquake Alert System");
        
        auto *trayMenu = new QMenu(this);
        trayMenu->addAction("Show", this, &QWidget::show);
        trayMenu->addAction("Hide", this, &QWidget::hide);
        trayMenu->addSeparator();
        trayMenu->addAction("Exit", this, &QWidget::close);
        
        m_trayIcon->setContextMenu(trayMenu);
        connect(m_trayIcon, &QSystemTrayIcon::activated,
                this, &EarthquakeMainWindow::onTrayIconActivated);
        
        m_trayIcon->show();
    }
    
    // Setup alert sound
    m_alertSound = new QSoundEffect(this);
    m_alertSound->setSource(QUrl::fromLocalFile(":/sounds/alert.wav")); // Add sound resource
}

void EarthquakeMainWindow::connectSignals()
{
    // Map controls
    connect(m_latSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &EarthquakeMainWindow::onMapCenterChanged);
    connect(m_lonSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &EarthquakeMainWindow::onMapCenterChanged);
    connect(m_zoomSlider, &QSlider::valueChanged,
            this, &EarthquakeMainWindow::onZoomChanged);
    connect(m_showGridCheckBox, &QCheckBox::toggled,
            m_mapWidget, &EarthquakeMapWidget::setShowGrid);
    connect(m_showLegendCheckBox, &QCheckBox::toggled,
            m_mapWidget, &EarthquakeMapWidget::setShowLegend);
    connect(m_resetViewBtn, &QPushButton::clicked,
            this, &EarthquakeMainWindow::resetMapView);
    connect(m_fullscreenBtn, &QPushButton::clicked,
            this, &EarthquakeMainWindow::toggleFullscreen);
    
    // Filters
    connect(m_minMagnitudeSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &EarthquakeMainWindow::onFilterChanged);
    connect(m_maxMagnitudeSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &EarthquakeMainWindow::onFilterChanged);
    connect(m_maxAgeHoursSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &EarthquakeMainWindow::onFilterChanged);
    connect(m_alertLevelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EarthquakeMainWindow::onFilterChanged);
    connect(m_recentOnlyCheckBox, &QCheckBox::toggled,
            this, &EarthquakeMainWindow::onFilterChanged);
    
    // Earthquake table
    connect(m_earthquakeTable, &QTableWidget::itemSelectionChanged,
            this, &EarthquakeMainWindow::onEarthquakeSelected);
    connect(m_earthquakeTable, &QTableWidget::itemDoubleClicked,
            this, &EarthquakeMainWindow::onEarthquakeDoubleClicked);
    
    // Alert settings
    connect(m_alertsEnabledCheckBox, &QCheckBox::toggled,
            this, &EarthquakeMainWindow::onSettingsChanged);
    connect(m_soundEnabledCheckBox, &QCheckBox::toggled,
            this, &EarthquakeMainWindow::toggleAlertSound);
    connect(m_alertThresholdCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EarthquakeMainWindow::onSettingsChanged);
}

void EarthquakeMainWindow::fetchEarthquakeData()
{
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply = nullptr;
    }
    
    statusBar()->showMessage("Fetching earthquake data...");
    
    // Example URL - replace with actual earthquake data service
    QString url = "https://earthquake.usgs.gov/earthquakes/feed/v1.0/summary/all_day.geojson";
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "EarthquakeAlertSystem/2.1");
    
    m_currentReply = m_networkManager->get(request);
}

void EarthquakeMainWindow::onDataReceived()
{
    auto *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        
        try {
            QJsonDocument doc = QJsonDocument::fromJson(data);
            QJsonObject root = doc.object();
            QJsonArray features = root["features"].toArray();
            
            QVector<EarthquakeData> newEarthquakes;
            
            for (const auto &value : features) {
                QJsonObject feature = value.toObject();
                QJsonObject properties = feature["properties"].toObject();
                QJsonArray coordinates = feature["geometry"].toObject()["coordinates"].toArray();
                
                if (coordinates.size() >= 2) {
                    EarthquakeData eq;
                    eq.longitude = coordinates[0].toDouble();
                    eq.latitude = coordinates[1].toDouble();
                    eq.depth = coordinates.size() > 2 ? coordinates[2].toDouble() : 0.0;
                    eq.magnitude = properties["mag"].toDouble();
                    eq.location = properties["place"].toString();
                    eq.timestamp = QDateTime::fromMSecsSinceEpoch(properties["time"].toVariant().toLongLong());
                    
                    // Calculate alert level based on magnitude
                    if (eq.magnitude < 3.0) eq.alertLevel = 0;
                    else if (eq.magnitude < 4.0) eq.alertLevel = 1;
                    else if (eq.magnitude < 5.0) eq.alertLevel = 2;
                    else if (eq.magnitude < 6.0) eq.alertLevel = 3;
                    else eq.alertLevel = 4;
                    
                    newEarthquakes.append(eq);
                }
            }
            
            m_allEarthquakes = newEarthquakes;
            applyFilters();
            updateStatistics();
            
            statusBar()->showMessage(QString("Loaded %1 earthquakes").arg(newEarthquakes.size()), 3000);
            m_lastUpdateLabel->setText(QString("Last Update: %1").arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
            
        } catch (const std::exception &e) {
            statusBar()->showMessage("Error parsing earthquake data", 5000);
            qDebug() << "JSON parsing error:" << e.what();
        }
        
    } else {
        onNetworkError();
    }
    
    reply->deleteLater();
    m_currentReply = nullptr;
}

void EarthquakeMainWindow::onNetworkError()
{
    statusBar()->showMessage("Failed to fetch earthquake data", 5000);
