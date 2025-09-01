#pragma once
#include <QString>
#include <QDateTime>
#include <QGeoCoordinate>
#include <QJsonObject>

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

struct EarthquakeData2 {
    QString id;
    double magnitude;
    QGeoCoordinate location;
    QDateTime timestamp;
    QString place;
    QString url;
    double depth;
    QString type;
    
    // Constructor from USGS GeoJSON feature
    explicit EarthquakeData(const QJsonObject& feature);
    
    // Default constructor
    EarthquakeData() = default;
    
    // Comparison operators for sorting
    bool operator<(const EarthquakeData& other) const {
        return magnitude < other.magnitude;
    }
    
    bool operator>(const EarthquakeData& other) const {
        return magnitude > other.magnitude;
    }
};
