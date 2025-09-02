#pragma once

#include <QString>
#include <QDateTime>
#include <QtPositioning/QGeoCoordinate>
#include <QJsonObject>

struct EarthquakeData {
    QString eventId;
    double latitude;
    double longitude;
    double magnitude;
    int alertLevel;
    QGeoCoordinate location;
    QDateTime timestamp;
    QString place;
    QString url;
    double depth;
    QString type;
    QString dataSource;
    double uncertainty;
    QString tsunamiFlag;
    QString reviewStatus;
    
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
