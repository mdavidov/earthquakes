#include "geojson_parser.hpp"
#include <QDebug>


GeoJsonParser::ParseResult GeoJsonParser::parseUSGSGeoJson(const QByteArray& jsonData) {
    QElapsedTimer timer;
    timer.start();
    
    ParseResult result;
    result.success = false;
    result.totalFeatures = 0;
    
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonData, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        result.errorMessage = "JSON Parse Error: " + parseError.errorString();
        result.parseTimeMs = timer.elapsed();
        return result;
    }
    
    QJsonObject root = doc.object();
    
    if (!validateGeoJsonStructure(root)) {
        result.errorMessage = "Invalid GeoJSON structure";
        result.parseTimeMs = timer.elapsed();
        return result;
    }
    
    QJsonArray features = root["features"].toArray();
    result.totalFeatures = features.size();
    result.earthquakes.reserve(features.size());
    
    for (const auto& featureValue : features) {
        QJsonObject feature = featureValue.toObject();
        try {
            EarthquakeData earthquake(feature);
            if (earthquake.location.isValid() && earthquake.magnitude > 0) {
                result.earthquakes.push_back(earthquake);
            }
        } catch (const std::exception& e) {
            qDebug() << "Failed to parse earthquake feature:" << e.what();
        }
    }
    
    result.success = true;
    result.parseTimeMs = timer.elapsed();
    return result;
}

bool GeoJsonParser::validateGeoJsonStructure(const QJsonObject& root) {
    return root.contains("type") && 
           root["type"].toString() == "FeatureCollection" &&
           root.contains("features") &&
           root["features"].isArray();
}
