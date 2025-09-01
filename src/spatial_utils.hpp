#pragma once
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
