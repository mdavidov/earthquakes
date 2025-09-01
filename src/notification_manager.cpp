// NotificationManager.hpp
#ifndef NOTIFICATIONMANAGER_HPP
#define NOTIFICATIONMANAGER_HPP

#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtCore/QDateTime>
#include <QtCore/QSettings>
#include <QtCore/QQueue>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>
#include <QtCore/QStandardPaths>
#include <QtCore/QDir>
#include <QtCore/QTextStream>
#include <QtWidgets/QSystemTrayIcon>
#include <QtWidgets/QMenu>
#include <QtWidgets/QApplication>
#include <QtMultimedia/QSoundEffect>
#include <QtMultimedia/QAudioOutput>
#include <QtMultimedia/QMediaPlayer>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QVector>
#include <QMap>

// Forward declaration
struct EarthquakeData;

enum class NotificationType {
    Info,
    Warning,
    Critical,
    Emergency,
    SystemUpdate,
    NetworkStatus,
    DataUpdate
};

enum class NotificationPriority {
    Low = 1,
    Normal = 2,
    High = 3,
    Critical = 4,
    Emergency = 5
};

enum class DeliveryChannel {
    SystemTray,
    DesktopNotification,
    SoundAlert,
    EmailAlert,
    SMSAlert,
    PushNotification,
    LogFile,
    Console
};

enum class SoundType {
    None,
    Beep,
    Chime,
    Alert,
    Warning,
    Emergency,
    Custom
};

struct NotificationSettings {
    bool enabled = true;
    bool soundEnabled = true;
    bool systemTrayEnabled = true;
    bool desktopNotificationsEnabled = true;
    bool emailEnabled = false;
    bool smsEnabled = false;
    bool pushEnabled = false;
    
    double magnitudeThreshold = 5.0;
    int depthThreshold = 100; // km
    int proximityRadius = 500; // km from user location
    int quietHoursStart = 22; // 10 PM
    int quietHoursEnd = 7; // 7 AM
    bool respectQuietHours = true;
    
    QString emailAddress;
    QString smsNumber;
    QString pushServiceUrl;
    QString customSoundPath;
    
    SoundType defaultSoundType = SoundType::Alert;
    int notificationTimeout = 10000; // ms
    int maxNotificationsPerHour = 20;
    bool groupSimilarEvents = true;
    bool showPreview = true;
};

struct NotificationData {
    QString id;
    QString title;
    QString message;
    QString details;
    NotificationType type;
    NotificationPriority priority;
    QDateTime timestamp;
    QVector<DeliveryChannel> channels;
    QJsonObject metadata;
    bool acknowledged = false;
    bool persistent = false;
    int retryCount = 0;
    QDateTime expiryTime;
    QString sourceEventId;
};

struct AlertRule {
    QString name;
    bool enabled = true;
    double minMagnitude = 0.0;
    double maxMagnitude = 10.0;
    double minDepth = 0.0;
    double maxDepth = 1000.0;
    double centerLatitude = 0.0;
    double centerLongitude = 0.0;
    double radiusKm = 1000.0;
    bool useLocation = false;
    QStringList regions;
    NotificationPriority priority = NotificationPriority::Normal;
    QVector<DeliveryChannel> channels;
    QString customMessage;
    SoundType soundType = SoundType::Alert;
    int cooldownMinutes = 5;
    QDateTime lastTriggered;
};

class NotificationManager : public QObject
{
    Q_OBJECT

public:
    explicit NotificationManager(QObject *parent = nullptr);
    ~NotificationManager();

    // Configuration methods
    void setSettings(const NotificationSettings &settings);
    NotificationSettings getSettings() const;
    void saveSettings();
    void loadSettings();
    
    // Alert rule management
    void addAlertRule(const AlertRule &rule);
    void removeAlertRule(const QString &name);
    void updateAlertRule(const QString &name, const AlertRule &rule);
    QVector<AlertRule> getAlertRules() const;
    void setAlertRules(const QVector<AlertRule> &rules);
    
    // User location for proximity alerts
    void setUserLocation(double latitude, double longitude);
    QPair<double, double> getUserLocation() const;
    
    // Notification methods
    void showNotification(const NotificationData &notification);
    void showEarthquakeAlert(const EarthquakeData &earthquake);
    void showSystemNotification(const QString &title, const QString &message, 
                              NotificationType type = NotificationType::Info);
    void showNetworkStatusNotification(bool connected);
    void showDataUpdateNotification(int earthquakeCount);
    
    // Management methods
    void acknowledgeNotification(const QString &id);
    void acknowledgeAllNotifications();
    void clearExpiredNotifications();
    void clearAllNotifications();
    
    // Control methods
    void enableNotifications(bool enabled);
    void enableSounds(bool enabled);
    void enableQuietHours(bool enabled);
    void testNotification();
    void testAllChannels();
    
    // Status and information
    bool isEnabled() const;
    bool areSoundsEnabled() const;
    bool isInQuietHours() const;
    int getPendingNotificationsCount() const;
    int getTodayNotificationsCount() const;
    QVector<NotificationData> getRecentNotifications(int hours = 24) const;
    QVector<NotificationData> getUnacknowledgedNotifications() const;

signals:
    void notificationShown(const QString &id, NotificationType type);
    void notificationAcknowledged(const QString &id);
    void alertRuleTriggered(const QString &ruleName, const EarthquakeData &earthquake);
    void settingsChanged(const NotificationSettings &settings);
    void deliveryFailed(const QString &id, DeliveryChannel channel, const QString &error);
    void statisticsUpdated(int totalToday, int pending, int acknowledged);

private slots:
    void onSystemTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void processNotificationQueue();
    void cleanupExpiredNotifications();
    void updateStatistics();
    void onEmailSent();
    void onSmsSent();
    void onPushNotificationSent();
    void checkQuietHours();

private:
    // Initialization methods
    void initializeSystemTray();
    void initializeAudioSystem();
    void initializeNetworkManager();
    void createNotificationDirectory();
    void loadDefaultAlertRules();
    
    // Notification processing
    void processNotification(const NotificationData &notification);
    bool shouldShowNotification(const NotificationData &notification) const;
    QString generateNotificationId() const;
    void enqueueNotification(const NotificationData &notification);
    
    // Delivery methods
    void deliverToSystemTray(const NotificationData &notification);
    void deliverToDesktop(const NotificationData &notification);
    void deliverSoundAlert(const NotificationData &notification);
    void deliverEmailAlert(const NotificationData &notification);
    void deliverSmsAlert(const NotificationData &notification);
    void deliverPushNotification(const NotificationData &notification);
    void deliverToLogFile(const NotificationData &notification);
    void deliverToConsole(const NotificationData &notification);
    
    // Sound management
    void playSound(SoundType soundType, NotificationPriority priority);
    void playCustomSound(const QString &filePath, float volume = 1.0f);
    void stopAllSounds();
    QString getSoundFilePath(SoundType soundType) const;
    float calculateSoundVolume(NotificationPriority priority) const;
    
    // Alert rule processing
    QVector<AlertRule> getTriggeredRules(const EarthquakeData &earthquake) const;
    bool evaluateAlertRule(const AlertRule &rule, const EarthquakeData &earthquake) const;
    bool isRuleInCooldown(const AlertRule &rule) const;
    void updateRuleCooldown(const QString &ruleName);
    
    // Utility methods
    QString formatEarthquakeMessage(const EarthquakeData &earthquake) const;
    QString formatNotificationTime(const QDateTime &timestamp) const;
    NotificationPriority calculatePriority(const EarthquakeData &earthquake) const;
    QVector<DeliveryChannel> getChannelsForPriority(NotificationPriority priority) const;
    double calculateDistanceToUser(double lat, double lon) const;
    bool isInRegion(const EarthquakeData &earthquake, const QStringList &regions) const;
    
    // Rate limiting
    bool isRateLimited() const;
    void updateRateLimit();
    void resetDailyCounters();
    
    // Persistence methods
    void saveNotificationToFile(const NotificationData &notification);
    void loadPersistentNotifications();
    void savePersistentNotifications();
    QString getNotificationLogPath() const;
    
    // Email/SMS/Push helpers
    void sendEmail(const QString &to, const QString &subject, const QString &body);
    void sendSms(const QString &number, const QString &message);
    void sendPushNotification(const QString &title, const QString &message, const QJsonObject &data);
    
    // UI helpers
    QSystemTrayIcon::MessageIcon getSystemTrayIcon(NotificationType type) const;
    QString getNotificationTitle(NotificationType type) const;
    void updateSystemTrayTooltip();
    void createSystemTrayMenu();

private:
    // Settings and configuration
    NotificationSettings m_settings;
    QSettings *m_qsettings;
    
    // Alert rules
    QVector<AlertRule> m_alertRules;
    QMutex m_rulesMutex;
    
    // User location
    double m_userLatitude;
    double m_userLongitude;
    bool m_hasUserLocation;
    
    // Notification management
    QQueue<NotificationData> m_notificationQueue;
    QVector<NotificationData> m_activeNotifications;
    QVector<NotificationData> m_notificationHistory;
    QMutex m_notificationMutex;
    
    // System integration
    QSystemTrayIcon *m_systemTray;
    QMenu *m_trayMenu;
    
    // Audio system
    QSoundEffect *m_soundEffect;
    QMediaPlayer *m_mediaPlayer;
    QAudioOutput *m_audioOutput;
    QMap<SoundType, QString> m_soundPaths;
    
    // Network for external notifications
    QNetworkAccessManager *m_networkManager;
    
    // Timers
    QTimer *m_queueTimer;
    QTimer *m_cleanupTimer;
    QTimer *m_statisticsTimer;
    QTimer *m_quietHoursTimer;
    
    // Statistics and rate limiting
    int m_notificationsToday;
    int m_notificationsThisHour;
    QDateTime m_lastHourReset;
    QDateTime m_lastDayReset;
    QDateTime m_lastCleanup;
    
    // Status flags
    bool m_initialized;
    bool m_systemTrayAvailable;
    
    // File paths
    QString m_notificationLogFile;
    QString m_persistentDataFile;
    QString m_soundsDirectory;
    
    // Constants
    static const int MAX_QUEUE_SIZE;
    static const int CLEANUP_INTERVAL_MS;
    static const int STATISTICS_UPDATE_INTERVAL_MS;
    static const int MAX_NOTIFICATION_HISTORY;
    static const int DEFAULT_NOTIFICATION_TIMEOUT_MS;
};

#endif // NOTIFICATIONMANAGER_HPP

// NotificationManager.cpp
#include "NotificationManager.hpp"
#include "SpatialUtils.h" // For distance calculations
#include <QtWidgets/QMessageBox>
#include <QtCore/QUuid>
#include <QtCore/QJsonArray>
#include <QtCore/QCoreApplication>
#include <QtCore/QRegularExpression>
#include <QtCore/QStandardPaths>

// Include EarthquakeData definition
struct EarthquakeData {
    double latitude;
    double longitude;
    double magnitude;
    double depth;
    QDateTime timestamp;
    QString location;
    int alertLevel;
    QString eventId;
    QString dataSource;
    double uncertainty;
    QString tsunamiFlag;
    QString reviewStatus;
};

// Constants
const int NotificationManager::MAX_QUEUE_SIZE = 100;
const int NotificationManager::CLEANUP_INTERVAL_MS = 300000; // 5 minutes
const int NotificationManager::STATISTICS_UPDATE_INTERVAL_MS = 60000; // 1 minute
const int NotificationManager::MAX_NOTIFICATION_HISTORY = 1000;
const int NotificationManager::DEFAULT_NOTIFICATION_TIMEOUT_MS = 10000;

NotificationManager::NotificationManager(QObject *parent)
    : QObject(parent)
    , m_qsettings(nullptr)
    , m_userLatitude(0.0)
    , m_userLongitude(0.0)
    , m_hasUserLocation(false)
    , m_systemTray(nullptr)
    , m_trayMenu(nullptr)
    , m_soundEffect(nullptr)
    , m_mediaPlayer(nullptr)
    , m_audioOutput(nullptr)
    , m_networkManager(nullptr)
    , m_queueTimer(nullptr)
    , m_cleanupTimer(nullptr)
    , m_statisticsTimer(nullptr)
    , m_quietHoursTimer(nullptr)
    , m_notificationsToday(0)
    , m_notificationsThisHour(0)
    , m_initialized(false)
    , m_systemTrayAvailable(false)
{
    // Initialize settings
    m_qsettings = new QSettings("EarthquakeAlertSystem", "NotificationManager", this);
    
    // Initialize components
    initializeSystemTray();
    initializeAudioSystem();
    initializeNetworkManager();
    createNotificationDirectory();
    
    // Setup timers
    m_queueTimer = new QTimer(this);
    connect(m_queueTimer, &QTimer::timeout, this, &NotificationManager::processNotificationQueue);
    m_queueTimer->start(1000); // Process queue every second
    
    m_cleanupTimer = new QTimer(this);
    connect(m_cleanupTimer, &QTimer::timeout, this, &NotificationManager::cleanupExpiredNotifications);
    m_cleanupTimer->start(CLEANUP_INTERVAL_MS);
    
    m_statisticsTimer = new QTimer(this);
    connect(m_statisticsTimer, &QTimer::timeout, this, &NotificationManager::updateStatistics);
    m_statisticsTimer->start(STATISTICS_UPDATE_INTERVAL_MS);
    
    m_quietHoursTimer = new QTimer(this);
    connect(m_quietHoursTimer, &QTimer::timeout, this, &NotificationManager::checkQuietHours);
    m_quietHoursTimer->start(60000); // Check quiet hours every minute
    
    // Initialize rate limiting
    m_lastHourReset = QDateTime::currentDateTime();
    m_lastDayReset = QDateTime::currentDateTime();
    m_lastCleanup = QDateTime::currentDateTime();
    
    // Load settings and data
    loadSettings();
    loadDefaultAlertRules();
    loadPersistentNotifications();
    
    m_initialized = true;
    
    qDebug() << "NotificationManager initialized successfully";
}

NotificationManager::~NotificationManager()
{
    saveSettings();
    savePersistentNotifications();
    stopAllSounds();
}

void NotificationManager::initializeSystemTray()
{
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        m_systemTray = new QSystemTrayIcon(this);
        m_systemTray->setIcon(QIcon(":/icons/earthquake_alert.png"));
        
        connect(m_systemTray, &QSystemTrayIcon::activated,
                this, &NotificationManager::onSystemTrayActivated);
        
        createSystemTrayMenu();
        m_systemTray->show();
        m_systemTrayAvailable = true;
        
        qDebug() << "System tray initialized";
    } else {
        qWarning() << "System tray not available";
        m_systemTrayAvailable = false;
    }
}

void NotificationManager::initializeAudioSystem()
{
    // Initialize sound effect for short alerts
    m_soundEffect = new QSoundEffect(this);
    
    // Initialize media player for longer sounds
    m_audioOutput = new QAudioOutput(this);
    m_mediaPlayer = new QMediaPlayer(this);
    m_mediaPlayer->setAudioOutput(m_audioOutput);
    
    // Setup sound file paths
    m_soundsDirectory = ":/sounds/";
    m_soundPaths[SoundType::Beep] = m_soundsDirectory + "beep.wav";
    m_soundPaths[SoundType::Chime] = m_soundsDirectory + "chime.wav";
    m_soundPaths[SoundType::Alert] = m_soundsDirectory + "alert.wav";
    m_soundPaths[SoundType::Warning] = m_soundsDirectory + "warning.wav";
    m_soundPaths[SoundType::Emergency] = m_soundsDirectory + "emergency.wav";
    
    qDebug() << "Audio system initialized";
}

void NotificationManager::initializeNetworkManager()
{
    m_networkManager = new QNetworkAccessManager(this);
    
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, [this](QNetworkReply *reply) {
                // Handle network replies for email/SMS/push notifications
                reply->deleteLater();
            });
    
    qDebug() << "Network manager initialized";
}

void NotificationManager::createNotificationDirectory()
{
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appDataPath + "/notifications");
    
    m_notificationLogFile = appDataPath + "/notifications/notifications.log";
    m_persistentDataFile = appDataPath + "/notifications/persistent.json";
    
    qDebug() << "Notification directory created:" << appDataPath;
}

void NotificationManager::loadDefaultAlertRules()
{
    // Create default alert rules
    AlertRule significantRule;
    significantRule.name = "Significant Earthquakes";
    significantRule.enabled = true;
    significantRule.minMagnitude = 5.0;
    significantRule.priority = NotificationPriority::High;
    significantRule.channels = {DeliveryChannel::SystemTray, DeliveryChannel::DesktopNotification, 
                              DeliveryChannel::SoundAlert, DeliveryChannel::LogFile};
    significantRule.soundType = SoundType::Alert;
    significantRule.customMessage = "Significant earthquake detected: M{magnitude} - {location}";
    
    AlertRule majorRule;
    majorRule.name = "Major Earthquakes";
    majorRule.enabled = true;
    majorRule.minMagnitude = 7.0;
    majorRule.priority = NotificationPriority::Critical;
    majorRule.channels = {DeliveryChannel::SystemTray, DeliveryChannel::DesktopNotification, 
                         DeliveryChannel::SoundAlert, DeliveryChannel::EmailAlert, DeliveryChannel::LogFile};
    majorRule.soundType = SoundType::Warning;
    majorRule.customMessage = "MAJOR EARTHQUAKE: M{magnitude} - {location}";
    
    AlertRule emergencyRule;
    emergencyRule.name = "Great Earthquakes";
    emergencyRule.enabled = true;
    emergencyRule.minMagnitude = 8.0;
    emergencyRule.priority = NotificationPriority::Emergency;
    emergencyRule.channels = {DeliveryChannel::SystemTray, DeliveryChannel::DesktopNotification, 
                             DeliveryChannel::SoundAlert, DeliveryChannel::EmailAlert, 
                             DeliveryChannel::SMSAlert, DeliveryChannel::LogFile};
    emergencyRule.soundType = SoundType::Emergency;
    emergencyRule.customMessage = "GREAT EARTHQUAKE ALERT: M{magnitude} - {location} - SEEK IMMEDIATE SAFETY";
    
    AlertRule proximityRule;
    proximityRule.name = "Nearby Earthquakes";
    proximityRule.enabled = false; // Disabled by default until user sets location
    proximityRule.minMagnitude = 3.0;
    proximityRule.useLocation = true;
    proximityRule.radiusKm = 200.0;
    proximityRule.priority = NotificationPriority::High;
    proximityRule.channels = {DeliveryChannel::SystemTray, DeliveryChannel::DesktopNotification, 
                             DeliveryChannel::SoundAlert, DeliveryChannel::LogFile};
    proximityRule.soundType = SoundType::Chime;
    proximityRule.customMessage = "Earthquake near you: M{magnitude} - {distance}km away";
    
    m_alertRules = {significantRule, majorRule, emergencyRule, proximityRule};
    
    qDebug() << "Default alert rules loaded:" << m_alertRules.size();
}

void NotificationManager::setSettings(const NotificationSettings &settings)
{
    m_settings = settings;
    emit settingsChanged(settings);
    saveSettings();
}

NotificationSettings NotificationManager::getSettings() const
{
    return m_settings;
}

void NotificationManager::saveSettings()
{
    if (!m_qsettings) return;
    
    m_qsettings->beginGroup("General");
    m_qsettings->setValue("enabled", m_settings.enabled);
    m_qsettings->setValue("soundEnabled", m_settings.soundEnabled);
    m_qsettings->setValue("systemTrayEnabled", m_settings.systemTrayEnabled);
    m_qsettings->setValue("desktopNotificationsEnabled", m_settings.desktopNotificationsEnabled);
    m_qsettings->setValue("emailEnabled", m_settings.emailEnabled);
    m_qsettings->setValue("smsEnabled", m_settings.smsEnabled);
    m_qsettings->setValue("pushEnabled", m_settings.pushEnabled);
    m_qsettings->endGroup();
    
    m_qsettings->beginGroup("Thresholds");
    m_qsettings->setValue("magnitudeThreshold", m_settings.magnitudeThreshold);
    m_qsettings->setValue("depthThreshold", m_settings.depthThreshold);
    m_qsettings->setValue("proximityRadius", m_settings.proximityRadius);
    m_qsettings->setValue("quietHoursStart", m_settings.quietHoursStart);
    m_qsettings->setValue("quietHoursEnd", m_settings.quietHoursEnd);
    m_qsettings->setValue("respectQuietHours", m_settings.respectQuietHours);
    m_qsettings->endGroup();
    
    m_qsettings->beginGroup("External");
    m_qsettings->setValue("emailAddress", m_settings.emailAddress);
    m_qsettings->setValue("smsNumber", m_settings.smsNumber);
    m_qsettings->setValue("pushServiceUrl", m_settings.pushServiceUrl);
    m_qsettings->setValue("customSoundPath", m_settings.customSoundPath);
    m_qsettings->endGroup();
    
    m_qsettings->beginGroup("Behavior");
    m_qsettings->setValue("defaultSoundType", static_cast<int>(m_settings.defaultSoundType));
    m_qsettings->setValue("notificationTimeout", m_settings.notificationTimeout);
    m_qsettings->setValue("maxNotificationsPerHour", m_settings.maxNotificationsPerHour);
    m_qsettings->setValue("groupSimilarEvents", m_settings.groupSimilarEvents);
    m_qsettings->setValue("showPreview", m_settings.showPreview);
    m_qsettings->endGroup();
    
    m_qsettings->beginGroup("UserLocation");
    m_qsettings->setValue("latitude", m_userLatitude);
    m_qsettings->setValue("longitude", m_userLongitude);
    m_qsettings->setValue("hasLocation", m_hasUserLocation);
    m_qsettings->endGroup();
    
    m_qsettings->sync();
}

void NotificationManager::loadSettings()
{
    if (!m_qsettings) return;
    
    m_qsettings->beginGroup("General");
    m_settings.enabled = m_qsettings->value("enabled", true).toBool();
    m_settings.soundEnabled = m_qsettings->value("soundEnabled", true).toBool();
    m_settings.systemTrayEnabled = m_qsettings->value("systemTrayEnabled", true).toBool();
    m_settings.desktopNotificationsEnabled = m_qsettings->value("desktopNotificationsEnabled", true).toBool();
    m_settings.emailEnabled = m_qsettings->value("emailEnabled", false).toBool();
    m_settings.smsEnabled = m_qsettings->value("smsEnabled", false).toBool();
    m_settings.pushEnabled = m_qsettings->value("pushEnabled", false).toBool();
    m_qsettings->endGroup();
    
    m_qsettings->beginGroup("Thresholds");
    m_settings.magnitudeThreshold = m_qsettings->value("magnitudeThreshold", 5.0).toDouble();
    m_settings.depthThreshold = m_qsettings->value("depthThreshold", 100).toInt();
    m_settings.proximityRadius = m_qsettings->value("proximityRadius", 500).toInt();
    m_settings.quietHoursStart = m_qsettings->value("quietHoursStart", 22).toInt();
    m_settings.quietHoursEnd = m_qsettings->value("quietHoursEnd", 7).toInt();
    m_settings.respectQuietHours = m_qsettings->value("respectQuietHours", true).toBool();
    m_qsettings->endGroup();
    
    m_qsettings->beginGroup("External");
    m_settings.emailAddress = m_qsettings->value("emailAddress").toString();
    m_settings.smsNumber = m_qsettings->value("smsNumber").toString();
    m_settings.pushServiceUrl = m_qsettings->value("pushServiceUrl").toString();
    m_settings.customSoundPath = m_qsettings->value("customSoundPath").toString();
    m_qsettings->endGroup();
    
    m_qsettings->beginGroup("Behavior");
    m_settings.defaultSoundType = static_cast<SoundType>(m_qsettings->value("defaultSoundType", 
                                                        static_cast<int>(SoundType::Alert)).toInt());
    m_settings.notificationTimeout = m_qsettings->value("notificationTimeout", 10000).toInt();
    m_settings.maxNotificationsPerHour = m_qsettings->value("maxNotificationsPerHour", 20).toInt();
    m_settings.groupSimilarEvents = m_qsettings->value("groupSimilarEvents", true).toBool();
    m_settings.showPreview = m_qsettings->value("showPreview", true).toBool();
    m_qsettings->endGroup();
    
    m_qsettings->beginGroup("UserLocation");
    m_userLatitude = m_qsettings->value("latitude", 0.0).toDouble();
    m_userLongitude = m_qsettings->value("longitude", 0.0).toDouble();
    m_hasUserLocation = m_qsettings->value("hasLocation", false).toBool();
    m_qsettings->endGroup();
}

void NotificationManager::addAlertRule(const AlertRule &rule)
{
    QMutexLocker locker(&m_rulesMutex);
    
    // Check if rule with same name exists
    for (int i = 0; i < m_alertRules.size(); ++i) {
        if (m_alertRules[i].name == rule.name) {
            m_alertRules[i] = rule;
            qDebug() << "Updated alert rule:" << rule.name;
            return;
        }
    }
    
    m_alertRules.append(rule);
    qDebug() << "Added new alert rule:" << rule.name;
}

void NotificationManager::removeAlertRule(const QString &name)
{
    QMutexLocker locker(&m_rulesMutex);
    
    for (int i = 0; i < m_alertRules.size(); ++i) {
        if (m_alertRules[i].name == name) {
            m_alertRules.removeAt(i);
            qDebug() << "Removed alert rule:" << name;
            return;
        }
    }
}

void NotificationManager::updateAlertRule(const QString &name, const AlertRule &rule)
{
    QMutexLocker locker(&m_rulesMutex);
    
    for (int i = 0; i < m_alertRules.size(); ++i) {
        if (m_alertRules[i].name == name) {
            m_alertRules[i] = rule;
            qDebug() << "Updated alert rule:" << name;
            return;
        }
    }
}

QVector<AlertRule> NotificationManager::getAlertRules() const
{
    QMutexLocker locker(&m_rulesMutex);
    return m_alertRules;
}

void NotificationManager::setAlertRules(const QVector<AlertRule> &rules)
{
    QMutexLocker locker(&m_rulesMutex);
    m_alertRules = rules;
}

void NotificationManager::setUserLocation(double latitude, double longitude)
{
    m_userLatitude = latitude;
    m_userLongitude = longitude;
    m_hasUserLocation = true;
    
    // Enable proximity-based alert rules
    QMutexLocker locker(&m_rulesMutex);
    for (auto &rule : m_alertRules) {
        if (rule.useLocation && rule.name == "Nearby Earthquakes") {
            rule.enabled = true;
            rule.centerLatitude = latitude;
            rule.centerLongitude = longitude;
        }
    }
    
    qDebug() << "User location set:" << latitude << longitude;
}

QPair<double, double> NotificationManager::getUserLocation() const
{
    return qMakePair(m_userLatitude, m_userLongitude);
}

void NotificationManager::showNotification(const NotificationData &notification)
{
    if (!m_settings.enabled || !shouldShowNotification(notification)) {
        return;
    }
    
    if (isRateLimited()) {
        qDebug() << "Rate limited, skipping notification:" << notification.title;
        return;
    }
    
    // Create a copy with generated ID if needed
    NotificationData processedNotification = notification;
    if (processedNotification.id.isEmpty()) {
        processedNotification.id = generateNotificationId();
    }
    if (processedNotification.timestamp.isNull()) {
        processedNotification.timestamp = QDateTime::currentDateTime();
    }
    
    enqueueNotification(processedNotification);
    updateRateLimit();
}

void NotificationManager::showEarthquakeAlert(const EarthquakeData &earthquake)
{
    if (!m_settings.enabled) return;
    
    // Check alert rules
    QVector<AlertRule> triggeredRules = getTriggeredRules(earthquake);
    if (triggeredRules.isEmpty()) {
        return; // No rules triggered
    }
    
    // Use the highest priority rule
    AlertRule activeRule = triggeredRules.first();
    for (const auto &rule : triggeredRules) {
        if (rule.priority > activeRule.priority) {
            activeRule = rule;
        }
    }
    
    // Check cooldown
    if (isRuleInCooldown(activeRule)) {
        return;
    }
    
    // Create notification
    NotificationData notification;
    notification.id = generateNotificationId();
    notification.title = "Earthquake Alert";
    notification.message = formatEarthquakeMessage(earthquake);
    notification.type = (activeRule.priority >= NotificationPriority::Critical) ? 
                       NotificationType::Critical : NotificationType::Warning;
    notification.priority = activeRule.priority;
    notification.timestamp = QDateTime::currentDateTime();
    notification.channels = activeRule.channels;
    notification.sourceEventId = earthquake.eventId;
    notification.persistent = (activeRule.priority >= NotificationPriority::Critical);
    notification.expiryTime = QDateTime::currentDateTime().addSecs(3600); // 1 hour expiry
    
    // Add earthquake metadata
    QJsonObject metadata;
    metadata["magnitude"] = earthquake.magnitude;
    metadata["depth"] = earthquake.depth;
    metadata["latitude"] = earthquake.latitude;
    metadata["longitude"] = earthquake.longitude;
    metadata["location"] = earthquake.location;
    metadata["eventId"] = earthquake.eventId;
    metadata["dataSource"] = earthquake.dataSource;
    metadata["alertLevel"] = earthquake.alertLevel;
    metadata["ruleName"] = activeRule.name;
    metadata["soundType"] = static_cast<int>(activeRule.soundType);
    
    if (m_hasUserLocation) {
        double distance = calculateDistanceToUser(earthquake.latitude, earthquake.longitude);
        metadata["distanceKm"] = distance;
        notification.message += QString("\nDistance: %1 km").arg(distance, 0, 'f', 0);
    }
    
    notification.metadata = metadata;
    
    // Show notification
    showNotification(notification);
    
    // Update rule cooldown
    updateRuleCooldown(activeRule.name);
    
    // Emit signal
    emit alertRuleTriggered(activeRule.name, earthquake);
    
    qDebug() << "Earthquake alert shown for rule:" << activeRule.name 
             << "M" << earthquake.magnitude << earthquake.location;
}

void NotificationManager::showSystemNotification(const QString &title, const QString &message, NotificationType type)
{
    NotificationData notification;
    notification.title = title;
    notification.message = message;
    notification.type = type;
    notification.priority = NotificationPriority::Low;
    notification.channels = {DeliveryChannel::SystemTray, DeliveryChannel::LogFile};
    
    showNotification(notification);
}

void NotificationManager::showNetworkStatusNotification(bool connected)
{
    QString title = connected ? "Network Connected" : "Network Disconnected";
    QString message = connected ? "Earthquake data updates resumed" : "Unable to fetch earthquake data";
    NotificationType type = connected ? NotificationType::Info : NotificationType::Warning;
    
    showSystemNotification(title, message, type);
}

void NotificationManager::showDataUpdateNotification(int earthquakeCount)
{
    QString title = "Data Updated";
    QString message = QString("Received %1 earthquake updates").arg(earthquakeCount);
    
    showSystemNotification(title, message, NotificationType::DataUpdate);
}

void NotificationManager::acknowledgeNotification(const QString &id)
{
    QMutexLocker locker(&m_notificationMutex);
    
    for (auto &notification : m_activeNotifications) {
        if (notification.id == id) {
            notification.acknowledged = true;
            emit notificationAcknowledged(id);
            qDebug() << "Notification acknowledged:" << id;
            return;
        }
    }
}

void NotificationManager::acknowledgeAllNotifications()
{
    QMutexLocker locker(&m_notificationMutex);
    
    int count = 0;
    for (auto &notification : m_activeNotifications) {
        if (!notification.acknowledged) {
            notification.acknowledged = true;
            emit notificationAcknowledged(notification.id);
            count++;
        }
    }
    
    qDebug() << "Acknowledged all notifications, count:" << count;
}

void NotificationManager::clearExpiredNotifications()
{
    QMutexLocker locker(&m_notificationMutex);
    QDateTime now = QDateTime::currentDateTime();
    
    int removed = 0;
    auto it = m_activeNotifications.begin();
    while (it != m_activeNotifications.end()) {
        if (it->expiryTime.isValid() && now > it->expiryTime) {
            it = m_activeNotifications.erase(it);
            removed++;
        } else {
            ++it;
        }
    }
    
    if (removed > 0) {
        qDebug() << "Cleared" << removed << "expired notifications";
    }
}

void NotificationManager::clearAllNotifications()
{
    QMutexLocker locker(&m_notificationMutex);
    
    int count = m_activeNotifications.size();
    m_activeNotifications.clear();
    m_notificationQueue.clear();
    
    qDebug() << "Cleared all notifications, count:" << count;
}

void NotificationManager::enableNotifications(bool enabled)
{
    m_settings.enabled = enabled;
    saveSettings();
    emit settingsChanged(m_settings);
}

void NotificationManager::enableSounds(bool enabled)
{
    m_settings.soundEnabled = enabled;
    if (!enabled) {
        stopAllSounds();
    }
    saveSettings();
    emit settingsChanged(m_settings);
}

void NotificationManager::enableQuietHours(bool enabled)
{
    m_settings.respectQuietHours = enabled;
    saveSettings();
    emit settingsChanged(m_settings);
}

void NotificationManager::testNotification()
{
    NotificationData testNotification;
    testNotification.title = "Test Notification";
    testNotification.message = "This is a test earthquake alert notification.";
    testNotification.type = NotificationType::Warning;
    testNotification.priority = NotificationPriority::Normal;
    testNotification.channels = {DeliveryChannel::SystemTray, DeliveryChannel::DesktopNotification, 
                                DeliveryChannel::SoundAlert, DeliveryChannel::LogFile};
    
    // Add test earthquake metadata
    QJsonObject metadata;
    metadata["magnitude"] = 5.2;
    metadata["location"] = "Test Location";
    metadata["soundType"] = static_cast<int>(SoundType::Alert);
    testNotification.metadata = metadata;
    
    showNotification(testNotification);
}

void NotificationManager::testAllChannels()
{
    QVector<DeliveryChannel> allChannels = {
        DeliveryChannel::SystemTray,
        DeliveryChannel::DesktopNotification,
        DeliveryChannel::SoundAlert,
        DeliveryChannel::LogFile,
        DeliveryChannel::Console
    };
    
    if (m_settings.emailEnabled && !m_settings.emailAddress.isEmpty()) {
        allChannels.append(DeliveryChannel::EmailAlert);
    }
    if (m_settings.smsEnabled && !m_settings.smsNumber.isEmpty()) {
        allChannels.append(DeliveryChannel::SMSAlert);
    }
    if (m_settings.pushEnabled && !m_settings.pushServiceUrl.isEmpty()) {
        allChannels.append(DeliveryChannel::PushNotification);
    }
    
    for (DeliveryChannel channel : allChannels) {
        NotificationData testNotification;
        testNotification.title = QString("Channel Test: %1").arg(static_cast<int>(channel));
        testNotification.message = "Testing notification delivery channel";
        testNotification.type = NotificationType::Info;
        testNotification.priority = NotificationPriority::Low;
        testNotification.channels = {channel};
        
        showNotification(testNotification);
    }
}

bool NotificationManager::isEnabled() const
{
    return m_settings.enabled;
}

bool NotificationManager::areSoundsEnabled() const
{
    return m_settings.soundEnabled;
}

bool NotificationManager::isInQuietHours() const
{
    if (!m_settings.respectQuietHours) {
        return false;
    }
    
    QTime currentTime = QTime::currentTime();
    QTime startTime(m_settings.quietHoursStart, 0);
    QTime endTime(m_settings.quietHoursEnd, 0);
    
    // Handle overnight quiet hours (e.g., 22:00 to 07:00)
    if (startTime > endTime) {
        return currentTime >= startTime || currentTime <= endTime;
    } else {
        return currentTime >= startTime && currentTime <= endTime;
    }
}

int NotificationManager::getPendingNotificationsCount() const
{
    QMutexLocker locker(&m_notificationMutex);
    return m_notificationQueue.size();
}

int NotificationManager::getTodayNotificationsCount() const
{
    return m_notificationsToday;
}

QVector<NotificationData> NotificationManager::getRecentNotifications(int hours) const
{
    QMutexLocker locker(&m_notificationMutex);
    QDateTime cutoff = QDateTime::currentDateTime().addSecs(-hours * 3600);
    
    QVector<NotificationData> recent;
    for (const auto &notification : m_notificationHistory) {
        if (notification.timestamp >= cutoff) {
            recent.append(notification);
        }
    }
    
    return recent;
}

QVector<NotificationData> NotificationManager::getUnacknowledgedNotifications() const
{
    QMutexLocker locker(&m_notificationMutex);
    
    QVector<NotificationData> unacknowledged;
    for (const auto &notification : m_activeNotifications) {
        if (!notification.acknowledged) {
            unacknowledged.append(notification);
        }
    }
    
    return unacknowledged;
}

void NotificationManager::onSystemTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
        case QSystemTrayIcon::DoubleClick:
            // Show main window or notification list
            QApplication::setActiveWindow(QApplication::activeWindow());
            break;
        case QSystemTrayIcon::MiddleClick:
            acknowledgeAllNotifications();
            break;
        default:
            break;
    }
}

void NotificationManager::processNotificationQueue()
{
    if (m_notificationQueue.isEmpty()) {
        return;
    }
    
    QMutexLocker locker(&m_notificationMutex);
    NotificationData notification = m_notificationQueue.dequeue();
    locker.unlock();
    
    processNotification(notification);
}

void NotificationManager::cleanupExpiredNotifications()
{
    clearExpiredNotifications();
    
    // Cleanup history if too large
    QMutexLocker locker(&m_notificationMutex);
    if (m_notificationHistory.size() > MAX_NOTIFICATION_HISTORY) {
        int toRemove = m_notificationHistory.size() - MAX_NOTIFICATION_HISTORY;
        m_notificationHistory.remove(0, toRemove);
    }
}

void NotificationManager::updateStatistics()
{
    resetDailyCounters();
    
    QMutexLocker locker(&m_notificationMutex);
    int pending = m_notificationQueue.size();
    int acknowledged = 0;
    
    for (const auto &notification : m_activeNotifications) {
        if (notification.acknowledged) {
            acknowledged++;
        }
    }
    
    emit statisticsUpdated(m_notificationsToday, pending, acknowledged);
    updateSystemTrayTooltip();
}

void NotificationManager::checkQuietHours()
{
    // This runs every minute to check if we've entered/exited quiet hours
    static bool wasInQuietHours = false;
    bool nowInQuietHours = isInQuietHours();
    
    if (wasInQuietHours != nowInQuietHours) {
        QString message = nowInQuietHours ? "Entered quiet hours - notifications muted" : 
                                          "Exited quiet hours - notifications resumed";
        qDebug() << message;
        
        // Optional: Show a brief system notification about quiet hours change
        if (m_settings.systemTrayEnabled && m_systemTray) {
            m_systemTray->showMessage("Notification Manager", message, 
                                     QSystemTrayIcon::Information, 3000);
        }
    }
    
    wasInQuietHours = nowInQuietHours;
}

void NotificationManager::processNotification(const NotificationData &notification)
{
    // Skip if in quiet hours (except for emergency notifications)
    if (isInQuietHours() && notification.priority < NotificationPriority::Emergency) {
        qDebug() << "Skipping notification due to quiet hours:" << notification.title;
        return;
    }
    
    // Deliver to all specified channels
    for (DeliveryChannel channel : notification.channels) {
        try {
            switch (channel) {
                case DeliveryChannel::SystemTray:
                    deliverToSystemTray(notification);
                    break;
                case DeliveryChannel::DesktopNotification:
                    deliverToDesktop(notification);
                    break;
                case DeliveryChannel::SoundAlert:
                    deliverSoundAlert(notification);
                    break;
                case DeliveryChannel::EmailAlert:
                    deliverEmailAlert(notification);
                    break;
                case DeliveryChannel::SMSAlert:
                    deliverSmsAlert(notification);
                    break;
                case DeliveryChannel::PushNotification:
                    deliverPushNotification(notification);
                    break;
                case DeliveryChannel::LogFile:
                    deliverToLogFile(notification);
                    break;
                case DeliveryChannel::Console:
                    deliverToConsole(notification);
                    break;
            }
        } catch (const std::exception &e) {
            emit deliveryFailed(notification.id, channel, e.what());
            qWarning() << "Failed to deliver notification via channel" << static_cast<int>(channel) 
                      << ":" << e.what();
        }
    }
    
    // Add to active notifications and history
    QMutexLocker locker(&m_notificationMutex);
    m_activeNotifications.append(notification);
    m_notificationHistory.append(notification);
    
    emit notificationShown(notification.id, notification.type);
    
    qDebug() << "Processed notification:" << notification.title << "Channels:" << notification.channels.size();
}

bool NotificationManager::shouldShowNotification(const NotificationData &notification) const
{
    // Check if notifications are globally enabled
    if (!m_settings.enabled) {
        return false;
    }
    
    // Always show emergency notifications
    if (notification.priority == NotificationPriority::Emergency) {
        return true;
    }
    
    // Check rate limiting
    if (isRateLimited()) {
        return false;
    }
    
    // Check for duplicate notifications (grouping)
    if (m_settings.groupSimilarEvents && !notification.sourceEventId.isEmpty()) {
        QMutexLocker locker(&m_notificationMutex);
        for (const auto &existing : m_activeNotifications) {
            if (existing.sourceEventId == notification.sourceEventId && !existing.acknowledged) {
                return false; // Skip duplicate
            }
        }
    }
    
    return true;
}

QString NotificationManager::generateNotificationId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

void NotificationManager::enqueueNotification(const NotificationData &notification)
{
    QMutexLocker locker(&m_notificationMutex);
    
    if (m_notificationQueue.size() >= MAX_QUEUE_SIZE) {
        // Remove oldest non-critical notification
        for (int i = 0; i < m_notificationQueue.size(); ++i) {
            if (m_notificationQueue[i].priority < NotificationPriority::Critical) {
                m_notificationQueue.removeAt(i);
                break;
            }
        }
        
        // If still full, remove oldest
        if (m_notificationQueue.size() >= MAX_QUEUE_SIZE) {
            m_notificationQueue.dequeue();
        }
    }
    
    m_notificationQueue.enqueue(notification);
}

void NotificationManager::deliverToSystemTray(const NotificationData &notification)
{
    if (!m_settings.systemTrayEnabled || !m_systemTray || !m_systemTrayAvailable) {
        return;
    }
    
    QSystemTrayIcon::MessageIcon icon = getSystemTrayIcon(notification.type);
    int timeout = notification.persistent ? 0 : m_settings.notificationTimeout;
    
    QString title = notification.title;
    QString message = notification.message;
    
    // Truncate long messages for system tray
    if (message.length() > 200) {
        message = message.left(197) + "...";
    }
    
    m_systemTray->showMessage(title, message, icon, timeout);
}

void NotificationManager::deliverToDesktop(const NotificationData &notification)
{
    if (!m_settings.desktopNotificationsEnabled) {
        return;
    }
    
    // Use system tray for desktop notifications on most platforms
    deliverToSystemTray(notification);
}

void NotificationManager::deliverSoundAlert(const NotificationData &notification)
{
    if (!m_settings.soundEnabled) {
        return;
    }
    
    SoundType soundType = m_settings.defaultSoundType;
    
    // Use sound type from metadata if available
    if (notification.metadata.contains("soundType")) {
        soundType = static_cast<SoundType>(notification.metadata["soundType"].toInt());
    }
    
    playSound(soundType, notification.priority);
}

void NotificationManager::deliverEmailAlert(const NotificationData &notification)
{
    if (!m_settings.emailEnabled || m_settings.emailAddress.isEmpty()) {
        return;
    }
    
    QString subject = QString("[Earthquake Alert] %1").arg(notification.title);
    QString body = QString("%1\n\n%2\n\nTime: %3")
                   .arg(notification.message)
                   .arg(notification.details)
                   .arg(notification.timestamp.toString("yyyy-MM-dd hh:mm:ss UTC"));
    
    sendEmail(m_settings.emailAddress, subject, body);
}

void NotificationManager::deliverSmsAlert(const NotificationData &notification)
{
    if (!m_settings.smsEnabled || m_settings.smsNumber.isEmpty()) {
        return;
    }
    
    // SMS messages should be short
    QString message = QString("%1: %2").arg(notification.title, notification.message);
    if (message.length() > 160) {
        message = message.left(157) + "...";
    }
    
    sendSms(m_settings.smsNumber, message);
}

void NotificationManager::deliverPushNotification(const NotificationData &notification)
{
    if (!m_settings.pushEnabled || m_settings.pushServiceUrl.isEmpty()) {
        return;
    }
    
    sendPushNotification(notification.title, notification.message, notification.metadata);
}

void NotificationManager::deliverToLogFile(const NotificationData &notification)
{
    saveNotificationToFile(notification);
}

void NotificationManager::deliverToConsole(const NotificationData &notification)
{
    QString logMessage = QString("[%1] %2: %3")
                        .arg(notification.timestamp.toString("hh:mm:ss"))
                        .arg(notification.title)
                        .arg(notification.message);
    
    switch (notification.type) {
        case NotificationType::Critical:
        case NotificationType::Emergency:
            qCritical() << logMessage;
            break;
        case NotificationType::Warning:
            qWarning() << logMessage;
            break;
        default:
            qInfo() << logMessage;
            break;
    }
}

void NotificationManager::playSound(SoundType soundType, NotificationPriority priority)
{
    if (soundType == SoundType::None || !m_soundEffect) {
        return;
    }
    
    QString soundPath;
    if (soundType == SoundType::Custom && !m_settings.customSoundPath.isEmpty()) {
        soundPath = m_settings.customSoundPath;
    } else {
        soundPath = getSoundFilePath(soundType);
    }
    
    if (soundPath.isEmpty() || !QFile::exists(soundPath)) {
        qWarning() << "Sound file not found:" << soundPath;
        return;
    }
    
    float volume = calculateSoundVolume(priority);
    
    m_soundEffect->setSource(QUrl::fromLocalFile(soundPath));
    m_soundEffect->setVolume(volume);
    m_soundEffect->play();
    
    qDebug() << "Playing sound:" << soundPath << "Volume:" << volume;
}

void NotificationManager::playCustomSound(const QString &filePath, float volume)
{
    if (!m_soundEffect || !QFile::exists(filePath)) {
        return;
    }
    
    m_soundEffect->setSource(QUrl::fromLocalFile(filePath));
    m_soundEffect->setVolume(volume);
    m_soundEffect->play();
}

void NotificationManager::stopAllSounds()
{
    if (m_soundEffect) {
        m_soundEffect->stop();
    }
    if (m_mediaPlayer) {
        m_mediaPlayer->stop();
    }
}

QString NotificationManager::getSoundFilePath(SoundType soundType) const
{
    return m_soundPaths.value(soundType, "");
}

float NotificationManager::calculateSoundVolume(NotificationPriority priority) const
{
    switch (priority) {
        case NotificationPriority::Low: return 0.3f;
        case NotificationPriority::Normal: return 0.5f;
        case NotificationPriority::High: return 0.7f;
        case NotificationPriority::Critical: return 0.9f;
        case NotificationPriority::Emergency: return 1.0f;
        default: return 0.5f;
    }
}

QVector<AlertRule> NotificationManager::getTriggeredRules(const EarthquakeData &earthquake) const
{
    QMutexLocker locker(&m_rulesMutex);
    QVector<AlertRule> triggered;
    
    for (const auto &rule : m_alertRules) {
        if (rule.enabled && evaluateAlertRule(rule, earthquake)) {
            triggered.append(rule);
        }
    }
    
    return triggered;
}

bool NotificationManager::evaluateAlertRule(const AlertRule &rule, const EarthquakeData &earthquake) const
{
    // Check magnitude range
    if (earthquake.magnitude < rule.minMagnitude || earthquake.magnitude > rule.maxMagnitude) {
        return false;
    }
    
    // Check depth range
    if (earthquake.depth < rule.minDepth || earthquake.depth > rule.maxDepth) {
        return false;
    }
    
    // Check location-based rules
    if (rule.useLocation && m_hasUserLocation) {
        double distance = calculateDistanceToUser(earthquake.latitude, earthquake.longitude);
        if (distance > rule.radiusKm) {
            return false;
        }
    }
    
    // Check specific regions
    if (!rule.regions.isEmpty()) {
        if (!isInRegion(earthquake, rule.regions)) {
            return false;
        }
    }
    
    return true;
}

bool NotificationManager::isRuleInCooldown(const AlertRule &rule) const
{
    if (rule.cooldownMinutes <= 0) {
        return false;
    }
    
    QDateTime cooldownEnd = rule.lastTriggered.addSecs(rule.cooldownMinutes * 60);
    return QDateTime::currentDateTime() < cooldownEnd;
}

void NotificationManager::updateRuleCooldown(const QString &ruleName)
{
    QMutexLocker locker(&m_rulesMutex);
    
    for (auto &rule : m_alertRules) {
        if (rule.name == ruleName) {
            rule.lastTriggered = QDateTime::currentDateTime();
            break;
        }
    }
}

QString NotificationManager::formatEarthquakeMessage(const EarthquakeData &earthquake) const
{
    QString message = QString("M%1 earthquake - %2")
                     .arg(earthquake.magnitude, 0, 'f', 1)
                     .arg(earthquake.location);
    
    if (earthquake.depth > 0) {
        message += QString("\nDepth: %1 km").arg(earthquake.depth, 0, 'f', 0);
    }
    
    message += QString("\nTime: %1 UTC").arg(earthquake.timestamp.toString("hh:mm:ss"));
    
    if (earthquake.tsunamiFlag == "Yes") {
        message += "\n⚠️ TSUNAMI POSSIBLE";
    }
    
    return message;
}

QString NotificationManager::formatNotificationTime(const QDateTime &timestamp) const
{
    QDateTime now = QDateTime::currentDateTime();
    qint64 seconds = timestamp.secsTo(now);
    
    if (seconds < 60) {
        return "Just now";
    } else if (seconds < 3600) {
        return QString("%1 minutes ago").arg(seconds / 60);
    } else if (seconds < 86400) {
        return QString("%1 hours ago").arg(seconds / 3600);
    } else {
        return timestamp.toString("MMM d, hh:mm");
    }
}

NotificationPriority NotificationManager::calculatePriority(const EarthquakeData &earthquake) const
{
    if (earthquake.magnitude >= 8.0) return NotificationPriority::Emergency;
    if (earthquake.magnitude >= 7.0) return NotificationPriority::Critical;
    if (earthquake.magnitude >= 5.0) return NotificationPriority::High;
    if (earthquake.magnitude >= 3.0) return NotificationPriority::Normal;
    return NotificationPriority::Low;
}

QVector<DeliveryChannel> NotificationManager::getChannelsForPriority(NotificationPriority priority) const
{
    QVector<DeliveryChannel> channels = {DeliveryChannel::SystemTray, DeliveryChannel::LogFile};
    
    if (priority >= NotificationPriority::Normal) {
        channels.append(DeliveryChannel::DesktopNotification);
    }
    if (priority >= NotificationPriority::High) {
        channels.append(DeliveryChannel::SoundAlert);
    }
    if (priority >= NotificationPriority::Critical && m_settings.emailEnabled) {
        channels.append(DeliveryChannel::EmailAlert);
    }
    if (priority >= NotificationPriority::Emergency && m_settings.smsEnabled) {
        channels.append(DeliveryChannel::SMSAlert);
    }
    
    return channels;
}

double NotificationManager::calculateDistanceToUser(double lat, double lon) const
{
    if (!m_hasUserLocation) {
        return 0.0;
    }
    
    return SpatialUtils::haversineDistance(m_userLatitude, m_userLongitude, lat, lon);
}

bool NotificationManager::isInRegion(const EarthquakeData &earthquake, const QStringList &regions) const
{
    for (const QString &region : regions) {
        if (earthquake.location.contains(region, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

bool NotificationManager::isRateLimited() const
{
    return m_notificationsThisHour >= m_settings.maxNotificationsPerHour;
}

void NotificationManager::updateRateLimit()
{
    m_notificationsThisHour++;
    m_notificationsToday++;
    
    QDateTime now = QDateTime::currentDateTime();
    if (m_lastHourReset.secsTo(now) >= 3600) {
        m_notificationsThisHour = 0;
        m_lastHourReset = now;
    }
}

void NotificationManager::resetDailyCounters()
{
    QDateTime now = QDateTime::currentDateTime();
    if (m_lastDayReset.daysTo(now) >= 1) {
        m_notificationsToday = 0;
        m_lastDayReset = now;
    }
}

void NotificationManager::saveNotificationToFile(const NotificationData &notification)
{
    QFile logFile(m_notificationLogFile);
    if (!logFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
        qWarning() << "Failed to open notification log file:" << m_notificationLogFile;
        return;
    }
    
    QTextStream stream(&logFile);
    stream << notification.timestamp.toString(Qt::ISODate) << " | "
           << static_cast<int>(notification.type) << " | "
           << static_cast<int>(notification.priority) << " | "
           << notification.title << " | "
           << notification.message << " | "
           << notification.id << "\n";
    
    stream.flush();
}

void NotificationManager::loadPersistentNotifications()
{
    QFile dataFile(m_persistentDataFile);
    if (!dataFile.open(QIODevice::ReadOnly)) {
        return; // File doesn't exist yet, that's OK
    }
    
    QByteArray data = dataFile.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    
    if (!doc.isObject()) {
        return;
    }
    
    QJsonObject root = doc.object();
    QJsonArray notifications = root["notifications"].toArray();
    
    QMutexLocker locker(&m_notificationMutex);
    for (const auto &value : notifications) {
        QJsonObject obj = value.toObject();
        
        NotificationData notification;
        notification.id = obj["id"].toString();
        notification.title = obj["title"].toString();
        notification.message = obj["message"].toString();
        notification.type = static_cast<NotificationType>(obj["type"].toInt());
        notification.priority = static_cast<NotificationPriority>(obj["priority"].toInt());
        notification.timestamp = QDateTime::fromString(obj["timestamp"].toString(), Qt::ISODate);
        notification.acknowledged = obj["acknowledged"].toBool();
        notification.persistent = obj["persistent"].toBool();
        notification.sourceEventId = obj["sourceEventId"].toString();
        notification.metadata = obj["metadata"].toObject();
        
        if (notification.persistent && !notification.acknowledged) {
            m_activeNotifications.append(notification);
        }
    }
    
    qDebug() << "Loaded" << notifications.size() << "persistent notifications";
}

void NotificationManager::savePersistentNotifications()
{
    QJsonArray notifications;
    
    QMutexLocker locker(&m_notificationMutex);
    for (const auto &notification : m_activeNotifications) {
        if (notification.persistent) {
            QJsonObject obj;
            obj["id"] = notification.id;
            obj["title"] = notification.title;
            obj["message"] = notification.message;
            obj["type"] = static_cast<int>(notification.type);
            obj["priority"] = static_cast<int>(notification.priority);
            obj["timestamp"] = notification.timestamp.toString(Qt::ISODate);
            obj["acknowledged"] = notification.acknowledged;
            obj["persistent"] = notification.persistent;
            obj["sourceEventId"] = notification.sourceEventId;
            obj["metadata"] = notification.metadata;
            
            notifications.append(obj);
        }
    }
    
    QJsonObject root;
    root["notifications"] = notifications;
    root["saveTime"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    QJsonDocument doc(root);
    
    QFile dataFile(m_persistentDataFile);
    if (dataFile.open(QIODevice::WriteOnly)) {
        dataFile.write(doc.toJson());
        qDebug() << "Saved" << notifications.size() << "persistent notifications";
    } else {
        qWarning() << "Failed to save persistent notifications to:" << m_persistentDataFile;
    }
}

QString NotificationManager::getNotificationLogPath() const
{
    return m_notificationLogFile;
}

void NotificationManager::sendEmail(const QString &to, const QString &subject, const QString &body)
{
    // This is a simplified email implementation
    // In a real application, you would integrate with an email service like SMTP
    
    QJsonObject emailData;
    emailData["to"] = to;
    emailData["subject"] = subject;
    emailData["body"] = body;
    emailData["from"] = "earthquake-alerts@example.com";
    emailData["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    // Example: Send to email service API
    QNetworkRequest request(QUrl("https://api.emailservice.com/send"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QJsonDocument doc(emailData);
    QNetworkReply *reply = m_networkManager->post(request, doc.toJson());
    
    connect(reply, &QNetworkReply::finished, this, &NotificationManager::onEmailSent);
    
    qDebug() << "Email queued for sending to:" << to;
}

void NotificationManager::sendSms(const QString &number, const QString &message)
{
    // This is a simplified SMS implementation
    // In a real application, you would integrate with an SMS service like Twilio
    
    QJsonObject smsData;
    smsData["to"] = number;
    smsData["message"] = message;
    smsData["from"] = "+1234567890"; // Your SMS service number
    smsData["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    // Example: Send to SMS service API
    QNetworkRequest request(QUrl("https://api.smsservice.com/send"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QJsonDocument doc(smsData);
    QNetworkReply *reply = m_networkManager->post(request, doc.toJson());
    
    connect(reply, &QNetworkReply::finished, this, &NotificationManager::onSmsSent);
    
    qDebug() << "SMS queued for sending to:" << number;
}

void NotificationManager::sendPushNotification(const QString &title, const QString &message, const QJsonObject &data)
{
    // This is a simplified push notification implementation
    // In a real application, you would integrate with a push service like Firebase
    
    QJsonObject pushData;
    pushData["title"] = title;
    pushData["message"] = message;
    pushData["data"] = data;
    pushData["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    QNetworkRequest request(QUrl(m_settings.pushServiceUrl));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QJsonDocument doc(pushData);
    QNetworkReply *reply = m_networkManager->post(request, doc.toJson());
    
    connect(reply, &QNetworkReply::finished, this, &NotificationManager::onPushNotificationSent);
    
    qDebug() << "Push notification queued for sending";
}

void NotificationManager::onEmailSent()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    if (reply->error() == QNetworkReply::NoError) {
        qDebug() << "Email sent successfully";
    } else {
        qWarning() << "Failed to send email:" << reply->errorString();
        emit deliveryFailed("", DeliveryChannel::EmailAlert, reply->errorString());
    }
    
    reply->deleteLater();
}

void NotificationManager::onSmsSent()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    if (reply->error() == QNetworkReply::NoError) {
        qDebug() << "SMS sent successfully";
    } else {
        qWarning() << "Failed to send SMS:" << reply->errorString();
        emit deliveryFailed("", DeliveryChannel::SMSAlert, reply->errorString());
    }
    
    reply->deleteLater();
}

void NotificationManager::onPushNotificationSent()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    if (reply->error() == QNetworkReply::NoError) {
        qDebug() << "Push notification sent successfully";
    } else {
        qWarning() << "Failed to send push notification:" << reply->errorString();
        emit deliveryFailed("", DeliveryChannel::PushNotification, reply->errorString());
    }
    
    reply->deleteLater();
}

QSystemTrayIcon::MessageIcon NotificationManager::getSystemTrayIcon(NotificationType type) const
{
    switch (type) {
        case NotificationType::Info:
        case NotificationType::SystemUpdate:
        case NotificationType::DataUpdate:
            return QSystemTrayIcon::Information;
        case NotificationType::Warning:
        case NotificationType::NetworkStatus:
            return QSystemTrayIcon::Warning;
        case NotificationType::Critical:
        case NotificationType::Emergency:
            return QSystemTrayIcon::Critical;
        default:
            return QSystemTrayIcon::Information;
    }
}

QString NotificationManager::getNotificationTitle(NotificationType type) const
{
    switch (type) {
        case NotificationType::Info:
            return "Information";
        case NotificationType::Warning:
            return "Warning";
        case NotificationType::Critical:
            return "Critical Alert";
        case NotificationType::Emergency:
            return "EMERGENCY";
        case NotificationType::SystemUpdate:
            return "System Update";
        case NotificationType::NetworkStatus:
            return "Network Status";
        case NotificationType::DataUpdate:
            return "Data Update";
        default:
            return "Notification";
    }
}

void NotificationManager::updateSystemTrayTooltip()
{
    if (!m_systemTray) return;
    
    int unacknowledged = getUnacknowledgedNotifications().size();
    QString tooltip = QString("Earthquake Alert System\n%1 notifications today")
                     .arg(m_notificationsToday);
    
    if (unacknowledged > 0) {
        tooltip += QString("\n%1 unacknowledged alerts").arg(unacknowledged);
    }
    
    if (!m_settings.enabled) {
        tooltip += "\n(Notifications disabled)";
    } else if (isInQuietHours()) {
        tooltip += "\n(Quiet hours active)";
    }
    
    m_systemTray->setToolTip(tooltip);
}

void NotificationManager::createSystemTrayMenu()
{
    if (!m_systemTray) return;
    
    m_trayMenu = new QMenu();
    
    // Status section
    QAction *statusAction = m_trayMenu->addAction(QString("Notifications: %1")
                                                 .arg(m_settings.enabled ? "Enabled" : "Disabled"));
    statusAction->setEnabled(false);
    
    m_trayMenu->addSeparator();
    
    // Control actions
    QAction *toggleAction = m_trayMenu->addAction(m_settings.enabled ? "Disable Notifications" : "Enable Notifications");
    connect(toggleAction, &QAction::triggered, [this]() {
        enableNotifications(!m_settings.enabled);
    });
    
    QAction *toggleSoundAction = m_trayMenu->addAction(m_settings.soundEnabled ? "Mute Sounds" : "Enable Sounds");
    connect(toggleSoundAction, &QAction::triggered, [this]() {
        enableSounds(!m_settings.soundEnabled);
    });
    
    m_trayMenu->addSeparator();
    
    // Utility actions
    QAction *testAction = m_trayMenu->addAction("Test Notification");
    connect(testAction, &QAction::triggered, this, &NotificationManager::testNotification);
    
    QAction *acknowledgeAction = m_trayMenu->addAction("Acknowledge All");
    connect(acknowledgeAction, &QAction::triggered, this, &NotificationManager::acknowledgeAllNotifications);
    
    QAction *clearAction = m_trayMenu->addAction("Clear All");
    connect(clearAction, &QAction::triggered, this, &NotificationManager::clearAllNotifications);
    
    m_trayMenu->addSeparator();
    
    // Exit action
    QAction *exitAction = m_trayMenu->addAction("Exit");
    connect(exitAction, &QAction::triggered, QApplication::instance(), &QApplication::quit);
    
    m_systemTray->setContextMenu(m_trayMenu);
}

// Example integration and usage:
/*
// Example integration with EarthquakeMainWindow
class EarthquakeMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    EarthquakeMainWindow(QWidget *parent = nullptr) : QMainWindow(parent)
    {
        m_notificationManager = new NotificationManager(this);
        connectNotificationSignals();
        configureNotifications();
    }

private slots:
    void onNewEarthquakeReceived(const EarthquakeData &earthquake)
    {
        // Show earthquake alert through notification manager
        m_notificationManager->showEarthquakeAlert(earthquake);
    }
    
    void onNotificationShown(const QString &id, NotificationType type)
    {
        // Update UI to show notification indicator
        updateNotificationIndicator();
    }
    
    void onAlertRuleTriggered(const QString &ruleName, const EarthquakeData &earthquake)
    {
        // Log alert rule trigger for statistics
        qDebug() << "Alert rule triggered:" << ruleName << "for M" << earthquake.magnitude;
    }

private:
    void connectNotificationSignals()
    {
        connect(m_notificationManager, &NotificationManager::notificationShown,
                this, &EarthquakeMainWindow::onNotificationShown);
        connect(m_notificationManager, &NotificationManager::alertRuleTriggered,
                this, &EarthquakeMainWindow::onAlertRuleTriggered);
    }
    
    void configureNotifications()
    {
        // Set user location for proximity alerts
        m_notificationManager->setUserLocation(37.7749, -122.4194); // San Francisco
        
        // Configure notification settings
        NotificationSettings settings = m_notificationManager->getSettings();
        settings.magnitudeThreshold = 4.5;
        settings.proximityRadius = 300; // 300km
        settings.emailEnabled = true;
        settings.emailAddress = "user@example.com";
        m_notificationManager->setSettings(settings);
        
        // Add custom alert rule
        AlertRule customRule;
        customRule.name = "Pacific Ring of Fire";
        customRule.enabled = true;
        customRule.minMagnitude = 6.0;
        customRule.regions = {"Pacific", "Japan", "California", "Chile", "Indonesia"};
        customRule.priority = NotificationPriority::High;
        customRule.channels = {DeliveryChannel::SystemTray, DeliveryChannel::SoundAlert, 
                              DeliveryChannel::EmailAlert};
        m_notificationManager->addAlertRule(customRule);
    }

private:
    NotificationManager *m_notificationManager;
};

// Example usage scenarios:
void exampleUsage()
{
    NotificationManager manager;
    
    // Configure basic settings
    NotificationSettings settings;
    settings.enabled = true;
    settings.soundEnabled = true;
    settings.magnitudeThreshold = 5.0;
    settings.emailEnabled = true;
    settings.emailAddress = "alerts@example.com";
    manager.setSettings(settings);
    
    // Set user location for proximity alerts
    manager.setUserLocation(34.0522, -118.2437); // Los Angeles
    
    // Create custom alert rules
    AlertRule tsunamiRule;
    tsunamiRule.name = "Tsunami Risk";
    tsunamiRule.enabled = true;
    tsunamiRule.minMagnitude = 7.5;
    tsunamiRule.maxDepth = 50.0; // Shallow earthquakes more likely to cause tsunamis
    tsunamiRule.priority = NotificationPriority::Emergency;
    tsunamiRule.channels = {DeliveryChannel::SystemTray, DeliveryChannel::SoundAlert,
                           DeliveryChannel::EmailAlert, DeliveryChannel::SMSAlert};
    tsunamiRule.soundType = SoundType::Emergency;
    manager.addAlertRule(tsunamiRule);
    
    // Simulate earthquake alert
    EarthquakeData earthquake;
    earthquake.magnitude = 7.8;
    earthquake.depth = 15.0;
    earthquake.latitude = 35.0;
    earthquake.longitude = -120.0;
    earthquake.location = "Central California";
    earthquake.eventId = "us123456789";
    earthquake.timestamp = QDateTime::currentDateTimeUtc();
    earthquake.tsunamiFlag = "Yes";
    
    manager.showEarthquakeAlert(earthquake);
    
    // Test all notification channels
    manager.testAllChannels();
}
*/
    