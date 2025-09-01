#include "spatial_utils.hpp"
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
