#pragma once
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QGeoCoordinate>
#include <QGeoRectangle>
#include <vector>
#include "earthquake_data.h"


class EarthquakeDatabase {
public:
    explicit EarthquakeDatabase(const QString& dbPath = "earthquakes.db");
    ~EarthquakeDatabase();
    
    bool initialize();
    bool insertEarthquake(const EarthquakeData& data);
    bool insertEarthquakes(const std::vector<EarthquakeData>& earthquakes);
    
    std::vector<EarthquakeData> getEarthquakes(
        const QDateTime& startTime = QDateTime(),
        const QDateTime& endTime = QDateTime(),
        double minMagnitude = 0.0,
        double maxMagnitude = 10.0,
        const QGeoRectangle& region = QGeoRectangle()
    );
    
    std::vector<EarthquakeData> getEarthquakesInRadius(
        const QGeoCoordinate& center,
        double radiusKm,
        const QDateTime& since = QDateTime()
    );
    
    bool earthquakeExists(const QString& id);
    int getEarthquakeCount();
    void cleanOldRecords(int daysToKeep = 30);
    
private:
    QSqlDatabase db;
    QString connectionName;
    
    bool createTables();
    double calculateDistance(const QGeoCoordinate& coord1, const QGeoCoordinate& coord2);
};
