#pragma once
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QSplashScreen>
#include <QtWidgets/QStyleFactory>
#include <QtWidgets/QSystemTrayIcon>
#include <QtCore/QCommandLineParser>
#include <QtCore/QCommandLineOption>
#include <QtCore/QDir>
#include <QtCore/QStandardPaths>
#include <QtCore/QLoggingCategory>
#include <QtCore/QTimer>
#include <QtCore/QTranslator>
#include <QtCore/QLibraryInfo>
#include <QtCore/QProcessEnvironment>
#include <QtGui/QPixmap>
#include <QtGui/QPainter>
#include <QtNetwork/QSslSocket>

#include "earthquake_api_client.hpp"
#include "earthquake_main_window.hpp"
#include "earthquake_map_widget.hpp"
#include "notification_manager.hpp"
#include "spatial_utils.hpp"

// Application information
#define APP_NAME "Earthquake Alert System"
#define APP_VERSION "2.1.0"
#define APP_ORGANIZATION "Earthquake Alert System"
#define APP_DOMAIN "earthquakealert.org"
#define APP_DESCRIPTION "Real-time earthquake monitoring and alert application"

// Logging categories
Q_LOGGING_CATEGORY(main, "earthquake.main")
Q_LOGGING_CATEGORY(network, "earthquake.network")
Q_LOGGING_CATEGORY(notifications, "earthquake.notifications")
Q_LOGGING_CATEGORY(spatial, "earthquake.spatial")

class EarthquakeApplication : public QApplication
{
    Q_OBJECT

public:
    EarthquakeApplication(int &argc, char **argv)
        : QApplication(argc, argv)
        , m_mainWindow(nullptr)
        , m_apiClient(nullptr)
        , m_notificationManager(nullptr)
        , m_splashScreen(nullptr)
        , m_initializationComplete(false)
    {
        setupApplication();
        parseCommandLine();
        setupLogging();
        setupApplicationPaths();
        checkSystemRequirements();
        setupSplashScreen();
        initializeComponents();
        connectSignals();
    }

    ~EarthquakeApplication()
    {
        cleanup();
    }

    bool initialize()
    {
        try {
            // Show splash screen
            if (m_splashScreen && !m_commandLineArgs.noSplash) {
                m_splashScreen->show();
                updateSplashMessage("Initializing earthquake monitoring system...");
                processEvents();
            }

            // Initialize API client
            updateSplashMessage("Setting up data connections...");
            processEvents();
            
            if (!initializeApiClient()) {
                throw std::runtime_error("Failed to initialize API client");
            }

            // Initialize notification system
            updateSplashMessage("Configuring notification system...");
            processEvents();
            
            if (!initializeNotificationManager()) {
                throw std::runtime_error("Failed to initialize notification manager");
            }

            // Initialize main window
            updateSplashMessage("Building user interface...");
            processEvents();
            
            if (!initializeMainWindow()) {
                throw std::runtime_error("Failed to initialize main window");
            }

            // Connect all components
            updateSplashMessage("Connecting system components...");
            processEvents();
            
            connectComponents();

            // Load initial data
            updateSplashMessage("Loading earthquake data...");
            processEvents();
            
            loadInitialData();

            // Finalize initialization
            updateSplashMessage("Starting monitoring...");
            processEvents();
            
            finalizeInitialization();

            m_initializationComplete = true;
            
            // Hide splash screen
            if (m_splashScreen) {
                QTimer::singleShot(1000, [this]() {
                    m_splashScreen->finish(m_mainWindow);
                    m_splashScreen->deleteLater();
                    m_splashScreen = nullptr;
                });
            }

            // Show main window unless started minimized
            if (!m_commandLineArgs.startMinimized) {
                m_mainWindow->show();
                m_mainWindow->raise();
                m_mainWindow->activateWindow();
            }

            return true;
            
        } catch (const std::exception &e) {
            handleInitializationError(e.what());
            return false;
        }
    }

private slots:
    void onEarthquakeDataReceived(const QVector<EarthquakeData> &earthquakes, ApiRequestType requestType)
    {
        qCInfo(main) << "Received" << earthquakes.size() << "earthquakes, type:" << static_cast<int>(requestType);
        
        // Forward to main window
        if (m_mainWindow) {
            for (const auto &earthquake : earthquakes) {
                m_mainWindow->addEarthquake(earthquake);
            }
            m_mainWindow->updateDataTimestamp();
        }
        
        // Trigger notifications for new earthquakes
        if (m_notificationManager && requestType == ApiRequestType::Refresh) {
            for (const auto &earthquake : earthquakes) {
                m_notificationManager->showEarthquakeAlert(earthquake);
            }
        }
        
        // Show data update notification for large updates
        if (earthquakes.size() > 10) {
            m_notificationManager->showDataUpdateNotification(earthquakes.size());
        }
    }
    
    void onApiError(const QString &error, ApiRequestType requestType)
    {
        qCWarning(network) << "API Error:" << error << "Type:" << static_cast<int>(requestType);
        
        if (m_notificationManager) {
            m_notificationManager->showSystemNotification("Data Error", 
                QString("Failed to fetch earthquake data: %1").arg(error),
                NotificationType::Warning);
        }
        
        if (m_mainWindow) {
            m_mainWindow->showNetworkError(error);
        }
    }
    
    void onNetworkStatusChanged(bool connected)
    {
        qCInfo(network) << "Network status changed:" << (connected ? "Connected" : "Disconnected");
        
        if (m_notificationManager) {
            m_notificationManager->showNetworkStatusNotification(connected);
        }
        
        if (m_mainWindow) {
            m_mainWindow->updateNetworkStatus(connected);
        }
    }
    
    void onNotificationTriggered(const QString &ruleName, const EarthquakeData &earthquake)
    {
        qCInfo(notifications) << "Alert rule triggered:" << ruleName 
                             << "for M" << earthquake.magnitude << earthquake.location;
        
        // Log significant alerts
        if (earthquake.magnitude >= 7.0) {
            qCCritical(main) << "MAJOR EARTHQUAKE ALERT: M" << earthquake.magnitude 
                            << earthquake.location << "at" << earthquake.timestamp.toString();
        }
    }

private:
    struct CommandLineArgs {
        bool noSplash = false;
        bool startMinimized = false;
        bool debugMode = false;
        bool offlineMode = false;
        QString configFile;
        QString logLevel = "info";
        QString dataSource;
        double userLatitude = 0.0;
        double userLongitude = 0.0;
        bool hasUserLocation = false;
    };

    void setupApplication()
    {
        // Set application metadata
        setApplicationName(APP_NAME);
        setApplicationVersion(APP_VERSION);
        setOrganizationName(APP_ORGANIZATION);
        setOrganizationDomain(APP_DOMAIN);
        setApplicationDisplayName(APP_NAME);
        
        // Set application icon
        setWindowIcon(QIcon(":/icons/earthquake_app.png"));
        
        // Enable high DPI scaling
        setAttribute(Qt::AA_EnableHighDpiScaling);
        setAttribute(Qt::AA_UseHighDpiPixmaps);
        
        // Set application style
        setStyle(QStyleFactory::create("Fusion"));
        
        // Apply dark theme if requested
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        if (env.value("EARTHQUAKE_THEME", "light").toLower() == "dark") {
            applyDarkTheme();
        }
    }

    void parseCommandLine()
    {
        QCommandLineParser parser;
        parser.setApplicationDescription(APP_DESCRIPTION);
        parser.addHelpOption();
        parser.addVersionOption();

        // Define command line options
        QCommandLineOption noSplashOption({"no-splash", "n"}, "Start without splash screen");
        QCommandLineOption minimizedOption({"minimized", "m"}, "Start minimized to system tray");
        QCommandLineOption debugOption({"debug", "d"}, "Enable debug mode with verbose logging");
        QCommandLineOption offlineOption({"offline", "o"}, "Start in offline mode (no network requests)");
        QCommandLineOption configOption({"config", "c"}, "Use custom configuration file", "file");
        QCommandLineOption logLevelOption({"log-level", "l"}, "Set logging level (debug, info, warning, critical)", "level", "info");
        QCommandLineOption dataSourceOption({"data-source", "s"}, "Override default data source", "source");
        QCommandLineOption locationOption({"location"}, "Set user location (lat,lon)", "coordinates");

        parser.addOption(noSplashOption);
        parser.addOption(minimizedOption);
        parser.addOption(debugOption);
        parser.addOption(offlineOption);
        parser.addOption(configOption);
        parser.addOption(logLevelOption);
        parser.addOption(dataSourceOption);
        parser.addOption(locationOption);

        // Process command line
        parser.process(*this);

        // Store parsed arguments
        m_commandLineArgs.noSplash = parser.isSet(noSplashOption);
        m_commandLineArgs.startMinimized = parser.isSet(minimizedOption);
        m_commandLineArgs.debugMode = parser.isSet(debugOption);
        m_commandLineArgs.offlineMode = parser.isSet(offlineOption);
        m_commandLineArgs.configFile = parser.value(configOption);
        m_commandLineArgs.logLevel = parser.value(logLevelOption);
        m_commandLineArgs.dataSource = parser.value(dataSourceOption);

        // Parse location if provided
        if (parser.isSet(locationOption)) {
            QString locationStr = parser.value(locationOption);
            QStringList coords = locationStr.split(',');
            if (coords.size() == 2) {
                bool latOk, lonOk;
                double lat = coords[0].toDouble(&latOk);
                double lon = coords[1].toDouble(&lonOk);
                if (latOk && lonOk) {
                    m_commandLineArgs.userLatitude = lat;
                    m_commandLineArgs.userLongitude = lon;
                    m_commandLineArgs.hasUserLocation = true;
                }
            }
        }
    }

    void setupLogging()
    {
        // Configure logging based on command line arguments
        QString logLevel = m_commandLineArgs.logLevel.toLower();
        
        if (logLevel == "debug" || m_commandLineArgs.debugMode) {
            QLoggingCategory::setFilterRules("*=true");
        } else if (logLevel == "warning") {
            QLoggingCategory::setFilterRules("*.debug=false\n*.info=false");
        } else if (logLevel == "critical") {
            QLoggingCategory::setFilterRules("*.debug=false\n*.info=false\n*.warning=false");
        } else {
            QLoggingCategory::setFilterRules("*.debug=false"); // Default: info and above
        }
        
        // Setup log file
        QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/logs";
        QDir().mkpath(logDir);
        
        QString logFile = logDir + QString("/earthquake_%1.log")
                         .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd"));
        
        qSetMessagePattern("[%{time yyyy-MM-dd hh:mm:ss.zzz}] [%{category}] [%{type}] %{message}");
        
        qCInfo(main) << "Application starting..." << APP_NAME << APP_VERSION;
        qCInfo(main) << "Log file:" << logFile;
    }

    void setupApplicationPaths()
    {
        // Ensure application data directories exist
        QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QStringList subdirs = {"logs", "data", "cache", "notifications", "sounds", "maps"};
        
        for (const QString &subdir : subdirs) {
            QString path = appDataPath + "/" + subdir;
            if (!QDir().mkpath(path)) {
                qCWarning(main) << "Failed to create directory:" << path;
            }
        }
        
        qCInfo(main) << "Application data path:" << appDataPath;
    }

    void checkSystemRequirements()
    {
        // Check Qt version
        qCInfo(main) << "Qt version:" << qVersion();
        
        // Check SSL support
        if (!QSslSocket::supportsSsl()) {
            qCWarning(main) << "SSL not supported - HTTPS connections may fail";
        } else {
            qCInfo(main) << "SSL version:" << QSslSocket::sslLibraryVersionString();
        }
        
        // Check system tray availability
        if (!QSystemTrayIcon::isSystemTrayAvailable()) {
            qCWarning(main) << "System tray not available - notifications will be limited";
            
            if (!m_commandLineArgs.noSplash) {
                QMessageBox::warning(nullptr, APP_NAME,
                    "System tray is not available on this system.\n"
                    "Some notification features may not work properly.");
            }
        }
        
        // Check network connectivity
        QNetworkAccessManager testManager;
        QEventLoop loop;
        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        timeoutTimer.setInterval(5000); // 5 second timeout
        
        connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
        
        QNetworkRequest request(QUrl("https://earthquake.usgs.gov"));
        QNetworkReply *reply = testManager.head(request);
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        
        timeoutTimer.start();
        loop.exec();
        
        bool networkAvailable = (reply->error() == QNetworkReply::NoError);
        reply->deleteLater();
        
        qCInfo(main) << "Network connectivity:" << (networkAvailable ? "Available" : "Limited");
        
        if (!networkAvailable && !m_commandLineArgs.offlineMode) {
            qCWarning(main) << "Network connectivity issues detected";
        }
    }

    void setupSplashScreen()
    {
        if (m_commandLineArgs.noSplash) {
            return;
        }
        
        // Create splash screen
        QPixmap splashPixmap(400, 300);
        splashPixmap.fill(QColor(20, 30, 50));
        
        QPainter painter(&splashPixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        
        // Draw background gradient
        QLinearGradient gradient(0, 0, 0, 300);
        gradient.setColorAt(0, QColor(30, 50, 80));
        gradient.setColorAt(1, QColor(20, 30, 50));
        painter.fillRect(splashPixmap.rect(), gradient);
        
        // Draw app icon (if available)
        QPixmap icon(":/icons/earthquake_large.png");
        if (!icon.isNull()) {
            painter.drawPixmap(150, 50, 100, 100, icon);
        } else {
            // Draw simple earthquake icon
            painter.setPen(QPen(QColor(255, 100, 100), 3));
            painter.setBrush(QColor(255, 150, 150));
            painter.drawEllipse(175, 75, 50, 50);
            painter.setPen(QPen(QColor(255, 255, 255), 2));
            painter.drawText(185, 105, "EQ");
        }
        
        // Draw app name
        painter.setPen(QColor(255, 255, 255));
        painter.setFont(QFont("Arial", 18, QFont::Bold));
        painter.drawText(100, 180, 200, 30, Qt::AlignCenter, APP_NAME);
        
        // Draw version
        painter.setFont(QFont("Arial", 10));
        painter.drawText(100, 200, 200, 20, Qt::AlignCenter, QString("Version %1").arg(APP_VERSION));
        
        // Draw status text area
        painter.drawText(50, 250, 300, 20, Qt::AlignCenter, "Initializing...");
        
        m_splashScreen = new QSplashScreen(splashPixmap);
        m_splashScreen->setWindowFlags(Qt::WindowStaysOnTopHint | Qt::SplashScreen);
    }

    void updateSplashMessage(const QString &message)
    {
        if (m_splashScreen) {
            m_splashScreen->showMessage(message, Qt::AlignCenter | Qt::AlignBottom, QColor(255, 255, 255));
        }
    }

    void initializeComponents()
    {
        qCInfo(main) << "Initializing application components...";
        
        // Pre-initialize critical systems
        if (!QDir().exists(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))) {
            setupApplicationPaths();
        }
    }

    bool initializeApiClient()
    {
        try {
            m_apiClient = new EarthquakeApiClient(this);
            
            // Configure API client
            m_apiClient->setUserAgent(QString("%1/%2").arg(APP_NAME, APP_VERSION));
            m_apiClient->setTimeout(30000); // 30 second timeout
            m_apiClient->setMaxRetries(3);
            m_apiClient->setRateLimitDelay(1000); // 1 second between requests
            
            // Set custom data source if specified
            if (!m_commandLineArgs.dataSource.isEmpty()) {
                m_apiClient->setCustomApiUrl(m_commandLineArgs.dataSource);
            }
            
            qCInfo(main) << "API client initialized successfully";
            return true;
            
        } catch (const std::exception &e) {
            qCCritical(main) << "Failed to initialize API client:" << e.what();
            return false;
        }
    }

    bool initializeNotificationManager()
    {
        try {
            m_notificationManager = new NotificationManager(this);
            
            // Configure user location if provided
            if (m_commandLineArgs.hasUserLocation) {
                m_notificationManager->setUserLocation(m_commandLineArgs.userLatitude, 
                                                      m_commandLineArgs.userLongitude);
                qCInfo(main) << "User location set:" << m_commandLineArgs.userLatitude 
                            << m_commandLineArgs.userLongitude;
            }
            
            // Apply debug mode settings
            if (m_commandLineArgs.debugMode) {
                NotificationSettings settings = m_notificationManager->getSettings();
                settings.maxNotificationsPerHour = 100; // Higher limit for testing
                m_notificationManager->setSettings(settings);
            }
            
            qCInfo(main) << "Notification manager initialized successfully";
            return true;
            
        } catch (const std::exception &e) {
            qCCritical(main) << "Failed to initialize notification manager:" << e.what();
            return false;
        }
    }

    bool initializeMainWindow()
    {
        try {
            m_mainWindow = new EarthquakeMainWindow;
            
            // Configure main window
            m_mainWindow->setWindowTitle(QString("%1 v%2").arg(APP_NAME, APP_VERSION));
            
            // Apply command line settings
            if (m_commandLineArgs.debugMode) {
                m_mainWindow->enableDebugMode(true);
            }
            
            if (m_commandLineArgs.offlineMode) {
                m_mainWindow->setOfflineMode(true);
            }
            
            // Load custom configuration if specified
            if (!m_commandLineArgs.configFile.isEmpty()) {
                m_mainWindow->loadConfiguration(m_commandLineArgs.configFile);
            }
            
            qCInfo(main) << "Main window initialized successfully";
            return true;
            
        } catch (const std::exception &e) {
            qCCritical(main) << "Failed to initialize main window:" << e.what();
            return false;
        }
    }

    void connectComponents()
    {
        // Connect API client to main window
        connect(m_apiClient, &EarthquakeApiClient::earthquakeDataReceived,
                this, &EarthquakeApplication::onEarthquakeDataReceived);
        connect(m_apiClient, &EarthquakeApiClient::errorOccurred,
                this, &EarthquakeApplication::onApiError);
        connect(m_apiClient, &EarthquakeApiClient::networkStatusChanged,
                this, &EarthquakeApplication::onNetworkStatusChanged);
        
        // Connect notification manager
        connect(m_notificationManager, &NotificationManager::alertRuleTriggered,
                this, &EarthquakeApplication::onNotificationTriggered);
        
        // Connect main window to components
        if (m_mainWindow) {
            // Allow main window to control API client
            connect(m_mainWindow, &EarthquakeMainWindow::refreshDataRequested,
                    m_apiClient, QOverload<>::of(&EarthquakeApiClient::fetchAllEarthquakes));
            connect(m_mainWindow, &EarthquakeMainWindow::customDataRequested,
                    m_apiClient, &EarthquakeApiClient::fetchEarthquakesByRegion);
            
            // Allow main window to control notifications
            connect(m_mainWindow, &EarthquakeMainWindow::notificationSettingsChanged,
                    m_notificationManager, &NotificationManager::setSettings);
            connect(m_mainWindow, &EarthquakeMainWindow::userLocationChanged,
                    m_notificationManager, &NotificationManager::setUserLocation);
        }
        
        qCInfo(main) << "Component connections established";
    }

    void loadInitialData()
    {
        if (m_commandLineArgs.offlineMode) {
            qCInfo(main) << "Offline mode - skipping initial data load";
            return;
        }
        
        // Start with recent earthquake data
        m_apiClient->fetchRecentEarthquakes(24); // Last 24 hours
        
        // Start auto-refresh
        if (!m_commandLineArgs.debugMode) {
            m_apiClient->startAutoRefresh(5); // Every 5 minutes
        } else {
            m_apiClient->startAutoRefresh(1); // Every minute for testing
        }
        
        qCInfo(main) << "Initial data load started";
    }

    void finalizeInitialization()
    {
        // Show welcome notification
        if (m_notificationManager && !m_commandLineArgs.startMinimized) {
            m_notificationManager->showSystemNotification(
                "System Ready",
                "Earthquake Alert System is now monitoring for seismic activity",
                NotificationType::Info
            );
        }
        
        // Log successful startup
        qCInfo(main) << "Application initialization completed successfully";
        qCInfo(main) << "Monitoring started with auto-refresh every" << m_apiClient->getRefreshInterval() << "minutes";
    }

    void handleInitializationError(const QString &error)
    {
        qCCritical(main) << "Initialization failed:" << error;
        
        if (m_splashScreen) {
            m_splashScreen->hide();
        }
        
        QMessageBox::critical(nullptr, "Initialization Error",
            QString("Failed to initialize %1:\n\n%2\n\nThe application will now exit.")
            .arg(APP_NAME, error));
        
        exit(1);
    }

    void applyDarkTheme()
    {
        // Apply dark color scheme
        QPalette darkPalette;
        darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
        darkPalette.setColor(QPalette::WindowText, Qt::white);
        darkPalette.setColor(QPalette::Base, QColor(25, 25, 25));
        darkPalette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
        darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
        darkPalette.setColor(QPalette::ToolTipText, Qt::white);
        darkPalette.setColor(QPalette::Text, Qt::white);
        darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
        darkPalette.setColor(QPalette::ButtonText, Qt::white);
        darkPalette.setColor(QPalette::BrightText, Qt::red);
        darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
        darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
        darkPalette.setColor(QPalette::HighlightedText, Qt::black);
        
        setPalette(darkPalette);
        
        // Apply custom stylesheet for enhanced dark theme
        setStyleSheet(
            "QMainWindow { background-color: #353535; }"
            "QDockWidget { background-color: #404040; color: white; }"
            "QGroupBox { font-weight: bold; border: 2px solid #555; border-radius: 5px; margin: 5px; }"
            "QGroupBox::title { subcontrol-origin: margin; padding: 0 5px; }"
            "QTableWidget { gridline-color: #666; background-color: #2a2a2a; }"
            "QTableWidget::item:selected { background-color: #2a82da; }"
            "QHeaderView::section { background-color: #404040; border: 1px solid #555; }"
            "QSpinBox, QDoubleSpinBox, QComboBox { background-color: #404040; border: 1px solid #666; }"
            "QPushButton { background-color: #404040; border: 1px solid #666; padding: 5px; }"
            "QPushButton:hover { background-color: #4a4a4a; }"
            "QPushButton:pressed { background-color: #2a82da; }"
            "QSlider::groove:horizontal { background-color: #404040; height: 8px; }"
            "QSlider::handle:horizontal { background-color: #2a82da; width: 20px; margin: -6px 0; border-radius: 10px; }"
        );
        
        qCInfo(main) << "Dark theme applied";
    }

    void cleanup()
    {
        qCInfo(main) << "Application cleanup starting...";
        
        // Stop all timers and network requests
        if (m_apiClient) {
            m_apiClient->stopAutoRefresh();
            m_apiClient->cancelAllRequests();
        }
        
        // Save all settings
        if (m_mainWindow) {
            m_mainWindow->saveAllSettings();
        }
        
        if (m_notificationManager) {
            m_notificationManager->saveSettings();
        }
        
        qCInfo(main) << "Application cleanup completed";
    }

private:
    EarthquakeMainWindow *m_mainWindow;
    EarthquakeApiClient *m_apiClient;
    NotificationManager *m_notificationManager;
    QSplashScreen *m_splashScreen;
    CommandLineArgs m_commandLineArgs;
    bool m_initializationComplete;
};

// Signal handler for graceful shutdown
void signalHandler(int signal)
{
    qCInfo(main) << "Received signal" << signal << "- initiating graceful shutdown";
    QApplication::quit();
}

// Additional utility functions for main.cpp

namespace EarthquakeAppUtils {
    
    void setupTranslations(QApplication &app)
    {
        // Setup internationalization
        QTranslator qtTranslator;
        if (qtTranslator.load("qt_" + QLocale::system().name(),
                             QLibraryInfo::location(QLibraryInfo::TranslationsPath))) {
            app.installTranslator(&qtTranslator);
        }
        
        QTranslator appTranslator;
        if (appTranslator.load("earthquake_" + QLocale::system().name(), ":/translations")) {
            app.installTranslator(&appTranslator);
        }
        
        qCInfo(main) << "Translations loaded for locale:" << QLocale::system().name();
    }
    
    void checkForUpdates()
    {
        // Simple update check implementation
        QNetworkAccessManager manager;
        QNetworkRequest request(QUrl("https://api.earthquakealert.org/version"));
        request.setHeader(QNetworkRequest::UserAgentHeader, 
                         QString("%1/%2").arg(APP_NAME, APP_VERSION));
        
        QNetworkReply *reply = manager.get(request);
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        
        QTimer::singleShot(5000, &loop, &QEventLoop::quit); // 5 second timeout
        loop.exec();
        
        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QJsonObject obj = doc.object();
            QString latestVersion = obj["version"].toString();
            QString currentVersion = APP_VERSION;
            
            if (latestVersion != currentVersion && !latestVersion.isEmpty()) {
                qCInfo(main) << "Update available:" << latestVersion << "(current:" << currentVersion << ")";
                // Could show update notification here
            } else {
                qCInfo(main) << "Application is up to date";
            }
        }
        
        reply->deleteLater();
    }
    
    void performSystemCheck()
    {
        qCInfo(main) << "=== System Information ===";
        qCInfo(main) << "OS:" << QSysInfo::prettyProductName();
        qCInfo(main) << "Kernel:" << QSysInfo::kernelType() << QSysInfo::kernelVersion();
        qCInfo(main) << "CPU Architecture:" << QSysInfo::currentCpuArchitecture();
        qCInfo(main) << "Qt Build ABI:" << QSysInfo::buildAbi();
        qCInfo(main) << "Application Path:" << QCoreApplication::applicationDirPath();
        qCInfo(main) << "Working Directory:" << QDir::currentPath();
        
        // Check available disk space
        QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QStorageInfo storage(appDataPath);
        if (storage.isValid()) {
            qint64 availableGB = storage.bytesAvailable() / (1024 * 1024 * 1024);
            qCInfo(main) << "Available disk space:" << availableGB << "GB";
            
            if (availableGB < 1) {
                qCWarning(main) << "Low disk space warning - less than 1GB available";
            }
        }
        
        // Check memory usage
        #ifdef Q_OS_WIN
            MEMORYSTATUSEX memInfo;
            memInfo.dwLength = sizeof(MEMORYSTATUSEX);
            GlobalMemoryStatusEx(&memInfo);
            qint64 totalPhysMB = memInfo.ullTotalPhys / (1024 * 1024);
            qint64 availPhysMB = memInfo.ullAvailPhys / (1024 * 1024);
            qCInfo(main) << "Total RAM:" << totalPhysMB << "MB, Available:" << availPhysMB << "MB";
        #endif
        
        qCInfo(main) << "=========================";
    }
    
    bool verifyInstallation()
    {
        // Check if all required resources are available
        QStringList requiredResources = {
            ":/icons/earthquake_app.png",
            ":/icons/earthquake_large.png", 
            ":/icons/earthquake_alert.png",
            ":/sounds/alert.wav",
            ":/sounds/warning.wav",
            ":/sounds/emergency.wav"
        };
        
        bool allFound = true;
        for (const QString &resource : requiredResources) {
            if (!QFile::exists(resource)) {
                qCWarning(main) << "Missing resource:" << resource;
                allFound = false;
            }
        }
        
        if (!allFound) {
            qCWarning(main) << "Some resources are missing - application may not function correctly";
        }
        
        return allFound;
    }
    
    void createDesktopShortcut()
    {
        // Create desktop shortcut on first run (platform-specific)
        #ifdef Q_OS_WIN
        // Windows shortcut creation would go here
        #elif defined(Q_OS_LINUX)
        QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
        QString shortcutPath = desktopPath + "/Earthquake Alert System.desktop";
        
        if (!QFile::exists(shortcutPath)) {
            QFile shortcutFile(shortcutPath);
            if (shortcutFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&shortcutFile);
                out << "[Desktop Entry]\n";
                out << "Version=1.0\n";
                out << "Type=Application\n";
                out << "Name=" << APP_NAME << "\n";
                out << "Comment=" << APP_DESCRIPTION << "\n";
                out << "Exec=" << QCoreApplication::applicationFilePath() << "\n";
                out << "Icon=" << QCoreApplication::applicationDirPath() << "/earthquake.png\n";
                out << "Terminal=false\n";
                out << "Categories=Science;Education;\n";
                
                shortcutFile.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
                                          QFile::ReadGroup | QFile::ExeGroup |
                                          QFile::ReadOther | QFile::ExeOther);
                
                qCInfo(main) << "Desktop shortcut created:" << shortcutPath;
            }
        }
        #endif
    }
    
    void registerFileAssociations()
    {
        // Register file associations for earthquake data files
        #ifdef Q_OS_WIN
        // Windows registry operations will go here
        #elif defined(Q_OS_LINUX)
        QString mimeTypesPath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + 
                               "/mime/packages/earthquake-alert.xml";
        
        QDir().mkpath(QFileInfo(mimeTypesPath).absolutePath());
        
        QFile mimeFile(mimeTypesPath);
        if (!mimeFile.exists() && mimeFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&mimeFile);
            out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
            out << "<mime-info xmlns=\"http://www.freedesktop.org/standards/shared-mime-info\">\n";
            out << "  <mime-type type=\"application/x-earthquake-data\">\n";
            out << "    <comment>Earthquake Data File</comment>\n";
            out << "    <glob pattern=\"*.eqdata\"/>\n";
            out << "  </mime-type>\n";
            out << "</mime-info>\n";
            
            qCInfo(main) << "MIME types registered";
        }
        #endif
    }
    
    void setupCrashHandler()
    {
        // Basic crash handling setup
        #ifdef Q_OS_WIN
        // Windows crash handling (SetUnhandledExceptionFilter) would go here
        #elif defined(Q_OS_UNIX)
        struct sigaction sa;
        sa.sa_handler = [](int sig) {
            qCCritical(main) << "Application crashed with signal:" << sig;
            
            // Try to save any critical data
            QString crashLogPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + 
                                  "/earthquake_crash.log";
            QFile crashLog(crashLogPath);
            if (crashLog.open(QIODevice::WriteOnly | QIODevice::Append)) {
                QTextStream stream(&crashLog);
                stream << "CRASH: " << QDateTime::currentDateTime().toString() 
                       << " Signal: " << sig << "\n";
            }
            
            // Try graceful cleanup
            if (QApplication::instance()) {
                QApplication::quit();
            } else {
                _exit(1);
            }
        };
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        
        sigaction(SIGSEGV, &sa, nullptr);
        sigaction(SIGABRT, &sa, nullptr);
        sigaction(SIGFPE, &sa, nullptr);
        #endif
    }
}
