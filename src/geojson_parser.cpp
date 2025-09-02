#include "geojson_parser.hpp"

#include <cmath>
#include <QGeoCoordinate>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QDebug>
#include <QByteArray>


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


QGeoCoordinate GeoJsonParser::parseCoordinate(const QString& input) {
    QStringList parts = input.split(',', Qt::SkipEmptyParts);
    if (parts.size() < 2)
        return QGeoCoordinate(); // invalid

    bool ok1 = false, ok2 = false;
    const auto lat = parts[0].trimmed().toDouble(&ok1);
    const auto lon = parts[1].trimmed().toDouble(&ok2);
    if (!ok1 || !ok2)
        return QGeoCoordinate(); // invalid

    if (parts.size() == 3) {
        bool ok3 = false;
        const auto alt = parts[2].trimmed().toDouble(&ok3);
        return ok3 ? QGeoCoordinate(lat, lon, alt) : QGeoCoordinate(lat, lon);
    }
    return QGeoCoordinate(lat, lon);
}


QGeoCoordinate GeoJsonParser::parseDMSCoordinate(const QString& input) {

    QRegularExpression regex(R"((\d+)°(\d+)'(\d+(?:\.\d+)?)\"([NS]),\s*(\d+)°(\d+)'(\d+(?:\.\d+)?)\"([EW]))");
    QRegularExpressionMatch match = regex.match(input.trimmed());
    if (!match.hasMatch())
        return QGeoCoordinate(); // invalid

    int latDeg = match.captured(1).toInt();
    int latMin = match.captured(2).toInt();
    double latSec = match.captured(3).toDouble();
    QChar latHem = match.captured(4).at(0);

    int lonDeg = match.captured(5).toInt();
    int lonMin = match.captured(6).toInt();
    double lonSec = match.captured(7).toDouble();
    QChar lonHem = match.captured(8).at(0);

    double latitude = GeoJsonParser::dmsToDecimal(latDeg, latMin, latSec, latHem);
    double longitude = GeoJsonParser::dmsToDecimal(lonDeg, lonMin, lonSec, lonHem);

    return QGeoCoordinate(latitude, longitude);
}


double GeoJsonParser::dmsToDecimal(int deg, int min, double sec, QChar hemisphere) {
    double decimal = deg + min / 60.0 + sec / 3600.0;
    if (hemisphere == 'S' || hemisphere == 'W')
        decimal *= -1;
    return decimal;
}


QJsonObject GeoJsonParser::coordinateToJson(const QGeoCoordinate& coord) {
    QJsonObject json;
    if (!coord.isValid())
        return json;

    json["latitude"] = coord.latitude();
    json["longitude"] = coord.longitude();

    if (coord.type() == QGeoCoordinate::Coordinate3D) {
        json["altitude"] = coord.altitude();
    }
    return json;
}


QString GeoJsonParser::coordinateToString(const QGeoCoordinate& coord,
                                          QGeoCoordinate::CoordinateFormat format) {
    return coord.isValid() ? coord.toString(format) : QString("Invalid coordinate");
}
