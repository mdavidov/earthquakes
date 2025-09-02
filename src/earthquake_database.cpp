
#include "earthquake_database.hpp"

#include <QSqlError>
#include <QDebug>
#include <QUuid>

EarthquakeDatabase::EarthquakeDatabase(const QString& dbPath) {
    connectionName = QUuid::createUuid().toString();
    db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    db.setDatabaseName(dbPath);
}

EarthquakeDatabase::~EarthquakeDatabase() {
    if (db.isOpen()) {
        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
}

bool EarthquakeDatabase::initialize() {
    if (!db.open()) {
        qDebug() << "Failed to open database:" << db.lastError().text();
        return false;
    }
    
    return createTables();
}

bool EarthquakeDatabase::createTables() {
    QSqlQuery query(db);
    
    QString createTableSQL = R"(
        CREATE TABLE IF NOT EXISTS earthquakes (
            eventId TEXT PRIMARY KEY,
            magnitude REAL NOT NULL,
            latitude REAL NOT NULL,
            longitude REAL NOT NULL,
            depth REAL,
            timestamp INTEGER NOT NULL,
            place TEXT,
            url TEXT,
            type TEXT,
            created_at INTEGER DEFAULT (strftime('%s', 'now'))
        )
    )";
    
    if (!query.exec(createTableSQL)) {
        qDebug() << "Failed to create table:" << query.lastError().text();
        return false;
    }
    
    // Create indexes for performance
    query.exec("CREATE INDEX IF NOT EXISTS idx_magnitude ON earthquakes(magnitude)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_timestamp ON earthquakes(timestamp)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_location ON earthquakes(latitude, longitude)");
    
    return true;
}

bool EarthquakeDatabase::insertEarthquake(const EarthquakeData& data) {
    if (earthquakeExists(data.eventId)) {
        return true; // Already exists, skip
    }
    
    QSqlQuery query(db);
    query.prepare(R"(
        INSERT INTO earthquakes 
        (eventId, magnitude, latitude, longitude, depth, timestamp, place, url, type)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");
    
    query.addBindValue(data.eventId);
    query.addBindValue(data.magnitude);
    query.addBindValue(data.location.latitude());
    query.addBindValue(data.location.longitude());
    query.addBindValue(data.depth);
    query.addBindValue(data.timestamp.toMSecsSinceEpoch());
    query.addBindValue(data.place);
    query.addBindValue(data.url);
    query.addBindValue(data.type);
    
    if (!query.exec()) {
        qDebug() << "Failed to insert earthquake:" << query.lastError().text();
        return false;
    }
    
    return true;
}

bool EarthquakeDatabase::insertEarthquakes(const std::vector<EarthquakeData>& earthquakes) {
    db.transaction();
    
    for (const auto& earthquake : earthquakes) {
        if (!insertEarthquake(earthquake)) {
            db.rollback();
            return false;
        }
    }
    
    return db.commit();
}

std::vector<EarthquakeData> EarthquakeDatabase::getEarthquakes(
    const QDateTime& startTime,
    const QDateTime& endTime,
    double minMagnitude,
    double maxMagnitude,
    const QGeoRectangle& region) {
    
    std::vector<EarthquakeData> results;
    QSqlQuery query(db);
    
    QString sql = "SELECT * FROM earthquakes WHERE magnitude >= ? AND magnitude <= ?";
    
    if (startTime.isValid()) {
        sql += " AND timestamp >= ?";
    }
    if (endTime.isValid()) {
        sql += " AND timestamp <= ?";
    }
    if (region.isValid()) {
        sql += " AND latitude >= ? AND latitude <= ? AND longitude >= ? AND longitude <= ?";
    }
    
    sql += " ORDER BY timestamp DESC";
    
    query.prepare(sql);
    query.addBindValue(minMagnitude);
    query.addBindValue(maxMagnitude);
    
    if (startTime.isValid()) {
        query.addBindValue(startTime.toMSecsSinceEpoch());
    }
    if (endTime.isValid()) {
        query.addBindValue(endTime.toMSecsSinceEpoch());
    }
    if (region.isValid()) {
        query.addBindValue(region.bottomLeft().latitude());
        query.addBindValue(region.topRight().latitude());
        query.addBindValue(region.bottomLeft().longitude());
        query.addBindValue(region.topRight().longitude());
    }
    
    if (!query.exec()) {
        qDebug() << "Query failed:" << query.lastError().text();
        return results;
    }
    
    while (query.next()) {
        EarthquakeData data;
        data.eventId = query.value("eventId").toString();
        data.magnitude = query.value("magnitude").toDouble();
        data.location = QGeoCoordinate(
            query.value("latitude").toDouble(),
            query.value("longitude").toDouble()
        );
        data.depth = query.value("depth").toDouble();
        data.timestamp = QDateTime::fromMSecsSinceEpoch(query.value("timestamp").toLongLong());
        data.place = query.value("place").toString();
        data.url = query.value("url").toString();
        data.type = query.value("type").toString();
        
        results.push_back(data);
    }
    
    return results;
}

bool EarthquakeDatabase::earthquakeExists(const QString& eventId) {
    QSqlQuery query(db);
    query.prepare("SELECT 1 FROM earthquakes WHERE eventId = ? LIMIT 1");
    query.addBindValue(eventId);
    
    return query.exec() && query.next();
}
