#pragma once
#include "earthquake_data.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QElapsedTimer>
#include <QGeoCoordinate>
#include <vector>
class QByteArray;

class GeoJsonParser {
public:
    struct ParseResult {
        std::vector<EarthquakeData> earthquakes;
        int totalFeatures;
        qint64 parseTimeMs;
        bool success;
        QString errorMessage;
    };
    
    static ParseResult parseUSGSGeoJson(const QByteArray& jsonData);
    static ParseResult parseUSGSGeoJsonOptimized(const QByteArray& jsonData);

    static QGeoCoordinate parseCoordinate(const QString& input);
    static QGeoCoordinate parseDMSCoordinate(const QString& input);
    static double dmsToDecimal(int deg, int min, double sec, QChar hemisphere);
    static QJsonObject coordinateToJson(const QGeoCoordinate& coord);
    static QString coordinateToString(const QGeoCoordinate& coord,
                           QGeoCoordinate::CoordinateFormat format =
                               QGeoCoordinate::DegreesMinutesSecondsWithHemisphere);

private:
    static bool validateGeoJsonStructure(const QJsonObject& root);
    static EarthquakeData parseFeature(const QJsonObject& feature);
};
