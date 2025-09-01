#pragma once

#pragma once
#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtCore/QDateTime>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QUrlQuery>
#include <QtCore/QDebug>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QSslConfiguration>
#include <QVector>
#include <QQueue>
#include <QMutex>
#include <QMutexLocker>
#include "earthquake_data.hpp"

enum class ApiDataSource {
    USGS_All_Hour,
    USGS_All_Day,
    USGS_All_Week,
    USGS_All_Month,
    USGS_Significant_Month,
    EMSC_Latest,
    JMA_Latest,
    Custom
};

enum class ApiRequestType {
    InitialLoad,
    Refresh,
    HistoricalData,
    SpecificEvent,
    RegionalData
};

struct ApiRequest {
    ApiRequestType type;
    ApiDataSource source;
    QDateTime startTime;
    QDateTime endTime;
    double minLatitude;
    double maxLatitude;
    double minLongitude;
    double maxLongitude;
    double minMagnitude;
    double maxMagnitude;
    QString eventId;
    QNetworkReply *reply;
    int retryCount;
};

class EarthquakeApiClient : public QObject
{
    Q_OBJECT

public:
    explicit EarthquakeApiClient(QObject *parent = nullptr);
    ~EarthquakeApiClient();

    // Configuration methods
    void setApiKey(const QString &apiKey);
    void setUserAgent(const QString &userAgent);
    void setTimeout(int timeoutMs);
    void setMaxRetries(int maxRetries);
    void setRateLimitDelay(int delayMs);
    void setCustomApiUrl(const QString &url);

    // Data fetching methods
    void fetchAllEarthquakes(ApiDataSource source = ApiDataSource::USGS_All_Day);
    void fetchRecentEarthquakes(int hours = 24);
    void fetchEarthquakesByRegion(double minLat, double maxLat, double minLon, double maxLon);
    void fetchEarthquakesByMagnitude(double minMag, double maxMag);
    void fetchEarthquakesByTimeRange(const QDateTime &start, const QDateTime &end);
    void fetchSpecificEarthquake(const QString &eventId);
    void fetchSignificantEarthquakes();

    // Control methods
    void startAutoRefresh(int intervalMinutes = 5);
    void stopAutoRefresh();
    void cancelAllRequests();
    void clearCache();

    // Status and information
    bool isConnected() const;
    QDateTime getLastUpdateTime() const;
    int getPendingRequestsCount() const;
    QString getLastError() const;
    QVector<QString> getAvailableDataSources() const;

signals:
    void earthquakeDataReceived(const QVector<EarthquakeData> &earthquakes, ApiRequestType requestType);
    void singleEarthquakeReceived(const EarthquakeData &earthquake);
    void dataSourceChanged(ApiDataSource source);
    void requestStarted(ApiRequestType type);
    void requestFinished(ApiRequestType type, bool success);
    void errorOccurred(const QString &error, ApiRequestType requestType);
    void networkStatusChanged(bool connected);
    void rateLimitReached(int waitTimeMs);

private slots:
    void onNetworkReplyFinished();
    void onSslErrors(const QList<QSslError> &errors);
    void onTimeout();
    void processRequestQueue();

private:
    // URL building methods
    QString buildUsgsUrl(ApiDataSource source) const;
    QString buildEmscUrl() const;
    QString buildJmaUrl() const;
    QString buildCustomUrl(const ApiRequest &request) const;
    
    // Request management
    void enqueueRequest(const ApiRequest &request);
    void executeRequest(const ApiRequest &request);
    void retryRequest(const ApiRequest &request);
    void handleRequestError(const ApiRequest &request, const QString &error);
    
    // Data parsing methods
    QVector<EarthquakeData> parseUsgsGeoJson(const QByteArray &data, ApiRequestType requestType);
    QVector<EarthquakeData> parseEmscData(const QByteArray &data);
    QVector<EarthquakeData> parseJmaData(const QByteArray &data);
    EarthquakeData parseUsgsFeature(const QJsonObject &feature, const QString &source = "USGS");
    
    // Utility methods
    void updateNetworkStatus();
    bool isRateLimited() const;
    void enforceRateLimit();
    QString formatApiUrl(const QString &baseUrl, const QUrlQuery &params) const;
    void logApiCall(const QString &url, ApiRequestType type);
    void updateStatistics(int earthquakeCount, ApiRequestType type);
    
    // Validation methods
    bool validateEarthquakeData(const EarthquakeData &earthquake) const;
    bool isValidCoordinate(double lat, double lon) const;
    bool isValidMagnitude(double magnitude) const;
    bool isValidDepth(double depth) const;
    
    // Cache management
    void cacheResponse(const QString &url, const QByteArray &data);
    QByteArray getCachedResponse(const QString &url) const;
    void cleanExpiredCache();

private:
    // Network components
    QNetworkAccessManager *m_networkManager;
    QTimer *m_refreshTimer;
    QTimer *m_timeoutTimer;
    QTimer *m_rateLimitTimer;
    QTimer *m_queueTimer;
    
    // Configuration
    QString m_apiKey;
    QString m_userAgent;
    QString m_customApiUrl;
    int m_timeoutMs;
    int m_maxRetries;
    int m_rateLimitDelayMs;
    int m_refreshIntervalMinutes;
    
    // Request management
    QQueue<ApiRequest> m_requestQueue;
    QVector<ApiRequest> m_activeRequests;
    QMutex m_requestMutex;
    
    // Status tracking
    bool m_isConnected;
    QDateTime m_lastUpdateTime;
    QDateTime m_lastRequestTime;
    QString m_lastError;
    int m_totalRequestsToday;
    int m_successfulRequests;
    int m_failedRequests;
    
    // Data sources configuration
    QMap<ApiDataSource, QString> m_dataSources;
    ApiDataSource m_currentDataSource;
    
    // Cache system
    QMap<QString, QPair<QByteArray, QDateTime>> m_responseCache;
    int m_cacheExpiryMinutes;
    int m_maxCacheSize;
    
    // Rate limiting
    QDateTime m_lastApiCall;
    int m_callsThisMinute;
    int m_maxCallsPerMinute;
    
    // Constants
    static const int DEFAULT_TIMEOUT_MS;
    static const int DEFAULT_MAX_RETRIES;
    static const int DEFAULT_RATE_LIMIT_MS;
    static const int DEFAULT_CACHE_EXPIRY_MINUTES;
    static const int DEFAULT_MAX_CACHE_SIZE;
    static const int DEFAULT_MAX_CALLS_PER_MINUTE;
};
