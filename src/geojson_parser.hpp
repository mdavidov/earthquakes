#pragma once
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QElapsedTimer>
#include <vector>
#include "earthquake_data.hpp"

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
    
private:
    static bool validateGeoJsonStructure(const QJsonObject& root);
    static EarthquakeData parseFeature(const QJsonObject& feature);
};
