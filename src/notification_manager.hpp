
#pragma once

#include "earthquake_data.hpp"

#include <QObject>
#include <QTimer>
#include <QDateTime>
#include <QSettings>
#include <QQueue>
#include <QMutex>
#include <QMutexLocker>
#include <QJsonObject>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QDir>
#include <QTextStream>
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
    mutable QMutex m_rulesMutex;

    // User location
    double m_userLatitude;
    double m_userLongitude;
    bool m_hasUserLocation;
    
    // Notification management
    QQueue<NotificationData> m_notificationQueue;
    QVector<NotificationData> m_activeNotifications;
    QVector<NotificationData> m_notificationHistory;
    mutable QMutex m_notificationMutex;
    
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

Q_DECLARE_METATYPE(NotificationManager)

// #include <QObject>
// #include <QSystemTrayIcon>
// #include <QMenu>
// #include <QAction>
// #include <QTimer>
// #include <set>
// #include "earthquake_data.hpp"
//
// class NotificationManager : public QObject {
//     Q_OBJECT
//
// public:
//     explicit NotificationManager(QObject* parent = nullptr);
//
//     void setMagnitudeThreshold(double threshold);
//     void setNotificationsEnabled(bool enabled);
//     void checkForAlerts(const std::vector<EarthquakeData>& earthquakes);
//
// signals:
//     void alertTriggered(const EarthquakeData& earthquake);
//
// private slots:
//     void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
//     void clearNotifiedEarthquakes();
//
// private:
//     QSystemTrayIcon* trayIcon;
//     QMenu* trayMenu;
//     QAction* toggleAction;
//     QTimer* cleanupTimer;
//
//     double magnitudeThreshold;
//     bool notificationsEnabled;
//     std::set<QString> notifiedEarthquakes;
//
//     void setupTrayIcon();
//     void showEarthquakeNotification(const EarthquakeData& earthquake);
// };
