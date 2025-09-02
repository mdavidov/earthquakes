
@include "earthquake_map_widget.hpp"

// Additional method implementations for EarthquakeMapWidget
// Add these methods to earthquake_map_widget.cpp

// =============================================================================
// PUBLIC INTERFACE METHODS (missing implementations)
// =============================================================================

void EarthquakeMapWidget::addEarthquakes(const QVector<EarthquakeData> &earthquakes)
{
    QMutexLocker locker(&m_dataMutex);
    
    for (const auto &earthquake : earthquakes) {
        // Check if earthquake already exists
        bool found = false;
        for (int i = 0; i < m_earthquakes.size(); ++i) {
            if (m_earthquakes[i].data.eventId == earthquake.eventId) {
                // Update existing earthquake
                m_earthquakes[i].data = earthquake;
                m_earthquakes[i].lastUpdate = QDateTime::currentDateTime();
                found = true;
                break;
            }
        }
        
        if (!found) {
            // Add new earthquake
            VisualEarthquake visualEq;
            visualEq.data = earthquake;
            visualEq.screenPos = latLonToScreen(earthquake.latitude, earthquake.longitude);
            visualEq.displaySize = getEarthquakeSize(earthquake);
            visualEq.displayColor = getEarthquakeColor(earthquake);
            visualEq.opacity = 1.0;
            visualEq.animationPhase = 0.0;
            visualEq.isVisible = isEarthquakeVisible(earthquake);
            visualEq.isHighlighted = false;
            visualEq.isSelected = false;
            visualEq.lastUpdate = QDateTime::currentDateTime();
            visualEq.clusterId = -1;
            visualEq.isClusterCenter = false;
            
            m_earthquakes.append(visualEq);
        }
    }
    
    updateVisibleEarthquakes();
    if (m_settings.enableClustering) {
        updateClusters();
    }
    
    update();
}

void EarthquakeMapWidget::removeEarthquake(const QString &eventId)
{
    QMutexLocker locker(&m_dataMutex);
    
    for (int i = 0; i < m_earthquakes.size(); ++i) {
        if (m_earthquakes[i].data.eventId == eventId) {
            m_earthquakes.removeAt(i);
            break;
        }
    }
    
    // Remove from selection if present
    m_selectedIds.removeAll(eventId);
    
    update();
}

void EarthquakeMapWidget::clearEarthquakes()
{
    QMutexLocker locker(&m_dataMutex);
    
    m_earthquakes.clear();
    m_selectedIds.clear();
    m_hoveredEarthquakeId.clear();
    clearClusters();
    
    update();
}

void EarthquakeMapWidget::updateEarthquake(const EarthquakeData &earthquake)
{
    QMutexLocker locker(&m_dataMutex);
    
    for (auto &eq : m_earthquakes) {
        if (eq.data.eventId == earthquake.eventId) {
            eq.data = earthquake;
            eq.displaySize = getEarthquakeSize(earthquake);
            eq.displayColor = getEarthquakeColor(earthquake);
            eq.isVisible = isEarthquakeVisible(earthquake);
            eq.lastUpdate = QDateTime::currentDateTime();
            break;
        }
    }
    
    update();
}

void EarthquakeMapWidget::zoomIn()
{
    double newZoom = qBound(MIN_ZOOM, m_zoomLevel * ZOOM_FACTOR, MAX_ZOOM);
    setZoomLevel(newZoom);
}

void EarthquakeMapWidget::zoomOut()
{
    double newZoom = qBound(MIN_ZOOM, m_zoomLevel / ZOOM_FACTOR, MAX_ZOOM);
    setZoomLevel(newZoom);
}

void EarthquakeMapWidget::fitToEarthquakes()
{
    QVector<EarthquakeData> visibleData = getVisibleEarthquakes();
    if (visibleData.isEmpty()) {
        return;
    }
    
    MapBounds bounds = calculateBounds(visibleData);
    fitToBounds(bounds);
}

void EarthquakeMapWidget::fitToBounds(const MapBounds &bounds)
{
    if (!bounds.isValid()) {
        return;
    }
    
    // Calculate center
    QPointF center = bounds.center();
    
    // Calculate optimal zoom
    double zoom = calculateOptimalZoom(bounds);
    
    // Animate to new position
    animateToLocation(center.y(), center.x(), zoom);
}

void EarthquakeMapWidget::setLayerEnabled(MapLayer layer, bool enabled)
{
    if (enabled) {
        if (!m_settings.enabledLayers.contains(layer)) {
            m_settings.enabledLayers.append(layer);
        }
    } else {
        m_settings.enabledLayers.removeAll(layer);
    }
    
    m_backgroundCacheValid = false;
    m_layerCacheValid = false;
    update();
}

bool EarthquakeMapWidget::isLayerEnabled(MapLayer layer) const
{
    return m_settings.enabledLayers.contains(layer);
}

void EarthquakeMapWidget::setBackgroundMap(const QPixmap &map)
{
    m_backgroundMap = map;
    m_backgroundCacheValid = false;
    update();
}

void EarthquakeMapWidget::loadBackgroundMapFromUrl(const QString &url)
{
    if (!m_networkManager) {
        return;
    }
    
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "EarthquakeMapWidget/1.0");
    
    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QPixmap mapImage;
            if (mapImage.loadFromData(data)) {
                setBackgroundMap(mapImage);
                emit backgroundMapLoaded();
            }
        }
        reply->deleteLater();
    });
}

EarthquakeData EarthquakeMapWidget::getEarthquakeAt(const QPoint &point) const
{
    int index = findEarthquakeAt(point);
    if (index >= 0) {
        QMutexLocker locker(&m_dataMutex);
        return m_earthquakes[index].data;
    }
    return EarthquakeData(); // Return empty data
}

QVector<EarthquakeData> EarthquakeMapWidget::getEarthquakesInRegion(const MapBounds &bounds) const
{
    QMutexLocker locker(&m_dataMutex);
    QVector<EarthquakeData> result;
    
    for (const auto &eq : m_earthquakes) {
        if (bounds.contains(eq.data.latitude, eq.data.longitude)) {
            result.append(eq.data);
        }
    }
    
    return result;
}

void EarthquakeMapWidget::enableClustering(bool enabled)
{
    m_settings.enableClustering = enabled;
    
    if (enabled) {
        updateClusters();
    } else {
        clearClusters();
    }
    
    update();
}

void EarthquakeMapWidget::setClusterDistance(double pixels)
{
    m_settings.clusterDistance = qMax(10.0, pixels);
    
    if (m_settings.enableClustering) {
        updateClusters();
        update();
    }
}

void EarthquakeMapWidget::setLocationFilter(const MapBounds &bounds)
{
    m_locationFilter = bounds;
    m_hasLocationFilter = bounds.isValid();
    
    updateVisibleEarthquakes();
    update();
}

void EarthquakeMapWidget::flashEarthquake(const QString &eventId, int times)
{
    // Simple flash implementation using timer
    int flashCount = 0;
    int maxFlashes = times * 2; // Each flash is on/off
    
    QTimer *flashTimer = new QTimer(this);
    flashTimer->setInterval(300); // 300ms intervals
    
    connect(flashTimer, &QTimer::timeout, [this, eventId, flashTimer, flashCount, maxFlashes]() mutable {
        bool highlight = (flashCount % 2 == 0);
        
        QMutexLocker locker(&m_dataMutex);
        for (auto &eq : m_earthquakes) {
            if (eq.data.eventId == eventId) {
                eq.isHighlighted = highlight;
                break;
            }
        }
        
        update();
        flashCount++;
        
        if (flashCount >= maxFlashes) {
            flashTimer->stop();
            flashTimer->deleteLater();
        }
    });
    
    flashTimer->start();
}

void EarthquakeMapWidget::animateToLocation(double latitude, double longitude, double zoom, int durationMs)
{
    if (m_moveAnimationGroup && m_moveAnimationGroup->state() == QAbstractAnimation::Running) {
        m_moveAnimationGroup->stop();
    }
    
    // Set target values
    double targetZoom = (zoom > 0) ? qBound(MIN_ZOOM, zoom, MAX_ZOOM) : m_zoomLevel;
    
    // Configure latitude animation
    if (m_centerAnimation) {
        m_centerAnimation->setStartValue(m_centerLatitude);
        m_centerAnimation->setEndValue(latitude);
        m_centerAnimation->setDuration(durationMs);
    }
    
    // Configure longitude animation (handle wraparound)
    if (m_zoomAnimation) {
        m_zoomAnimation->setStartValue(m_zoomLevel);
        m_zoomAnimation->setEndValue(targetZoom);
        m_zoomAnimation->setDuration(durationMs);
    }
    
    // Start parallel animation
    if (m_moveAnimationGroup) {
        // Also animate longitude separately
        QPropertyAnimation *lonAnimation = new QPropertyAnimation(this, "centerLongitude");
        lonAnimation->setStartValue(m_centerLongitude);
        lonAnimation->setEndValue(longitude);
        lonAnimation->setDuration(durationMs);
        lonAnimation->setEasingCurve(QEasingCurve::InOutQuad);
        
        // Remove old animations and add new ones
        m_moveAnimationGroup->clear();
        m_moveAnimationGroup->addAnimation(m_centerAnimation);
        m_moveAnimationGroup->addAnimation(m_zoomAnimation);
        m_moveAnimationGroup->addAnimation(lonAnimation);
        
        m_moveAnimationGroup->start();
    }
}

QDateTime EarthquakeMapWidget::getLastUpdateTime() const
{
    QDateTime latest;
    QMutexLocker locker(&m_dataMutex);
    
    for (const auto &eq : m_earthquakes) {
        if (!latest.isValid() || eq.lastUpdate > latest) {
            latest = eq.lastUpdate;
        }
    }
    
    return latest;
}

// =============================================================================
// EXPORT FUNCTIONALITY
// =============================================================================

QByteArray EarthquakeMapWidget::exportToSvg(const QSize &size) const
{
    QSize renderSize = size.isValid() ? size : this->size();
    
    QByteArray svgData;
    QBuffer buffer(&svgData);
    buffer.open(QIODevice::WriteOnly);
    
    QSvgGenerator generator;
    generator.setOutputDevice(&buffer);
    generator.setSize(renderSize);
    generator.setViewBox(QRect(0, 0, renderSize.width(), renderSize.height()));
    generator.setTitle("Earthquake Map");
    generator.setDescription("Generated by Earthquake Alert System");
    
    QPainter painter(&generator);
    if (m_highQualityRendering) {
        painter.setRenderHint(QPainter::Antialiasing);
    }
    
    // Scale painter if different size
    if (renderSize != this->size()) {
        double scaleX = double(renderSize.width()) / width();
        double scaleY = double(renderSize.height()) / height();
        painter.scale(scaleX, scaleY);
    }
    
    // Render all components
    const_cast<EarthquakeMapWidget*>(this)->renderBackground(painter);
    const_cast<EarthquakeMapWidget*>(this)->renderMapLayers(painter);
    const_cast<EarthquakeMapWidget*>(this)->renderEarthquakes(painter);
    if (m_settings.enableClustering) {
        const_cast<EarthquakeMapWidget*>(this)->renderClusters(painter);
    }
    if (m_settings.showLegend) {
        const_cast<EarthquakeMapWidget*>(this)->renderLegend(painter);
    }
    
    return svgData;
}

// =============================================================================
// PROPERTY SETTERS/GETTERS FOR ANIMATION
// =============================================================================

void EarthquakeMapWidget::setCenter(double latitude, double longitude)
{
    bool changed = false;
    
    double newLat = qBound(-90.0, latitude, 90.0);
    double newLon = SpatialUtils::normalizeLongitude(longitude);
    
    if (qAbs(m_centerLatitude - newLat) > 1e-6) {
        m_centerLatitude = newLat;
        changed = true;
    }
    
    if (qAbs(m_centerLongitude - newLon) > 1e-6) {
        m_centerLongitude = newLon;
        changed = true;
    }
    
    if (changed) {
        updateVisibleBounds();
        updateVisibleEarthquakes();
        m_backgroundCacheValid = false;
        
        emit centerChanged(m_centerLatitude, m_centerLongitude);
        emit boundsChanged(m_visibleBounds);
        
        update();
    }
}

void EarthquakeMapWidget::setCenterLatitude(double latitude)
{
    setCenter(latitude, m_centerLongitude);
}

void EarthquakeMapWidget::setCenterLongitude(double longitude)
{
    setCenter(m_centerLatitude, longitude);
}

void EarthquakeMapWidget::setZoomLevel(double zoom)
{
    double newZoom = qBound(MIN_ZOOM, zoom, MAX_ZOOM);
    
    if (qAbs(m_zoomLevel - newZoom) > 1e-6) {
        m_zoomLevel = newZoom;
        
        updateVisibleBounds();
        updateVisibleEarthquakes();
        m_backgroundCacheValid = false;
        
        emit zoomChanged(m_zoomLevel);
        emit boundsChanged(m_visibleBounds);
        
        update();
    }
}

// =============================================================================
// UTILITY METHODS
// =============================================================================

QPointF EarthquakeMapWidget::unprojectCoordinate(const QPointF &projected) const
{
    // Inverse of projectCoordinate - implementation depends on projection
    switch (m_settings.projection) {
        case MapProjection::Mercator:
            return QPointF(projected.x(), (2 * atan(exp(projected.y() * M_PI / 180.0)) - M_PI/2) * 180.0 / M_PI);
        case MapProjection::Equirectangular:
            return projected;
        default:
            return projected; // Simplified
    }
}

void EarthquakeMapWidget::expandCluster(int clusterId)
{
    if (clusterId < 0 || clusterId >= m_clusters.size()) {
        return;
    }
    
    EarthquakeCluster &cluster = m_clusters[clusterId];
    if (cluster.isExpanded) {
        return;
    }
    
    // Mark as expanded
    cluster.isExpanded = true;
    
    // Animate individual earthquakes outward from cluster center
    QMutexLocker locker(&m_dataMutex);
    for (int eqId : cluster.earthquakeIds) {
        if (eqId >= 0 && eqId < m_earthquakes.size()) {
            m_earthquakes[eqId].clusterId = -1; // Remove from cluster
        }
    }
    
    // Trigger update
    QTimer::singleShot(CLUSTER_EXPAND_DURATION_MS, [this]() {
        updateClusters();
        update();
    });
    
    update();
}

void EarthquakeMapWidget::collapseCluster(int clusterId)
{
    if (clusterId < 0 || clusterId >= m_clusters.size()) {
        return;
    }
    
    EarthquakeCluster &cluster = m_clusters[clusterId];
    cluster.isExpanded = false;
    
    // Re-assign earthquakes to cluster
    QMutexLocker locker(&m_dataMutex);
    for (int eqId : cluster.earthquakeIds) {
        if (eqId >= 0 && eqId < m_earthquakes.size()) {
            m_earthquakes[eqId].clusterId = clusterId;
        }
    }
    
    update();
}

void EarthquakeMapWidget::cullOffscreenEarthquakes()
{
    QMutexLocker locker(&m_dataMutex);
    
    QRect extendedViewport = rect().adjusted(-200, -200, 200, 200);
    
    for (auto &eq : m_earthquakes) {
        bool wasVisible = eq.isVisible;
        bool inViewport = extendedViewport.contains(eq.screenPos.toPoint());
        
        eq.isVisible = inViewport && passesFilters(eq.data);
        
        if (wasVisible != eq.isVisible) {
            // Visibility changed - might need to update clustering
        }
    }
}

void EarthquakeMapWidget::updateLevelOfDetail()
{
    // Adjust rendering detail based on zoom level and earthquake count
    int visibleCount = getVisibleEarthquakes().size();
    
    if (m_zoomLevel < 0.5 || visibleCount > 1000) {
        // Low detail mode
        m_maxRenderingEarthquakes = 500;
        m_lodThreshold = 1.0;
    } else if (m_zoomLevel < 1.0 || visibleCount > 500) {
        // Medium detail mode
        m_maxRenderingEarthquakes = 1000;
        m_lodThreshold = 0.5;
    } else {
        // High detail mode
        m_maxRenderingEarthquakes = 10000;
        m_lodThreshold = 0.1;
    }
}

void EarthquakeMapWidget::optimizeForPerformance()
{
    updateLevelOfDetail();
    cullOffscreenEarthquakes();
    
    // Clean up old cache if memory usage is high
    if (m_backgroundCache.size().width() * m_backgroundCache.size().height() > width() * height() * 4) {
        m_backgroundCacheValid = false;
        m_backgroundCache = QPixmap();
    }
}

void EarthquakeMapWidget::spatialIndex()
{
    // Simple spatial indexing - in a real implementation you might use a quadtree
    // For now, just ensure earthquakes are sorted by screen position for faster lookup
    QMutexLocker locker(&m_dataMutex);
    
    std::sort(m_earthquakes.begin(), m_earthquakes.end(), 
              [](const VisualEarthquake &a, const VisualEarthquake &b) {
                  return a.screenPos.x() < b.screenPos.x();
              });
}

void EarthquakeMapWidget::updateBackgroundCache()
{
    if (m_backgroundCacheValid) {
        return;
    }
    
    if (m_backgroundCache.size() != size()) {
        m_backgroundCache = QPixmap(size());
    }
    
    m_backgroundCache.fill(m_settings.backgroundColor);
    
    QPainter cachePainter(&m_backgroundCache);
    if (m_highQualityRendering) {
        cachePainter.setRenderHint(QPainter::Antialiasing);
    }
    
    renderBackground(cachePainter);
    renderMapLayers(cachePainter);
    
    m_backgroundCacheValid = true;
}

void EarthquakeMapWidget::loadBuiltinMapData()
{
    // This could load more detailed map data from resources
    // For now, we use the simple continent polygons loaded in loadDefaultMapData()
    
    // Add more detailed country boundaries
    QPolygonF usa;
    usa << QPointF(-125, 49) << QPointF(-66, 49) << QPointF(-66, 25) << QPointF(-80, 25)
        << QPointF(-95, 29) << QPointF(-125, 32) << QPointF(-125, 49);
    m_countryPolygons.append(usa);
    
    QPolygonF canada;
    canada << QPointF(-140, 70) << QPointF(-60, 70) << QPointF(-60, 49) << QPointF(-125, 49)
           << QPointF(-140, 60) << QPointF(-140, 70);
    m_countryPolygons.append(canada);
    
    // Add more countries as needed...
}

void EarthquakeMapWidget::processMapTiles()
{
    // In a full implementation, this would handle loading and caching map tiles
    // from various map services (OpenStreetMap, etc.)
    // For now, it's a placeholder
}

void EarthquakeMapWidget::cacheMapSegment(const MapBounds &bounds, const QPixmap &segment)
{
    if (m_enableCaching && bounds.isValid() && !segment.isNull()) {
        m_mapTileCache[bounds] = segment;
        
        // Limit cache size
        if (m_mapTileCache.size() > 50) {
            auto oldest = m_mapTileCache.begin();
            m_mapTileCache.erase(oldest);
        }
    }
}

QPixmap EarthquakeMapWidget::getCachedMapSegment(const MapBounds &bounds) const
{
    return m_mapTileCache.value(bounds, QPixmap());
}
