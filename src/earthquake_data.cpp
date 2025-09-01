#include "earthquake_data.hpp"
#include <QJsonArray>


EarthquakeData::EarthquakeData(const QJsonObject& feature) {
    auto properties = feature["properties"].toObject();
    auto geometry = feature["geometry"].toObject();
    auto coordinates = geometry["coordinates"].toArray();
    
    id = feature["id"].toString();
    magnitude = properties["mag"].toDouble();
    place = properties["place"].toString();
    url = properties["url"].toString();
    depth = coordinates.size() > 2 ? coordinates[2].toDouble() : 0.0;
    type = properties["type"].toString();
    
    // Convert timestamp from milliseconds to QDateTime
    qint64 timeMs = properties["time"].toVariant().toLongLong();
    timestamp = QDateTime::fromMSecsSinceEpoch(timeMs);
    
    // Extract coordinates (longitude, latitude)
    if (coordinates.size() >= 2) {
        double longitude = coordinates[0].toDouble();
        double latitude = coordinates[1].toDouble();
        location = QGeoCoordinate(latitude, longitude);
    }
}
