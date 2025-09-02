#pragma once

#include "earthquake_data.hpp"

#include <QtWidgets/QWidget>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>
#include <QtGui/QMouseEvent>
#include <QtGui/QWheelEvent>
#include <QtGui/QKeyEvent>
#include <QtGui/QResizeEvent>
#include <QtGui/QPaintEvent>
#include <QtGui/QContextMenuEvent>
#include <QtCore/QTimer>
#include <QtCore/QDateTime>
#include <QtCore/QPropertyAnimation>
#include <QtCore/QEasingCurve>
#include <QtCore/QParallelAnimationGroup>
#include <QtCore/QSequentialAnimationGroup>
#include <QtCore/QSettings>
#include <QtWidgets/QMenu>
#include <QAction>
#include <QtWidgets/QToolTip>
#include <QtWidgets/QRubberBand>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QVector>
#include <QMap>
#include <QMutex>
#include <QMutexLocker>
#include <cmath>
#include <QByteArray>

// Forward declarations
class SpatialUtils;

// Enumerations
enum class MapProjection {
    Mercator,
    Equirectangular,
    OrthographicNorthPole,
    OrthographicSouthPole,
    Robinson
};

enum class MapLayer {
    Continents,
    Countries,
    States,
    Cities,
    PlateBoundaries,
    Topography,
    Bathymetry,
    Roads,
    Custom
};

enum class EarthquakeDisplayMode {
    Circles,
    Squares,
    Diamonds,
    Crosses,
    Heatmap,
    Density,
    Animation
};

enum class ColorScheme {
    Magnitude,
    Depth,
    Age,
    AlertLevel,
    DataSource,
    Custom
};

enum class AnimationStyle {
    None,
    Pulse,
    Ripple,
    Fade,
    Grow,
    Shake
};

// Supporting structures
struct MapBounds {
    double minLatitude;
    double maxLatitude;
    double minLongitude;
    double maxLongitude;
    
    bool contains(double lat, double lon) const {
        return lat >= minLatitude && lat <= maxLatitude &&
               lon >= minLongitude && lon <= maxLongitude;
    }

    bool operator<(const MapBounds& other) const {
        return std::tie(minLatitude, maxLatitude, minLongitude, maxLongitude) <
               std::tie(other.minLatitude, other.maxLatitude, other.minLongitude, other.maxLongitude);
    }

    bool isValid() const {
        return minLatitude < maxLatitude && minLongitude < maxLongitude;
    }
    
    double width() const { return maxLongitude - minLongitude; }
    double height() const { return maxLatitude - minLatitude; }
    QPointF center() const { 
        return QPointF((minLongitude + maxLongitude) / 2.0, 
                      (minLatitude + maxLatitude) / 2.0); 
    }
};

struct MapSettings {
    MapProjection projection = MapProjection::Mercator;
    QVector<MapLayer> enabledLayers = {MapLayer::Continents, MapLayer::Countries};
    EarthquakeDisplayMode displayMode = EarthquakeDisplayMode::Circles;
    ColorScheme colorScheme = ColorScheme::Magnitude;
    AnimationStyle animationStyle = AnimationStyle::Pulse;
    bool showGrid = true;
    bool showLegend = true;
    bool showTooltips = true;
    bool showMagnitudeLabels = true;
    bool showTimeLabels = false;
    bool enableClustering = true;
    bool enableFiltering = true;
    bool enableAnimation = true;
    double gridSpacing = 15.0; // degrees
    double clusterDistance = 50.0; // pixels
    double animationSpeed = 1.0; // multiplier
    int maxVisibleEarthquakes = 5000;
    QColor backgroundColor = QColor(20, 30, 50);
    QColor gridColor = QColor(60, 80, 100);
    QColor coastlineColor = QColor(100, 120, 140);
};

struct VisualEarthquake {
    EarthquakeData data;
    QPointF screenPos;
    double displaySize;
    QColor displayColor;
    double opacity;
    double animationPhase;
    bool isVisible;
    bool isHighlighted;
    bool isSelected;
    QDateTime lastUpdate;
    int clusterId;
    bool isClusterCenter;
    QVector<int> clusteredIds;
};

struct EarthquakeCluster {
    QPointF centerPos;
    QVector<int> earthquakeIds;
    double avgMagnitude;
    double maxMagnitude;
    QDateTime latestTime;
    QColor displayColor;
    double displaySize;
    bool isExpanded;
};

class EarthquakeMapWidget : public QWidget
{
    Q_OBJECT
    // Properties for animation and binding
    Q_PROPERTY(double centerLatitude READ getCenterLatitude WRITE setCenterLatitude NOTIFY centerChanged)
    Q_PROPERTY(double centerLongitude READ getCenterLongitude WRITE setCenterLongitude NOTIFY centerChanged)
    Q_PROPERTY(double zoomLevel READ getZoomLevel WRITE setZoomLevel NOTIFY zoomChanged)
    Q_PROPERTY(double animationOpacity READ getAnimationOpacity WRITE setAnimationOpacity)
    Q_PROPERTY(bool showGrid READ getShowGrid WRITE setShowGrid NOTIFY showGridChanged)
    Q_PROPERTY(bool showLegend READ getShowLegend WRITE setShowLegend NOTIFY showLegendChanged)

public:
    explicit EarthquakeMapWidget(QWidget *parent = nullptr);
    ~EarthquakeMapWidget();

    // Core functionality
    void addEarthquake(const EarthquakeData &earthquake);
    void addEarthquakes(const QVector<EarthquakeData> &earthquakes);
    void removeEarthquake(const QString &eventId);
    void clearEarthquakes();
    void updateEarthquake(const EarthquakeData &earthquake);
    
    // Map control
    void setCenter(double latitude, double longitude);
    void setCenterLatitude(double latitude);
    void setCenterLongitude(double longitude);
    void setZoomLevel(double zoom);
    bool getShowGrid() const { return m_showGrid; }
    void setShowGrid(bool show);
    bool getShowLegend() const { return m_showLegend; }
    void setShowLegend(bool show);
    void zoomIn();
    void zoomOut();
    void fitToEarthquakes();
    void fitToBounds(const MapBounds &bounds);
    
    // View settings
    void setMapSettings(const MapSettings &settings);
    MapSettings getMapSettings() const;
    void setProjection(MapProjection projection);
    void setDisplayMode(EarthquakeDisplayMode mode);
    void setColorScheme(ColorScheme scheme);
    void setAnimationStyle(AnimationStyle style);
    
    // Layer management
    void setLayerEnabled(MapLayer layer, bool enabled);
    bool isLayerEnabled(MapLayer layer) const;
    void setBackgroundMap(const QPixmap &map);
    void loadBackgroundMapFromUrl(const QString &url);
    
    // Selection and interaction
    QVector<EarthquakeData> getSelectedEarthquakes() const;
    EarthquakeData getEarthquakeAt(const QPoint &point) const;
    QVector<EarthquakeData> getEarthquakesInRegion(const MapBounds &bounds) const;
    void selectEarthquake(const QString &eventId);
    void selectEarthquakes(const QStringList &eventIds);
    void clearSelection();
    
    // Filtering and clustering
    void setMagnitudeFilter(double minMag, double maxMag);
    void setDepthFilter(double minDepth, double maxDepth);
    void setTimeFilter(const QDateTime &startTime, const QDateTime &endTime);
    void setLocationFilter(const MapBounds &bounds);
    void enableClustering(bool enabled);
    void setClusterDistance(double pixels);
    
    // Animation and effects
    void startAnimation();
    void stopAnimation();
    void setAnimationSpeed(double speed);
    void highlightEarthquake(const QString &eventId, int durationMs = 3000);
    void flashEarthquake(const QString &eventId, int times = 3);
    void animateToLocation(double latitude, double longitude, double zoom = -1, int durationMs = 1000);
    
    // Export and utility
    QPixmap renderToPixmap(const QSize &size = QSize()) const;
    void exportToImage(const QString &fileName, const QSize &size = QSize()) const;
    QByteArray exportToSvg(const QSize &size = QSize()) const;
    void saveSettings() const;
    void loadSettings();
    
    // Getters
    double getCenterLatitude() const { return m_centerLatitude; }
    double getCenterLongitude() const { return m_centerLongitude; }
    double getZoomLevel() const { return m_zoomLevel; }
    double getAnimationOpacity() const { return m_animationOpacity; }
    MapBounds getVisibleBounds() const;
    void updateVisibleBounds();
    QVector<EarthquakeData> getAllEarthquakes() const;
    QVector<EarthquakeData> getVisibleEarthquakes() const;
    int getEarthquakeCount() const;
    QDateTime getLastUpdateTime() const;

signals:
    void earthquakeClicked(const EarthquakeData &earthquake);
    void earthquakeDoubleClicked(const EarthquakeData &earthquake);
    void earthquakeHovered(const EarthquakeData &earthquake);
    void earthquakeSelected(const QStringList &eventIds);
    void mapClicked(double latitude, double longitude);
    void centerChanged(double latitude, double longitude);
    void zoomChanged(double zoom);
    void showGridChanged(bool show);
    void showLegendChanged(bool show);
    void boundsChanged(const MapBounds &bounds);
    void selectionChanged(const QVector<EarthquakeData> &selected);
    void contextMenuRequested(const QPoint &position, const EarthquakeData &earthquake);
    void backgroundMapLoaded();
    void animationFrameUpdated(int frame);

protected:
    // Event handling
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void leaveEvent(QEvent* event) override;

    void renderBackgroundWithCache(QPainter& painter);
    void renderDynamicContent(QPainter& painter);
    void renderUIOverlays(QPainter& painter);
    void renderEarthquakesOptimized(QPainter& painter);
    void renderSingleEarthquake(QPainter& painter, const VisualEarthquake& eq);
    void renderEarthquakeCircle(QPainter& painter, const QPointF& center,
                                               double size, const QColor& fillColor,
                                               const QColor& borderColor, bool selected);
    void renderEarthquakeSquare(QPainter& painter, const QPointF& center,
                                               double size, const QColor& fillColor,
                                               const QColor& borderColor, bool selected);
    void renderEarthquakeDiamond(QPainter& painter, const QPointF& center,
                                                double size, const QColor& fillColor,
                                                const QColor& borderColor, bool selected);
    void renderEarthquakeCross(QPainter& painter, const QPointF& center,
                                              double size, const QColor& color, bool selected);
    void renderEarthquakeLabels(QPainter& painter, const QVector<int>& visibleIndices, int maxRender);
    void renderSelection(QPainter& painter);
    void renderHoverEffects(QPainter& painter);
    void renderCoordinateDisplay(QPainter& painter);
    void renderStatusOverlays(QPainter& painter);

private slots:
    void updateAnimation();
    void onNetworkReplyFinished();
    void onAnimationFinished();
    void onSelectionAnimationFinished();

private:
    // Initialization
    void initializeWidget();
    void initializeAnimations();
    void initializeContextMenu();
    void loadDefaultMapData();
    void setupRenderingHints();
    
    // Coordinate transformation
    QPointF latLonToScreen(double latitude, double longitude) const;
    QPointF screenToLatLon(const QPointF &screen) const;
    QPointF projectCoordinate(double latitude, double longitude) const;
    QPointF unprojectCoordinate(const QPointF &projected) const;
    
    // Projection implementations
    QPointF mercatorProjection(double lat, double lon) const;
    QPointF equirectangularProjection(double lat, double lon) const;
    QPointF orthographicProjection(double lat, double lon, bool northPole) const;
    QPointF robinsonProjection(double lat, double lon) const;
    
    // Rendering methods
    void renderBackground(QPainter &painter) const;
    void renderMapLayers(QPainter &painter) const;
    void renderContinents(QPainter &painter) const;
    void renderCountries(QPainter &painter) const;
    void renderGrid(QPainter &painter) const;
    void renderEarthquakes(QPainter &painter) const;
    void renderClusters(QPainter &painter) const;
    void renderSelection(QPainter &painter) const;
    void renderLegend(QPainter &painter) const;
    void renderOverlay(QPainter &painter) const;
    void renderDebugInfo(QPainter &painter) const;
    void renderScaleBar(QPainter &painter) const;
    
    // Earthquake rendering
    void renderEarthquake(QPainter &painter, const VisualEarthquake &eq) const;
    void renderEarthquakeCircle(QPainter &painter, const VisualEarthquake &eq) const;
    void renderEarthquakeSquare(QPainter &painter, const VisualEarthquake &eq) const;
    void renderEarthquakeDiamond(QPainter &painter, const VisualEarthquake &eq) const;
    void renderEarthquakeCross(QPainter &painter, const VisualEarthquake &eq) const;
    void renderEarthquakeHeatmap(QPainter &painter) const;
    void renderEarthquakeLabels(QPainter &painter) const;

public:
    // Color and styling
    QColor getEarthquakeColor(const EarthquakeData &earthquake) const;
    QColor getMagnitudeColor(double magnitude) const;
    QColor getDepthColor(double depth) const;
    QColor getAgeColor(const QDateTime &timestamp) const;
    QColor getAlertLevelColor(int alertLevel) const;
    double getEarthquakeSize(const EarthquakeData &earthquake) const;
    double getScaledSize(double baseSize) const;

private:
    // Clustering
    void updateClusters();
    void clearClusters();
    bool shouldCluster(const VisualEarthquake &eq1, const VisualEarthquake &eq2) const;
    EarthquakeCluster createCluster(const QVector<int> &earthquakeIds);
    void expandCluster(int clusterId);
    void collapseCluster(int clusterId);
    
    // Filtering and culling
    void updateVisibleEarthquakes();
    bool isEarthquakeVisible(const EarthquakeData &earthquake) const;
    bool passesFilters(const EarthquakeData &earthquake) const;
    bool isInViewport(const QPointF &screenPos) const;
    
    // Hit testing
    int findEarthquakeAt(const QPoint &point) const;
    QVector<int> findEarthquakesInRect(const QRect &rect) const;
    double distanceToEarthquake(const QPoint &point, int earthquakeIndex) const;
    
    // Animation helpers
    void updateEarthquakeAnimations();
    double calculateAnimationPhase(const EarthquakeData &earthquake) const;
    double getAnimationValue(AnimationStyle style, double phase) const;
    void setAnimationOpacity(double opacity) { m_animationOpacity = opacity; update(); }
    
    // Map data management
    void updateBackgroundCache();
    void loadBuiltinMapData();
    void processMapTiles();
    void cacheMapSegment(const MapBounds &bounds, const QPixmap &segment);
    QPixmap getCachedMapSegment(const MapBounds &bounds) const;
    
    // Performance optimization
    void optimizeForPerformance();
    void updateLevelOfDetail();
    bool shouldSkipRendering(const VisualEarthquake &eq) const;
    void cullOffscreenEarthquakes();
    void spatialIndex();
    
    // Utility methods
    MapBounds calculateBounds(const QVector<EarthquakeData> &earthquakes) const;
    double calculateOptimalZoom(const MapBounds &bounds) const;
    QString formatCoordinate(double value, bool isLatitude) const;
    QString formatEarthquakeTooltip(const EarthquakeData &earthquake) const;
    void showTooltip(const QPoint &pos, const QString &text);
    void hideTooltip();

private:
    // Core data
    QVector<VisualEarthquake> m_earthquakes;
    QVector<EarthquakeCluster> m_clusters;
    QStringList m_selectedIds;
    mutable QMutex m_dataMutex;
    
    // Map state
    double m_centerLatitude;
    double m_centerLongitude;
    double m_zoomLevel;
    bool m_showGrid;
    bool m_showLegend;
    MapSettings m_settings;
    MapBounds m_visibleBounds;
    
    // Interaction state
    bool m_isPanning;
    bool m_isSelecting;
    QPoint m_lastMousePos;
    QPoint m_panStartPos;
    QPointF m_panStartCenter;
    QRubberBand *m_selectionBand;
    QPoint m_selectionStart;
    QString m_hoveredEarthquakeId;
    
    // Animation system
    QTimer *m_animationTimer;
    QPropertyAnimation *m_centerAnimation;
    QPropertyAnimation *m_zoomAnimation;
    QParallelAnimationGroup *m_moveAnimationGroup;
    int m_animationFrame;
    double m_animationOpacity;
    bool m_animationEnabled;
    
    // Rendering cache
    mutable QPixmap m_backgroundCache;
    mutable QPixmap m_layerCache;
    mutable bool m_backgroundCacheValid;
    mutable bool m_layerCacheValid;
    mutable QSize m_lastSize;
    
    // Map data
    QPixmap m_backgroundMap;
    QVector<QPolygonF> m_continentPolygons;
    QVector<QPolygonF> m_countryPolygons;
    QMap<MapBounds, QPixmap> m_mapTileCache;
    
    // Network for map loading
    QNetworkAccessManager *m_networkManager;
    
    // Filtering
    double m_minMagnitude;
    double m_maxMagnitude;
    double m_minDepth;
    double m_maxDepth;
    QDateTime m_startTime;
    QDateTime m_endTime;
    MapBounds m_locationFilter;
    bool m_hasLocationFilter;
    
    // Performance settings
    bool m_highQualityRendering;
    int m_maxRenderingEarthquakes;
    double m_lodThreshold;
    bool m_enableCaching;
    
    // Context menu
    QMenu *m_contextMenu;
    QAction *m_zoomInAction;
    QAction *m_zoomOutAction;
    QAction *m_centerHereAction;
    QAction *m_selectAllAction;
    QAction *m_clearSelectionAction;
    QAction *m_copyLocationAction;
    QAction *m_exportImageAction;
    
    // Constants
    static const double MIN_ZOOM;
    static const double MAX_ZOOM;
    static const double ZOOM_FACTOR;
    static const int ANIMATION_FPS;
    static const int MAX_TOOLTIP_WIDTH;
    static const double EARTH_RADIUS_KM;
    static const double DEFAULT_EARTHQUAKE_SIZE;
    static const int CLUSTER_EXPAND_DURATION_MS;
};

Q_DECLARE_METATYPE(EarthquakeMapWidget)
