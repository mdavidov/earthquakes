// EarthquakeMapWidget.h
#ifndef EARTHQUAKEMAPWIDGET_H
#define EARTHQUAKEMAPWIDGET_H

#include <QtWidgets/QWidget>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>
#include <QtCore/QTimer>
#include <QtCore/QDateTime>
#include <QtGui/QMouseEvent>
#include <QtGui/QWheelEvent>
#include <QVector>

struct EarthquakeData {
    double latitude;
    double longitude;
    double magnitude;
    double depth;
    QDateTime timestamp;
    QString location;
    int alertLevel; // 0-4: Info, Minor, Moderate, Major, Critical
};

class EarthquakeMapWidget : public QWidget
{
    Q_OBJECT

public:
    explicit EarthquakeMapWidget(QWidget *parent = nullptr);
    ~EarthquakeMapWidget();

    void addEarthquake(const EarthquakeData &earthquake);
    void clearEarthquakes();
    void setMapCenter(double lat, double lon);
    void setZoomLevel(double zoom);
    void setShowGrid(bool show);
    void setShowLegend(bool show);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void updateAnimation();

private:
    void drawWorldMap(QPainter &painter);
    void drawEarthquakes(QPainter &painter);
    void drawGrid(QPainter &painter);
    void drawLegend(QPainter &painter);
    void drawAlertIndicator(QPainter &painter, const EarthquakeData &eq, const QPoint &pos);
    
    QPoint latLonToPixel(double lat, double lon) const;
    QPair<double, double> pixelToLatLon(const QPoint &pixel) const;
    QColor getMagnitudeColor(double magnitude) const;
    QColor getAlertLevelColor(int alertLevel) const;
    int getMarkerSize(double magnitude) const;

    QVector<EarthquakeData> m_earthquakes;
    QPixmap m_worldMapCache;
    
    double m_centerLat;
    double m_centerLon;
    double m_zoomLevel;
    bool m_showGrid;
    bool m_showLegend;
    
    QPoint m_lastPanPoint;
    bool m_isPanning;
    
    QTimer *m_animationTimer;
    int m_animationFrame;
    
    static const double MIN_ZOOM;
    static const double MAX_ZOOM;
};

#endif // EARTHQUAKEMAPWIDGET_H

// EarthquakeMapWidget.cpp
#include "EarthquakeMapWidget.hpp"
#include "SpatialUtils.hpp"
#include <QtWidgets/QApplication>
#include <QtGui/QPainterPath>
#include <QtCore/QDebug>
#include <cmath>

const double EarthquakeMapWidget::MIN_ZOOM = 0.1;
const double EarthquakeMapWidget::MAX_ZOOM = 20.0;

EarthquakeMapWidget::EarthquakeMapWidget(QWidget *parent)
    : QWidget(parent)
    , m_centerLat(0.0)
    , m_centerLon(0.0)
    , m_zoomLevel(1.0)
    , m_showGrid(true)
    , m_showLegend(true)
    , m_isPanning(false)
    , m_animationFrame(0)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMouseTracking(true);
    setMinimumSize(400, 300);
    
    m_animationTimer = new QTimer(this);
    connect(m_animationTimer, &QTimer::timeout, this, &EarthquakeMapWidget::updateAnimation);
    m_animationTimer->start(100); // 10 FPS for smooth animation
}

EarthquakeMapWidget::~EarthquakeMapWidget()
{
}

void EarthquakeMapWidget::addEarthquake(const EarthquakeData &earthquake)
{
    m_earthquakes.append(earthquake);
    update();
}

void EarthquakeMapWidget::clearEarthquakes()
{
    m_earthquakes.clear();
    update();
}

void EarthquakeMapWidget::setMapCenter(double lat, double lon)
{
    m_centerLat = qBound(-90.0, lat, 90.0);
    m_centerLon = SpatialUtils::normalizeLongitude(lon);
    update();
}

void EarthquakeMapWidget::setZoomLevel(double zoom)
{
    m_zoomLevel = qBound(MIN_ZOOM, zoom, MAX_ZOOM);
    update();
}

void EarthquakeMapWidget::setShowGrid(bool show)
{
    m_showGrid = show;
    update();
}

void EarthquakeMapWidget::setShowLegend(bool show)
{
    m_showLegend = show;
    update();
}

void EarthquakeMapWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Clear background
    painter.fillRect(rect(), QColor(20, 30, 50));
    
    drawWorldMap(painter);
    if (m_showGrid) {
        drawGrid(painter);
    }
    drawEarthquakes(painter);
    if (m_showLegend) {
        drawLegend(painter);
    }
}

void EarthquakeMapWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_lastPanPoint = event->pos();
        m_isPanning = true;
        setCursor(Qt::ClosedHandCursor);
    }
}

void EarthquakeMapWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isPanning && (event->buttons() & Qt::LeftButton)) {
        QPoint delta = event->pos() - m_lastPanPoint;
        
        // Convert pixel movement to lat/lon movement
        double latDelta = -delta.y() * (180.0 / height()) / m_zoomLevel;
        double lonDelta = -delta.x() * (360.0 / width()) / m_zoomLevel;
        
        setMapCenter(m_centerLat + latDelta, m_centerLon + lonDelta);
        m_lastPanPoint = event->pos();
    } else {
        setCursor(Qt::ArrowCursor);
    }
}

void EarthquakeMapWidget::wheelEvent(QWheelEvent *event)
{
    double zoomFactor = event->angleDelta().y() > 0 ? 1.2 : 0.8;
    setZoomLevel(m_zoomLevel * zoomFactor);
}

void EarthquakeMapWidget::resizeEvent(QResizeEvent *event)
{
    m_worldMapCache = QPixmap(); // Clear cache to force redraw
    QWidget::resizeEvent(event);
}

void EarthquakeMapWidget::updateAnimation()
{
    m_animationFrame = (m_animationFrame + 1) % 60; // 6 second cycle at 10 FPS
    update();
}

void EarthquakeMapWidget::drawWorldMap(QPainter &painter)
{
    painter.setPen(QPen(QColor(100, 120, 140), 1));
    painter.setBrush(QColor(40, 60, 80));
    
    // Draw simplified world continents
    // This is a basic representation - in a real app, you'd load actual map data
    QVector<QPolygonF> continents;
    
    // North America (simplified)
    QPolygonF northAmerica;
    northAmerica << latLonToPixel(70, -160) << latLonToPixel(70, -60)
                 << latLonToPixel(25, -80) << latLonToPixel(10, -120)
                 << latLonToPixel(70, -160);
    continents.append(northAmerica);
    
    // Europe/Asia (simplified)
    QPolygonF eurasia;
    eurasia << latLonToPixel(75, -10) << latLonToPixel(75, 180)
            << latLonToPixel(10, 140) << latLonToPixel(35, 30)
            << latLonToPixel(75, -10);
    continents.append(eurasia);
    
    // Africa (simplified)
    QPolygonF africa;
    africa << latLonToPixel(35, -20) << latLonToPixel(35, 50)
           << latLonToPixel(-35, 40) << latLonToPixel(-35, 10)
           << latLonToPixel(35, -20);
    continents.append(africa);
    
    for (const auto &continent : continents) {
        painter.drawPolygon(continent);
    }
}

void EarthquakeMapWidget::drawEarthquakes(QPainter &painter)
{
    for (const auto &eq : m_earthquakes) {
        QPoint pos = latLonToPixel(eq.latitude, eq.longitude);
        
        // Skip if outside visible area
        if (!rect().contains(pos)) continue;
        
        drawAlertIndicator(painter, eq, pos);
    }
}

void EarthquakeMapWidget::drawGrid(QPainter &painter)
{
    painter.setPen(QPen(QColor(60, 80, 100), 1, Qt::DotLine));
    
    // Draw latitude lines
    for (int lat = -90; lat <= 90; lat += 15) {
        QPoint start = latLonToPixel(lat, -180);
        QPoint end = latLonToPixel(lat, 180);
        painter.drawLine(start, end);
    }
    
    // Draw longitude lines
    for (int lon = -180; lon <= 180; lon += 30) {
        QPoint start = latLonToPixel(-90, lon);
        QPoint end = latLonToPixel(90, lon);
        painter.drawLine(start, end);
    }
}

void EarthquakeMapWidget::drawLegend(QPainter &painter)
{
    QRect legendRect(width() - 200, 20, 180, 200);
    painter.fillRect(legendRect, QColor(0, 0, 0, 180));
    painter.setPen(QColor(255, 255, 255));
    painter.drawRect(legendRect);
    
    painter.setFont(QFont("Arial", 10, QFont::Bold));
    painter.drawText(legendRect.x() + 10, legendRect.y() + 20, "Earthquake Legend");
    
    // Magnitude scale
    painter.setFont(QFont("Arial", 8));
    int y = legendRect.y() + 45;
    
    for (double mag = 2.0; mag <= 8.0; mag += 1.0) {
        QColor color = getMagnitudeColor(mag);
        int size = getMarkerSize(mag);
        
        painter.setBrush(color);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(legendRect.x() + 15, y - size/2, size, size);
        
        painter.setPen(QColor(255, 255, 255));
        painter.drawText(legendRect.x() + 35, y + 4, QString("M%1").arg(mag, 0, 'f', 1));
        y += 20;
    }
    
    // Alert levels
    y += 10;
    painter.setFont(QFont("Arial", 8, QFont::Bold));
    painter.drawText(legendRect.x() + 10, y, "Alert Levels");
    y += 15;
    
    QStringList alertLabels = {"Info", "Minor", "Moderate", "Major", "Critical"};
    for (int i = 0; i < alertLabels.size(); ++i) {
        QColor color = getAlertLevelColor(i);
        painter.fillRect(legendRect.x() + 15, y - 6, 12, 12, color);
        painter.setPen(QColor(255, 255, 255));
        painter.drawText(legendRect.x() + 35, y + 4, alertLabels[i]);
        y += 15;
    }
}

void EarthquakeMapWidget::drawAlertIndicator(QPainter &painter, const EarthquakeData &eq, const QPoint &pos)
{
    int size = getMarkerSize(eq.magnitude);
    QColor baseColor = getMagnitudeColor(eq.magnitude);
    QColor alertColor = getAlertLevelColor(eq.alertLevel);
    
    // Pulsing animation for recent earthquakes
    bool isRecent = eq.timestamp.secsTo(QDateTime::currentDateTime()) < 3600; // Last hour
    double pulse = 1.0;
    if (isRecent) {
        pulse = 1.0 + 0.3 * sin(m_animationFrame * 0.3);
    }
    
    int animatedSize = static_cast<int>(size * pulse);
    
    // Draw outer ring (alert level)
    painter.setPen(QPen(alertColor, 3));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(pos.x() - animatedSize/2 - 3, pos.y() - animatedSize/2 - 3, 
                       animatedSize + 6, animatedSize + 6);
    
    // Draw main earthquake marker
    painter.setPen(QPen(baseColor.darker(150), 2));
    painter.setBrush(baseColor);
    painter.drawEllipse(pos.x() - animatedSize/2, pos.y() - animatedSize/2, 
                       animatedSize, animatedSize);
    
    // Draw magnitude text for larger earthquakes
    if (eq.magnitude >= 5.0) {
        painter.setPen(QColor(255, 255, 255));
        painter.setFont(QFont("Arial", 8, QFont::Bold));
        QString magText = QString::number(eq.magnitude, 'f', 1);
        QRect textRect = painter.fontMetrics().boundingRect(magText);
        painter.drawText(pos.x() - textRect.width()/2, pos.y() + textRect.height()/2, magText);
    }
}

QPoint EarthquakeMapWidget::latLonToPixel(double lat, double lon) const
{
    // Mercator projection
    double x = (lon - m_centerLon) * m_zoomLevel * width() / 360.0 + width() / 2.0;
    double latRad = lat * M_PI / 180.0;
    double centerLatRad = m_centerLat * M_PI / 180.0;
    double y = (centerLatRad - latRad) * m_zoomLevel * height() / M_PI + height() / 2.0;
    
    return QPoint(static_cast<int>(x), static_cast<int>(y));
}

QPair<double, double> EarthquakeMapWidget::pixelToLatLon(const QPoint &pixel) const
{
    double lon = (pixel.x() - width() / 2.0) * 360.0 / (m_zoomLevel * width()) + m_centerLon;
    double centerLatRad = m_centerLat * M_PI / 180.0;
    double latRad = centerLatRad - (pixel.y() - height() / 2.0) * M_PI / (m_zoomLevel * height());
    double lat = latRad * 180.0 / M_PI;
    
    return QPair<double, double>(lat, lon);
}

QColor EarthquakeMapWidget::getMagnitudeColor(double magnitude) const
{
    if (magnitude < 3.0) return QColor(100, 255, 100);      // Light green
    else if (magnitude < 4.0) return QColor(255, 255, 100); // Yellow
    else if (magnitude < 5.0) return QColor(255, 180, 100); // Orange
    else if (magnitude < 6.0) return QColor(255, 100, 100); // Light red
    else if (magnitude < 7.0) return QColor(200, 50, 50);   // Red
    else return QColor(150, 0, 150);                        // Purple
}

QColor EarthquakeMapWidget::getAlertLevelColor(int alertLevel) const
{
    switch (alertLevel) {
        case 0: return QColor(100, 150, 255); // Blue - Info
        case 1: return QColor(100, 255, 100); // Green - Minor
        case 2: return QColor(255, 255, 100); // Yellow - Moderate
        case 3: return QColor(255, 150, 50);  // Orange - Major
        case 4: return QColor(255, 50, 50);   // Red - Critical
        default: return QColor(128, 128, 128); // Gray - Unknown
    }
}

int EarthquakeMapWidget::getMarkerSize(double magnitude) const
{
    return static_cast<int>(qBound(4.0, magnitude * 3.0, 30.0));
}

void EarthquakeMapWidget::updateAnimation()
{
    update();
}

// SpatialUtils.h
#ifndef SPATIALUTILS_H
#define SPATIALUTILS_H

#include <QtCore/QPointF>
#include <QtCore/QVector>
#include <cmath>

class SpatialUtils
{
public:
    // Distance calculations
    static double haversineDistance(double lat1, double lon1, double lat2, double lon2);
    static double euclideanDistance(const QPointF &p1, const QPointF &p2);
    
    // Coordinate transformations
    static QPointF mercatorProjection(double lat, double lon);
    static QPointF inverseMercatorProjection(const QPointF &point);
    static double normalizeLongitude(double lon);
    static double normalizeLatitude(double lat);
    
    // Geometric utilities
    static double calculateBearing(double lat1, double lon1, double lat2, double lon2);
    static QPointF calculateDestination(double lat, double lon, double bearing, double distance);
    static bool isPointInPolygon(const QPointF &point, const QVector<QPointF> &polygon);
    static QPointF polygonCentroid(const QVector<QPointF> &polygon);
    
    // Earthquake-specific utilities
    static double estimateShakeIntensity(double magnitude, double distance);
    static double calculateSeismicEnergy(double magnitude);
    static int mercalliIntensity(double magnitude, double distance);
    static double estimateArrivalTime(double distance, bool isPWave = true);
    
    // Clustering and analysis
    static QVector<QVector<int>> spatialClustering(const QVector<QPointF> &points, double maxDistance);
    static QPointF calculateClusterCenter(const QVector<QPointF> &points, const QVector<int> &indices);
    static double calculateClusterRadius(const QVector<QPointF> &points, const QVector<int> &indices, const QPointF &center);
    
    // Constants
    static const double EARTH_RADIUS_KM;
    static const double P_WAVE_SPEED_KM_S;
    static const double S_WAVE_SPEED_KM_S;
};

#endif // SPATIALUTILS_H

// SpatialUtils.cpp
#include "SpatialUtils.h"
#include <QtCore/QDebug>
#include <algorithm>

const double SpatialUtils::EARTH_RADIUS_KM = 6371.0;
const double SpatialUtils::P_WAVE_SPEED_KM_S = 6.0;
const double SpatialUtils::S_WAVE_SPEED_KM_S = 3.5;

double SpatialUtils::haversineDistance(double lat1, double lon1, double lat2, double lon2)
{
    double lat1Rad = lat1 * M_PI / 180.0;
    double lon1Rad = lon1 * M_PI / 180.0;
    double lat2Rad = lat2 * M_PI / 180.0;
    double lon2Rad = lon2 * M_PI / 180.0;
    
    double dLat = lat2Rad - lat1Rad;
    double dLon = lon2Rad - lon1Rad;
    
    double a = sin(dLat/2) * sin(dLat/2) + 
               cos(lat1Rad) * cos(lat2Rad) * sin(dLon/2) * sin(dLon/2);
    double c = 2 * atan2(sqrt(a), sqrt(1-a));
    
    return EARTH_RADIUS_KM * c;
}

double SpatialUtils::euclideanDistance(const QPointF &p1, const QPointF &p2)
{
    double dx = p2.x() - p1.x();
    double dy = p2.y() - p1.y();
    return sqrt(dx * dx + dy * dy);
}

QPointF SpatialUtils::mercatorProjection(double lat, double lon)
{
    double x = lon * M_PI / 180.0;
    double latRad = lat * M_PI / 180.0;
    double y = log(tan(M_PI/4 + latRad/2));
    
    return QPointF(x, y);
}

QPointF SpatialUtils::inverseMercatorProjection(const QPointF &point)
{
    double lon = point.x() * 180.0 / M_PI;
    double lat = (2 * atan(exp(point.y())) - M_PI/2) * 180.0 / M_PI;
    
    return QPointF(lat, lon);
}

double SpatialUtils::normalizeLongitude(double lon)
{
    while (lon > 180.0) lon -= 360.0;
    while (lon < -180.0) lon += 360.0;
    return lon;
}

double SpatialUtils::normalizeLatitude(double lat)
{
    return qBound(-90.0, lat, 90.0);
}

double SpatialUtils::calculateBearing(double lat1, double lon1, double lat2, double lon2)
{
    double lat1Rad = lat1 * M_PI / 180.0;
    double lat2Rad = lat2 * M_PI / 180.0;
    double dLon = (lon2 - lon1) * M_PI / 180.0;
    
    double y = sin(dLon) * cos(lat2Rad);
    double x = cos(lat1Rad) * sin(lat2Rad) - sin(lat1Rad) * cos(lat2Rad) * cos(dLon);
    
    double bearing = atan2(y, x) * 180.0 / M_PI;
    return fmod(bearing + 360.0, 360.0);
}

QPointF SpatialUtils::calculateDestination(double lat, double lon, double bearing, double distance)
{
    double latRad = lat * M_PI / 180.0;
    double lonRad = lon * M_PI / 180.0;
    double bearingRad = bearing * M_PI / 180.0;
    double d = distance / EARTH_RADIUS_KM;
    
    double lat2 = asin(sin(latRad) * cos(d) + cos(latRad) * sin(d) * cos(bearingRad));
    double lon2 = lonRad + atan2(sin(bearingRad) * sin(d) * cos(latRad),
                                cos(d) - sin(latRad) * sin(lat2));
    
    return QPointF(lat2 * 180.0 / M_PI, normalizeLongitude(lon2 * 180.0 / M_PI));
}

bool SpatialUtils::isPointInPolygon(const QPointF &point, const QVector<QPointF> &polygon)
{
    bool inside = false;
    int n = polygon.size();
    
    for (int i = 0, j = n - 1; i < n; j = i++) {
        if (((polygon[i].y() > point.y()) != (polygon[j].y() > point.y())) &&
            (point.x() < (polygon[j].x() - polygon[i].x()) * (point.y() - polygon[i].y()) / 
             (polygon[j].y() - polygon[i].y()) + polygon[i].x())) {
            inside = !inside;
        }
    }
    
    return inside;
}

QPointF SpatialUtils::polygonCentroid(const QVector<QPointF> &polygon)
{
    if (polygon.isEmpty()) return QPointF();
    
    double cx = 0.0, cy = 0.0;
    double area = 0.0;
    int n = polygon.size();
    
    for (int i = 0; i < n; ++i) {
        int j = (i + 1) % n;
        double cross = polygon[i].x() * polygon[j].y() - polygon[j].x() * polygon[i].y();
        area += cross;
        cx += (polygon[i].x() + polygon[j].x()) * cross;
        cy += (polygon[i].y() + polygon[j].y()) * cross;
    }
    
    area *= 0.5;
    if (qAbs(area) < 1e-10) {
        // Fallback to arithmetic mean for degenerate polygons
        for (const auto &point : polygon) {
            cx += point.x();
            cy += point.y();
        }
        return QPointF(cx / n, cy / n);
    }
    
    cx /= (6.0 * area);
    cy /= (6.0 * area);
    
    return QPointF(cx, cy);
}

double SpatialUtils::estimateShakeIntensity(double magnitude, double distance)
{
    // Simplified intensity estimation based on magnitude and distance
    if (distance <= 0.0) return magnitude;
    
    double intensity = magnitude - 3.0 * log10(distance) + 2.0;
    return qMax(0.0, intensity);
}

double SpatialUtils::calculateSeismicEnergy(double magnitude)
{
    // Energy in Joules using Gutenberg-Richter relation
    return pow(10, 1.5 * magnitude + 4.8);
}

int SpatialUtils::mercalliIntensity(double magnitude, double distance)
{
    double intensity = estimateShakeIntensity(magnitude, distance);
    
    if (intensity < 2.0) return 1;      // I - Not felt
    else if (intensity < 3.0) return 2; // II - Weak
    else if (intensity < 4.0) return 3; // III - Weak
    else if (intensity < 5.0) return 4; // IV - Light
    else if (intensity < 6.0) return 5; // V - Moderate
    else if (intensity < 7.0) return 6; // VI - Strong
    else if (intensity < 8.0) return 7; // VII - Very strong
    else if (intensity < 9.0) return 8; // VIII - Severe
    else if (intensity < 10.0) return 9; // IX - Violent
    else if (intensity < 11.0) return 10; // X - Extreme
    else if (intensity < 12.0) return 11; // XI - Extreme
    else return 12;                     // XII - Extreme
}

double SpatialUtils::estimateArrivalTime(double distance, bool isPWave)
{
    double speed = isPWave ? P_WAVE_SPEED_KM_S : S_WAVE_SPEED_KM_S;
    return distance / speed; // Time in seconds
}

QVector<QVector<int>> SpatialUtils::spatialClustering(const QVector<QPointF> &points, double maxDistance)
{
    QVector<QVector<int>> clusters;
    QVector<bool> visited(points.size(), false);
    
    for (int i = 0; i < points.size(); ++i) {
        if (visited[i]) continue;
        
        QVector<int> cluster;
        QVector<int> toCheck;
        toCheck.append(i);
        
        while (!toCheck.isEmpty()) {
            int current = toCheck.takeLast();
            if (visited[current]) continue;
            
            visited[current] = true;
            cluster.append(current);
            
            // Find neighbors
            for (int j = 0; j < points.size(); ++j) {
                if (!visited[j] && euclideanDistance(points[current], points[j]) <= maxDistance) {
                    toCheck.append(j);
                }
            }
        }
        
        if (!cluster.isEmpty()) {
            clusters.append(cluster);
        }
    }
    
    return clusters;
}

QPointF SpatialUtils::calculateClusterCenter(const QVector<QPointF> &points, const QVector<int> &indices)
{
    if (indices.isEmpty()) return QPointF();
    
    double sumX = 0.0, sumY = 0.0;
    for (int idx : indices) {
        if (idx >= 0 && idx < points.size()) {
            sumX += points[idx].x();
            sumY += points[idx].y();
        }
    }
    
    return QPointF(sumX / indices.size(), sumY / indices.size());
}

double SpatialUtils::calculateClusterRadius(const QVector<QPointF> &points, const QVector<int> &indices, const QPointF &center)
{
    if (indices.isEmpty()) return 0.0;
    
    double maxDistance = 0.0;
    for (int idx : indices) {
        if (idx >= 0 && idx < points.size()) {
            double dist = euclideanDistance(points[idx], center);
            maxDistance = qMax(maxDistance, dist);
        }
    }
    
    return maxDistance;
}