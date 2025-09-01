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
    
    if (m_trayIcon) {
        m_trayIcon->showMessage("Earthquake Alert System", 
                               "Failed to fetch earthquake data. Check your internet connection.",
                               QSystemTrayIcon::Warning, 5000);
    }
}

void EarthquakeMainWindow::refreshData()
{
    fetchEarthquakeData();
}

void EarthquakeMainWindow::exportData()
{
    QString fileName = QFileDialog::getSaveFileName(this, 
        "Export Earthquake Data", 
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/earthquakes.csv",
        "CSV Files (*.csv);;JSON Files (*.json)");
    
    if (fileName.isEmpty()) return;
    
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Export Error", "Could not open file for writing.");
        return;
    }
    
    QTextStream out(&file);
    
    if (fileName.endsWith(".csv")) {
        // Export as CSV
        out << "Timestamp,Latitude,Longitude,Magnitude,Depth,Location,AlertLevel\n";
        for (const auto &eq : m_filteredEarthquakes) {
            out << eq.timestamp.toString(Qt::ISODate) << ","
                << eq.latitude << ","
                << eq.longitude << ","
                << eq.magnitude << ","
                << eq.depth << ","
                << "\"" << eq.location << "\","
                << eq.alertLevel << "\n";
        }
    } else {
        // Export as JSON
        QJsonArray earthquakesArray;
        for (const auto &eq : m_filteredEarthquakes) {
            QJsonObject eqObj;
            eqObj["timestamp"] = eq.timestamp.toString(Qt::ISODate);
            eqObj["latitude"] = eq.latitude;
            eqObj["longitude"] = eq.longitude;
            eqObj["magnitude"] = eq.magnitude;
            eqObj["depth"] = eq.depth;
            eqObj["location"] = eq.location;
            eqObj["alertLevel"] = eq.alertLevel;
            earthquakesArray.append(eqObj);
        }
        
        QJsonObject root;
        root["earthquakes"] = earthquakesArray;
        root["exportTime"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        root["totalCount"] = earthquakesArray.size();
        
        QJsonDocument doc(root);
        out << doc.toJson();
    }
    
    statusBar()->showMessage(QString("Exported %1 earthquakes to %2").arg(m_filteredEarthquakes.size()).arg(fileName), 3000);
}

void EarthquakeMainWindow::importData()
{
    QString fileName = QFileDialog::getOpenFileName(this,
        "Import Earthquake Data",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        "JSON Files (*.json);;CSV Files (*.csv)");
    
    if (fileName.isEmpty()) return;
    
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Import Error", "Could not open file for reading.");
        return;
    }
    
    // Implementation would parse the file format and load earthquake data
    statusBar()->showMessage("Import functionality would be implemented here", 3000);
}

void EarthquakeMainWindow::onMapCenterChanged()
{
    m_mapWidget->setMapCenter(m_latSpinBox->value(), m_lonSpinBox->value());
}

void EarthquakeMainWindow::onZoomChanged(int value)
{
    double zoom = value / 100.0; // Convert slider value to zoom level
    m_mapWidget->setZoomLevel(zoom);
}

void EarthquakeMainWindow::onFilterChanged()
{
    applyFilters();
}

void EarthquakeMainWindow::resetMapView()
{
    m_latSpinBox->setValue(0.0);
    m_lonSpinBox->setValue(0.0);
    m_zoomSlider->setValue(100);
    m_mapWidget->setMapCenter(0.0, 0.0);
    m_mapWidget->setZoomLevel(1.0);
}

void EarthquakeMainWindow::toggleFullscreen()
{
    if (isFullScreen()) {
        showNormal();
        m_fullscreenBtn->setText("Fullscreen");
    } else {
        showFullScreen();
        m_fullscreenBtn->setText("Exit Fullscreen");
    }
}

void EarthquakeMainWindow::onEarthquakeSelected()
{
    int currentRow = m_earthquakeTable->currentRow();
    if (currentRow >= 0 && currentRow < m_filteredEarthquakes.size()) {
        const EarthquakeData &eq = m_filteredEarthquakes[currentRow];
        
        // Update details pane
        QString details = QString(
            "Earthquake Details\n"
            "==================\n"
            "Location: %1\n"
            "Magnitude: %2\n"
            "Depth: %3 km\n"
            "Time: %4\n"
            "Alert Level: %5\n"
            "Coordinates: %6°, %7°\n\n"
            "Estimated Effects:\n"
            "Seismic Energy: %.2e J\n"
            "Mercalli Intensity: %8\n"
        ).arg(eq.location)
         .arg(formatMagnitude(eq.magnitude))
         .arg(formatDepth(eq.depth))
         .arg(eq.timestamp.toString("yyyy-MM-dd hh:mm:ss UTC"))
         .arg(getAlertLevelText(eq.alertLevel))
         .arg(eq.latitude, 0, 'f', 4)
         .arg(eq.longitude, 0, 'f', 4)
         .arg(SpatialUtils::calculateSeismicEnergy(eq.magnitude))
         .arg(SpatialUtils::mercalliIntensity(eq.magnitude, 50.0)); // Assume 50km distance
        
        m_detailsPane->setPlainText(details);
        
        // Center map on selected earthquake
        m_mapWidget->setMapCenter(eq.latitude, eq.longitude);
        
        // Update spinboxes without triggering signals
        m_latSpinBox->blockSignals(true);
        m_lonSpinBox->blockSignals(true);
        m_latSpinBox->setValue(eq.latitude);
        m_lonSpinBox->setValue(eq.longitude);
        m_latSpinBox->blockSignals(false);
        m_lonSpinBox->blockSignals(false);
    }
}

void EarthquakeMainWindow::onEarthquakeDoubleClicked()
{
    onEarthquakeSelected();
    m_mapWidget->setZoomLevel(5.0); // Zoom in on double-click
    m_zoomSlider->setValue(500);
}

void EarthquakeMainWindow::sortEarthquakeList()
{
    // Custom sorting logic can be implemented here
    m_earthquakeTable->sortItems(0, Qt::DescendingOrder); // Sort by time, newest first
}

void EarthquakeMainWindow::checkForAlerts()
{
    if (!m_alertsEnabled) return;
    
    QDateTime cutoff = QDateTime::currentDateTime().addSecs(-300); // 5 minutes ago
    double threshold = m_alertThresholdCombo->currentText().left(3).toDouble();
    
    for (const auto &eq : m_allEarthquakes) {
        if (eq.timestamp > cutoff && eq.magnitude >= threshold) {
            showAlert(eq);
        }
    }
}

void EarthquakeMainWindow::showAlert(const EarthquakeData &earthquake)
{
    QString alertText = QString("EARTHQUAKE ALERT\nM%1 - %2\n%3")
                       .arg(formatMagnitude(earthquake.magnitude))
                       .arg(earthquake.location)
                       .arg(earthquake.timestamp.toString("hh:mm:ss"));
    
    // Add to alerts list
    auto *item = new QListWidgetItem(alertText);
    item->setBackground(getAlertLevelColor(earthquake.alertLevel));
    m_alertsList->insertItem(0, item);
    
    // Keep only last 50 alerts
    if (m_alertsList->count() > 50) {
        delete m_alertsList->takeItem(m_alertsList->count() - 1);
    }
    
    // Show system tray notification
    if (m_trayIcon) {
        m_trayIcon->showMessage("Earthquake Alert", alertText, 
                               QSystemTrayIcon::Warning, 10000);
    }
    
    // Play alert sound
    if (m_soundEnabled && earthquake.alertLevel >= 2) {
        playAlertSound(earthquake.alertLevel);
    }
    
    // Update alert indicator
    m_alertIndicator->setValue(earthquake.alertLevel);
    m_alertIndicator->setFormat(getAlertLevelText(earthquake.alertLevel));
    
    // Flash the window if minimized
    if (isMinimized()) {
        QApplication::alert(this, 5000);
    }
}

void EarthquakeMainWindow::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick) {
        if (isVisible()) {
            hide();
        } else {
            show();
            raise();
            activateWindow();
        }
    }
}

void EarthquakeMainWindow::toggleAlertSound(bool enabled)
{
    m_soundEnabled = enabled;
    onSettingsChanged();
}

void EarthquakeMainWindow::showSettingsDialog()
{
    // Settings dialog implementation would go here
    QMessageBox::information(this, "Settings", "Settings dialog would be implemented here.");
}

void EarthquakeMainWindow::onSettingsChanged()
{
    m_alertsEnabled = m_alertsEnabledCheckBox->isChecked();
    
    QString thresholdText = m_alertThresholdCombo->currentText();
    m_alertThreshold = thresholdText.left(3).toDouble();
    
    saveSettings();
}

void EarthquakeMainWindow::saveSettings()
{
    if (!m_settings) {
        m_settings = new QSettings("EarthquakeAlertSystem", "MainWindow", this);
    }
    
    m_settings->setValue("geometry", saveGeometry());
    m_settings->setValue("windowState", saveState());
    m_settings->setValue("mapCenter/latitude", m_latSpinBox->value());
    m_settings->setValue("mapCenter/longitude", m_lonSpinBox->value());
    m_settings->setValue("mapZoom", m_zoomSlider->value());
    m_settings->setValue("showGrid", m_showGridCheckBox->isChecked());
    m_settings->setValue("showLegend", m_showLegendCheckBox->isChecked());
    m_settings->setValue("filters/minMagnitude", m_minMagnitudeSpinBox->value());
    m_settings->setValue("filters/maxMagnitude", m_maxMagnitudeSpinBox->value());
    m_settings->setValue("filters/maxAgeHours", m_maxAgeHoursSpinBox->value());
    m_settings->setValue("filters/alertLevel", m_alertLevelCombo->currentIndex());
    m_settings->setValue("filters/recentOnly", m_recentOnlyCheckBox->isChecked());
    m_settings->setValue("alerts/enabled", m_alertsEnabled);
    m_settings->setValue("alerts/sound", m_soundEnabled);
    m_settings->setValue("alerts/threshold", m_alertThreshold);
    m_settings->setValue("refreshInterval", m_refreshIntervalMinutes);
}

void EarthquakeMainWindow::loadSettings()
{
    if (!m_settings) {
        m_settings = new QSettings("EarthquakeAlertSystem", "MainWindow", this);
    }
    
    restoreGeometry(m_settings->value("geometry").toByteArray());
    restoreState(m_settings->value("windowState").toByteArray());
    
    m_latSpinBox->setValue(m_settings->value("mapCenter/latitude", 0.0).toDouble());
    m_lonSpinBox->setValue(m_settings->value("mapCenter/longitude", 0.0).toDouble());
    m_zoomSlider->setValue(m_settings->value("mapZoom", 100).toInt());
    m_showGridCheckBox->setChecked(m_settings->value("showGrid", true).toBool());
    m_showLegendCheckBox->setChecked(m_settings->value("showLegend", true).toBool());
    
    m_minMagnitudeSpinBox->setValue(m_settings->value("filters/minMagnitude", 2.0).toDouble());
    m_maxMagnitudeSpinBox->setValue(m_settings->value("filters/maxMagnitude", 10.0).toDouble());
    m_maxAgeHoursSpinBox->setValue(m_settings->value("filters/maxAgeHours", 168).toInt());
    m_alertLevelCombo->setCurrentIndex(m_settings->value("filters/alertLevel", 0).toInt());
    m_recentOnlyCheckBox->setChecked(m_settings->value("filters/recentOnly", false).toBool());
    
    m_alertsEnabled = m_settings->value("alerts/enabled", true).toBool();
    m_soundEnabled = m_settings->value("alerts/sound", true).toBool();
    m_alertThreshold = m_settings->value("alerts/threshold", 5.0).toDouble();
    m_refreshIntervalMinutes = m_settings->value("refreshInterval", 5).toInt();
    
    m_alertsEnabledCheckBox->setChecked(m_alertsEnabled);
    m_soundEnabledCheckBox->setChecked(m_soundEnabled);
    
    // Set alert threshold combo
    QString thresholdText = QString::number(m_alertThreshold, 'f', 1) + "+";
    int index = m_alertThresholdCombo->findText(thresholdText);
    if (index >= 0) {
        m_alertThresholdCombo->setCurrentIndex(index);
    }
    
    // Apply loaded settings
    onMapCenterChanged();
    onZoomChanged(m_zoomSlider->value());
    m_mapWidget->setShowGrid(m_showGridCheckBox->isChecked());
    m_mapWidget->setShowLegend(m_showLegendCheckBox->isChecked());
}

void EarthquakeMainWindow::showAboutDialog()
{
    QMessageBox::about(this, "About Earthquake Alert System",
        "<h3>Earthquake Alert System v2.1</h3>"
        "<p>A comprehensive real-time earthquake monitoring and alert application.</p>"
        "<p><b>Features:</b></p>"
        "<ul>"
        "<li>Real-time earthquake data from USGS</li>"
        "<li>Interactive world map with zoom and pan</li>"
        "<li>Customizable alerts and notifications</li>"
        "<li>Advanced filtering and sorting</li>"
        "<li>Data export capabilities</li>"
        "<li>System tray integration</li>"
        "</ul>"
        "<p><b>Data Source:</b> United States Geological Survey (USGS)</p>"
        "<p><b>Built with:</b> Qt Framework and C++</p>"
        "<p>© 2025 Earthquake Alert System. All rights reserved.</p>");
}

void EarthquakeMainWindow::showHelpDialog()
{
    QMessageBox::information(this, "Help",
        "<h3>Earthquake Alert System Help</h3>"
        "<p><b>Map Navigation:</b></p>"
        "<ul>"
        "<li>Left-click and drag to pan the map</li>"
        "<li>Use mouse wheel to zoom in/out</li>"
        "<li>Double-click an earthquake in the list to center on it</li>"
        "</ul>"
        "<p><b>Filtering:</b></p>"
        "<ul>"
        "<li>Set magnitude range to filter earthquakes by size</li>"
        "<li>Use age filter to show only recent earthquakes</li>"
        "<li>Select minimum alert level to focus on significant events</li>"
        "</ul>"
        "<p><b>Alerts:</b></p>"
        "<ul>"
        "<li>Enable alerts to receive notifications for significant earthquakes</li>"
        "<li>Set alert threshold to control when you're notified</li>"
        "<li>Sound alerts can be enabled/disabled separately</li>"
        "</ul>"
        "<p><b>Color Coding:</b></p>"
        "<ul>"
        "<li>Green: Minor earthquakes (M2-3)</li>"
        "<li>Yellow: Light earthquakes (M3-4)</li>"
        "<li>Orange: Moderate earthquakes (M4-5)</li>"
        "<li>Red: Strong earthquakes (M5-6)</li>"
        "<li>Dark Red: Major earthquakes (M6-7)</li>"
        "<li>Purple: Great earthquakes (M7+)</li>"
        "</ul>");
}

void EarthquakeMainWindow::updateEarthquakeList()
{
    m_earthquakeTable->setRowCount(m_filteredEarthquakes.size());
    
    for (int i = 0; i < m_filteredEarthquakes.size(); ++i) {
        const EarthquakeData &eq = m_filteredEarthquakes[i];
        
        // Time
        auto *timeItem = new QTableWidgetItem(eq.timestamp.toString("MM/dd hh:mm"));
        timeItem->setData(Qt::UserRole, eq.timestamp);
        m_earthquakeTable->setItem(i, 0, timeItem);
        
        // Location
        auto *locationItem = new QTableWidgetItem(eq.location);
        m_earthquakeTable->setItem(i, 1, locationItem);
        
        // Magnitude
        auto *magnitudeItem = new QTableWidgetItem(formatMagnitude(eq.magnitude));
        magnitudeItem->setData(Qt::UserRole, eq.magnitude);
        magnitudeItem->setBackground(getMagnitudeColor(eq.magnitude));
        m_earthquakeTable->setItem(i, 2, magnitudeItem);
        
        // Depth
        auto *depthItem = new QTableWidgetItem(formatDepth(eq.depth));
        depthItem->setData(Qt::UserRole, eq.depth);
        m_earthquakeTable->setItem(i, 3, depthItem);
        
        // Alert Level
        auto *alertItem = new QTableWidgetItem(getAlertLevelText(eq.alertLevel));
        alertItem->setData(Qt::UserRole, eq.alertLevel);
        alertItem->setBackground(getAlertLevelColor(eq.alertLevel));
        m_earthquakeTable->setItem(i, 4, alertItem);
        
        // Distance (from map center)
        double distance = SpatialUtils::haversineDistance(
            m_latSpinBox->value(), m_lonSpinBox->value(),
            eq.latitude, eq.longitude);
        auto *distanceItem = new QTableWidgetItem(QString("%1 km").arg(distance, 0, 'f', 0));
        distanceItem->setData(Qt::UserRole, distance);
        m_earthquakeTable->setItem(i, 5, distanceItem);
    }
    
    sortEarthquakeList();
}

void EarthquakeMainWindow::updateStatistics()
{
    int total = m_allEarthquakes.size();
    
    // Count recent earthquakes (last 24 hours)
    QDateTime cutoff = QDateTime::currentDateTime().addDays(-1);
    int recent = 0;
    double highestMag = 0.0;
    
    for (const auto &eq : m_allEarthquakes) {
        if (eq.timestamp > cutoff) {
            recent++;
        }
        if (eq.magnitude > highestMag) {
            highestMag = eq.magnitude;
        }
    }
    
    m_totalEarthquakesLabel->setText(QString("Total: %1").arg(total));
    m_recentEarthquakesLabel->setText(QString("Last 24h: %1").arg(recent));
    
    if (highestMag > 0.0) {
        m_highestMagnitudeLabel->setText(QString("Highest: M%1").arg(highestMag, 0, 'f', 1));
    } else {
        m_highestMagnitudeLabel->setText("Highest: N/A");
    }
}

void EarthquakeMainWindow::updateStatusBar()
{
    statusBar()->showMessage(QString("Displaying %1 of %2 earthquakes")
                           .arg(m_filteredEarthquakes.size())
                           .arg(m_allEarthquakes.size()));
}

void EarthquakeMainWindow::applyFilters()
{
    m_filteredEarthquakes.clear();
    
    for (const auto &eq : m_allEarthquakes) {
        if (passesFilter(eq)) {
            m_filteredEarthquakes.append(eq);
        }
    }
    
    // Update map with filtered data
    m_mapWidget->clearEarthquakes();
    for (const auto &eq : m_filteredEarthquakes) {
        m_mapWidget->addEarthquake(eq);
    }
    
    updateEarthquakeList();
    updateStatusBar();
}

QString EarthquakeMainWindow::formatMagnitude(double magnitude) const
{
    return QString("M%1").arg(magnitude, 0, 'f', 1);
}

QString EarthquakeMainWindow::formatDepth(double depth) const
{
    return QString("%1 km").arg(depth, 0, 'f', 1);
}

QString EarthquakeMainWindow::formatTimeAgo(const QDateTime &timestamp) const
{
    qint64 seconds = timestamp.secsTo(QDateTime::currentDateTime());
    
    if (seconds < 60) {
        return QString("%1s ago").arg(seconds);
    } else if (seconds < 3600) {
        return QString("%1m ago").arg(seconds / 60);
    } else if (seconds < 86400) {
        return QString("%1h ago").arg(seconds / 3600);
    } else {
        return QString("%1d ago").arg(seconds / 86400);
    }
}

QString EarthquakeMainWindow::getAlertLevelText(int level) const
{
    switch (level) {
        case 0: return "Info";
        case 1: return "Minor";
        case 2: return "Moderate";
        case 3: return "Major";
        case 4: return "Critical";
        default: return "Unknown";
    }
}

bool EarthquakeMainWindow::passesFilter(const EarthquakeData &eq) const
{
    // Magnitude filter
    if (eq.magnitude < m_minMagnitudeSpinBox->value() || 
        eq.magnitude > m_maxMagnitudeSpinBox->value()) {
        return false;
    }
    
    // Age filter
    QDateTime cutoff = QDateTime::currentDateTime().addSecs(-m_maxAgeHoursSpinBox->value() * 3600);
    if (eq.timestamp < cutoff) {
        return false;
    }
    
    // Recent only filter
    if (m_recentOnlyCheckBox->isChecked()) {
        QDateTime recentCutoff = QDateTime::currentDateTime().addDays(-1);
        if (eq.timestamp < recentCutoff) {
            return false;
        }
    }
    
    // Alert level filter
    int minAlertLevel = m_alertLevelCombo->currentIndex() - 1; // "All" = -1
    if (minAlertLevel >= 0 && eq.alertLevel < minAlertLevel) {
        return false;
    }
    
    return true;
}

void EarthquakeMainWindow::playAlertSound(int alertLevel)
{
    if (m_alertSound && m_alertSound->isLoaded()) {
        // Play different tones based on alert level
        m_alertSound->setVolume(0.3 + 0.2 * alertLevel); // Volume increases with severity
        m_alertSound->play();
    }
}

QColor EarthquakeMainWindow::getMagnitudeColor(double magnitude) const
{
    if (magnitude < 3.0) return QColor(100, 255, 100);      // Light green
    else if (magnitude < 4.0) return QColor(255, 255, 100); // Yellow
    else if (magnitude < 5.0) return QColor(255, 180, 100); // Orange
    else if (magnitude < 6.0) return QColor(255, 100, 100); // Light red
    else if (magnitude < 7.0) return QColor(200, 50, 50);   // Red
    else return QColor(150, 0, 150);                        // Purple
}

QColor EarthquakeMainWindow::getAlertLevelColor(int alertLevel) const
{
    switch (alertLevel) {
        case 0: return QColor(100, 150, 255); // Blue - Info
        case 1: return QColor(100, 255, 100); // Green - Minor
        case 2: return QColor(255, 255, 100); // Yellow - Moderate
        case 3: return QColor(255, 150, 50);  // Orange - Major
        case 4: return QColor(255, 50, 50);   // Red - Critical
        default: return QColor(128, 128, 128); // Gray - Unknown
    }
}

void EarthquakeMainWindow::closeEvent(QCloseEvent *event)
{
    if (m_trayIcon && m_trayIcon->isVisible()) {
        // Hide to system tray instead of closing
        hide();
        m_trayIcon->showMessage("Earthquake Alert System",
                               "Application was minimized to tray",
                               QSystemTrayIcon::Information, 2000);
        event->ignore();
    } else {
        saveSettings();
        event->accept();
    }
}
