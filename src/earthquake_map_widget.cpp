// EarthquakeMapWidget.cpp
#include "earthquake_map_widget.hpp"
#include "spatial_utils.hpp"
#include <QtCore/QDebug>
#include <QtCore/QSettings>
#include <QtCore/QStandardPaths>
#include <QtWidgets/QApplication>
#include <QtWidgets/QToolTip>
#include <QtWidgets/QRubberBand>
#include <QtGui/QPainterPath>
#include <QtGui/QPixmap>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtSvg/QSvgGenerator>

// Constants
const double EarthquakeMapWidget::MIN_ZOOM = 0.1;
const double EarthquakeMapWidget::MAX_ZOOM = 50.0;
const double EarthquakeMapWidget::ZOOM_FACTOR = 1.5;
const int EarthquakeMapWidget::ANIMATION_FPS = 30;
const int EarthquakeMapWidget::MAX_TOOLTIP_WIDTH = 300;
const double EarthquakeMapWidget::EARTH_RADIUS_KM = 6371.0;
const double EarthquakeMapWidget::DEFAULT_EARTHQUAKE_SIZE = 8.0;
const int EarthquakeMapWidget::CLUSTER_EXPAND_DURATION_MS = 300;

EarthquakeMapWidget::EarthquakeMapWidget(QWidget *parent)
    : QWidget(parent)
    , m_centerLatitude(0.0)
    , m_centerLongitude(0.0)
    , m_zoomLevel(1.0)
    , m_isPanning(false)
    , m_isSelecting(false)
    , m_selectionBand(nullptr)
    , m_animationTimer(nullptr)
    , m_centerAnimation(nullptr)
    , m_zoomAnimation(nullptr)
    , m_moveAnimationGroup(nullptr)
    , m_animationFrame(0)
    , m_animationOpacity(1.0)
    , m_animationEnabled(true)
    , m_backgroundCacheValid(false)
    , m_layerCacheValid(false)
    , m_networkManager(nullptr)
    , m_minMagnitude(0.0)
    , m_maxMagnitude(10.0)
    , m_minDepth(0.0)
    , m_maxDepth(1000.0)
    , m_hasLocationFilter(false)
    , m_highQualityRendering(true)
    , m_maxRenderingEarthquakes(10000)
    , m_lodThreshold(0.5)
    , m_enableCaching(true)
    , m_contextMenu(nullptr)
{
    initializeWidget();
    initializeAnimations();
    initializeContextMenu();
    loadDefaultMapData();
    loadSettings();
}

EarthquakeMapWidget::~EarthquakeMapWidget()
{
    saveSettings();
    
    if (m_animationTimer) {
        m_animationTimer->stop();
    }
    
    clearClusters();
}

void EarthquakeMapWidget::initializeWidget()
{
    // Widget setup
    setMinimumSize(400, 300);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);
    
    // Initialize bounds
    updateVisibleBounds();
    
    // Initialize network manager
    m_networkManager = new QNetworkAccessManager(this);
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &EarthquakeMapWidget::onNetworkReplyFinished);
    
    // Setup rendering
    setupRenderingHints();
    
    qDebug() << "EarthquakeMapWidget initialized";
}

void EarthquakeMapWidget::initializeAnimations()
{
    // Main animation timer
    m_animationTimer = new QTimer(this);
    connect(m_animationTimer, &QTimer::timeout, this, &EarthquakeMapWidget::updateAnimation);
    m_animationTimer->start(1000 / ANIMATION_FPS); // 30 FPS
    
    // Center animation
    m_centerAnimation = new QPropertyAnimation(this, "centerLatitude", this);
    m_centerAnimation->setDuration(1000);
    m_centerAnimation->setEasingCurve(QEasingCurve::InOutQuad);
    connect(m_centerAnimation, &QPropertyAnimation::finished, 
            this, &EarthquakeMapWidget::onAnimationFinished);
    
    // Zoom animation
    m_zoomAnimation = new QPropertyAnimation(this, "zoomLevel", this);
    m_zoomAnimation->setDuration(800);
    m_zoomAnimation->setEasingCurve(QEasingCurve::InOutQuad);
    
    // Parallel animation group for coordinated movements
    m_moveAnimationGroup = new QParallelAnimationGroup(this);
    m_moveAnimationGroup->addAnimation(m_centerAnimation);
    m_moveAnimationGroup->addAnimation(m_zoomAnimation);
    
    qDebug() << "Animation system initialized";
}

void EarthquakeMapWidget::initializeContextMenu()
{
    m_contextMenu = new QMenu(this);
    
    m_zoomInAction = m_contextMenu->addAction("Zoom In");
    m_zoomInAction->setShortcut(QKeySequence::ZoomIn);
    connect(m_zoomInAction, &QAction::triggered, this, &EarthquakeMapWidget::zoomIn);
    
    m_zoomOutAction = m_contextMenu->addAction("Zoom Out");
    m_zoomOutAction->setShortcut(QKeySequence::ZoomOut);
    connect(m_zoomOutAction, &QAction::triggered, this, &EarthquakeMapWidget::zoomOut);
    
    m_contextMenu->addSeparator();
    
    m_centerHereAction = m_contextMenu->addAction("Center Here");
    connect(m_centerHereAction, &QAction::triggered, [this]() {
        QPoint pos = m_contextMenu->pos();
        QPointF latLon = screenToLatLon(mapFromGlobal(pos));
        setCenter(latLon.y(), latLon.x());
    });
    
    m_contextMenu->addSeparator();
    
    m_selectAllAction = m_contextMenu->addAction("Select All Visible");
    m_selectAllAction->setShortcut(QKeySequence::SelectAll);
    connect(m_selectAllAction, &QAction::triggered, [this]() {
        QStringList visibleIds;
        for (const auto &eq : m_earthquakes) {
            if (eq.isVisible) {
                visibleIds.append(eq.data.eventId);
            }
        }
        selectEarthquakes(visibleIds);
    });
    
    m_clearSelectionAction = m_contextMenu->addAction("Clear Selection");
    connect(m_clearSelectionAction, &QAction::triggered, this, &EarthquakeMapWidget::clearSelection);
    
    m_contextMenu->addSeparator();
    
    m_copyLocationAction = m_contextMenu->addAction("Copy Coordinates");
    connect(m_copyLocationAction, &QAction::triggered, [this]() {
        QPoint pos = m_contextMenu->pos();
        QPointF latLon = screenToLatLon(mapFromGlobal(pos));
        QString coords = QString("%1, %2").arg(latLon.y(), 0, 'f', 6).arg(latLon.x(), 0, 'f', 6);
        QApplication::clipboard()->setText(coords);
    });
    
    m_exportImageAction = m_contextMenu->addAction("Export as Image...");
    connect(m_exportImageAction, &QAction::triggered, [this]() {
        QString fileName = QString("earthquake_map_%1.png")
                          .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss"));
        exportToImage(fileName);
    });
}

void EarthquakeMapWidget::loadDefaultMapData()
{
    // Load simplified world map data
    // In a real implementation, this would load from GeoJSON or Shapefile
    
    // Create simplified continent polygons
    QPolygonF northAmerica;
    northAmerica << QPointF(-150, 70) << QPointF(-50, 70) 
                 << QPointF(-80, 25) << QPointF(-120, 10) << QPointF(-150, 70);
    m_continentPolygons.append(northAmerica);
    
    QPolygonF southAmerica;
    southAmerica << QPointF(-80, 15) << QPointF(-40, 15)
                 << QPointF(-50, -55) << QPointF(-80, -20) << QPointF(-80, 15);
    m_continentPolygons.append(southAmerica);
    
    QPolygonF eurasia;
    eurasia << QPointF(-10, 75) << QPointF(180, 75)
            << QPointF(140, 10) << QPointF(30, 35) << QPointF(-10, 75);
    m_continentPolygons.append(eurasia);
    
    QPolygonF africa;
    africa << QPointF(-20, 35) << QPointF(50, 35)
           << QPointF(40, -35) << QPointF(10, -35) << QPointF(-20, 35);
    m_continentPolygons.append(africa);
    
    QPolygonF australia;
    australia << QPointF(110, -10) << QPointF(155, -10)
              << QPointF(155, -45) << QPointF(110, -45) << QPointF(110, -10);
    m_continentPolygons.append(australia);
    
    m_backgroundCacheValid = false;
    m_layerCacheValid = false;
    
    qDebug() << "Default map data loaded";
}

void EarthquakeMapWidget::setupRenderingHints()
{
    // This will be used in paintEvent for consistent rendering quality
}

void EarthquakeMapWidget::addEarthquake(const EarthquakeData &earthquake)
{
    QMutexLocker locker(&m_dataMutex);
    
    // Check if earthquake already exists
    for (int i = 0; i < m_earthquakes.size(); ++i) {
        if (m_earthquakes[i].data.eventId == earthquake.eventId) {
            // Update existing earthquake
            m_earthquakes[i].data = earthquake;
            m_earthquakes[i].lastUpdate = QDateTime::currentDateTime();
            updateVisibleEarthquakes();
            update();
            return;
        }
    }
    
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
    
    // Update clustering if enabled
    if (m_settings.enableClustering) {
        update();
    }
}







////////////////////////////////////////////////////////////////////////////////




        QPainter cachePainter(&m_backgroundCache);
        if (m_highQualityRendering) {
            cachePainter.setRenderHint(QPainter::Antialiasing);
        }
        
        renderBackground(cachePainter);
        renderMapLayers(cachePainter);
        
        m_backgroundCacheValid = true;
    }
    
    painter.drawPixmap(0, 0, m_backgroundCache);
    
    // Render earthquakes and overlays
    renderEarthquakes(painter);
    if (m_settings.enableClustering) {
        renderClusters(painter);
    }
    renderSelection(painter);
    
    if (m_settings.showLegend) {
        renderLegend(painter);
    }
    
    renderOverlay(painter);
    
    #ifdef QT_DEBUG
    if (QApplication::keyboardModifiers() & Qt::ControlModifier) {
        renderDebugInfo(painter);
    }
    #endif
}

void EarthquakeMapWidget::renderBackground(QPainter &painter) const
{
    // Draw background map if available
    if (!m_backgroundMap.isNull()) {
        painter.drawPixmap(rect(), m_backgroundMap, m_backgroundMap.rect());
    }
}

void EarthquakeMapWidget::renderMapLayers(QPainter &painter) const
{
    if (m_settings.enabledLayers.contains(MapLayer::Continents)) {
        renderContinents(painter);
    }
    
    if (m_settings.enabledLayers.contains(MapLayer::Countries)) {
        renderCountries(painter);
    }
    
    if (m_settings.showGrid) {
        renderGrid(painter);
    }
}

void EarthquakeMapWidget::renderContinents(QPainter &painter) const
{
    painter.setPen(QPen(m_settings.coastlineColor, 1));
    painter.setBrush(QColor(40, 60, 80, 128));
    
    for (const QPolygonF &continent : m_continentPolygons) {
        QPolygonF screenPolygon;
        for (const QPointF &point : continent) {
            QPointF screenPoint = latLonToScreen(point.y(), point.x());
            if (rect().contains(screenPoint.toPoint())) {
                screenPolygon.append(screenPoint);
            }
        }
        
        if (screenPolygon.size() >= 3) {
            painter.drawPolygon(screenPolygon);
        }
    }
}

void EarthquakeMapWidget::renderCountries(QPainter &painter) const
{
    painter.setPen(QPen(m_settings.coastlineColor.lighter(), 1, Qt::DotLine));
    
    for (const QPolygonF &country : m_countryPolygons) {
        QPolygonF screenPolygon;
        for (const QPointF &point : country) {
            QPointF screenPoint = latLonToScreen(point.y(), point.x());
            screenPolygon.append(screenPoint);
        }
        
        if (screenPolygon.size() >= 2) {
            painter.drawPolyline(screenPolygon);
        }
    }
}

void EarthquakeMapWidget::renderGrid(QPainter &painter) const
{
    painter.setPen(QPen(m_settings.gridColor, 1, Qt::DotLine));
    
    double spacing = m_settings.gridSpacing;
    
    // Latitude lines
    for (double lat = -90; lat <= 90; lat += spacing) {
        QPointF start = latLonToScreen(lat, -180);
        QPointF end = latLonToScreen(lat, 180);
        
        if (rect().intersects(QRectF(start, end).normalized().toRect())) {
            painter.drawLine(start, end);
        }
    }
    
    // Longitude lines  
    for (double lon = -180; lon <= 180; lon += spacing) {
        QPointF start = latLonToScreen(-90, lon);
        QPointF end = latLonToScreen(90, lon);
        
        if (rect().intersects(QRectF(start, end).normalized().toRect())) {
            painter.drawLine(start, end);
        }
    }
}

void EarthquakeMapWidget::renderEarthquakes(QPainter &painter) const
{
    QMutexLocker locker(&m_dataMutex);
    
    // Sort earthquakes by size (render smaller ones first)
    QVector<int> indices;
    for (int i = 0; i < m_earthquakes.size(); ++i) {
        if (m_earthquakes[i].isVisible && !shouldSkipRendering(m_earthquakes[i])) {
            indices.append(i);
        }
    }
    
    std::sort(indices.begin(), indices.end(), [this](int a, int b) {
        return m_earthquakes[a].displaySize < m_earthquakes[b].displaySize;
    });
    
    // Limit rendering count for performance
    int maxRender = qMin(indices.size(), m_maxRenderingEarthquakes);
    
    for (int i = 0; i < maxRender; ++i) {
        const VisualEarthquake &eq = m_earthquakes[indices[i]];
        renderEarthquake(painter, eq);
    }
    
    // Render labels separately on top
    if (m_settings.showMagnitudeLabels || m_settings.showTimeLabels) {
        renderEarthquakeLabels(painter);
    }
}

void EarthquakeMapWidget::renderEarthquake(QPainter &painter, const VisualEarthquake &eq) const
{
    if (!isInViewport(eq.screenPos)) {
        return;
    }
    
    painter.save();
    painter.setOpacity(eq.opacity * m_animationOpacity);
    
    switch (m_settings.displayMode) {
        case EarthquakeDisplayMode::Circles:
            renderEarthquakeCircle(painter, eq);
            break;
        case EarthquakeDisplayMode::Squares:
            renderEarthquakeSquare(painter, eq);
            break;
        case EarthquakeDisplayMode::Diamonds:
            renderEarthquakeDiamond(painter, eq);
            break;
        case EarthquakeDisplayMode::Crosses:
            renderEarthquakeCross(painter, eq);
            break;
        case EarthquakeDisplayMode::Heatmap:
            // Handled separately in renderEarthquakeHeatmap
            break;
        default:
            renderEarthquakeCircle(painter, eq);
            break;
    }
    
    painter.restore();
}

void EarthquakeMapWidget::renderEarthquakeCircle(QPainter &painter, const VisualEarthquake &eq) const
{
    double size = getScaledSize(eq.displaySize);
    
    // Apply animation effects
    if (m_settings.animationStyle != AnimationStyle::None && m_settings.enableAnimation) {
        double animValue = getAnimationValue(m_settings.animationStyle, eq.animationPhase);
        size *= animValue;
    }
    
    QColor baseColor = eq.displayColor;
    QColor borderColor = baseColor.darker(150);
    
    // Highlight effect
    if (eq.isHighlighted) {
        baseColor = baseColor.lighter(150);
        borderColor = QColor(255, 255, 100);
        size *= 1.3;
    }
    
    // Selection effect
    if (eq.isSelected) {
        borderColor = QColor(100, 150, 255);
        painter.setPen(QPen(borderColor, 3));
    } else {
        painter.setPen(QPen(borderColor, 1));
    }
    
    painter.setBrush(baseColor);
    
    QRectF rect(eq.screenPos.x() - size/2, eq.screenPos.y() - size/2, size, size);
    painter.drawEllipse(rect);
    
    // Draw magnitude text for larger earthquakes
    if (eq.data.magnitude >= 5.0 && size > 15) {
        painter.setPen(Qt::white);
        painter.setFont(QFont("Arial", qMax(8, int(size/3))));
        QString magText = QString::number(eq.data.magnitude, 'f', 1);
        painter.drawText(rect, Qt::AlignCenter, magText);
    }
}

void EarthquakeMapWidget::renderEarthquakeSquare(QPainter &painter, const VisualEarthquake &eq) const
{
    double size = getScaledSize(eq.displaySize);
    
    if (m_settings.animationStyle != AnimationStyle::None && m_settings.enableAnimation) {
        double animValue = getAnimationValue(m_settings.animationStyle, eq.animationPhase);
        size *= animValue;
    }
    
    QColor baseColor = eq.displayColor;
    QColor borderColor = baseColor.darker(150);
    
    if (eq.isHighlighted) {
        baseColor = baseColor.lighter(150);
        size *= 1.3;
    }
    
    painter.setPen(QPen(borderColor, eq.isSelected ? 3 : 1));
    painter.setBrush(baseColor);
    
    QRectF rect(eq.screenPos.x() - size/2, eq.screenPos.y() - size/2, size, size);
    painter.drawRect(rect);
}

void EarthquakeMapWidget::renderEarthquakeDiamond(QPainter &painter, const VisualEarthquake &eq) const
{
    double size = getScaledSize(eq.displaySize);
    
    if (m_settings.animationStyle != AnimationStyle::None && m_settings.enableAnimation) {
        double animValue = getAnimationValue(m_settings.animationStyle, eq.animationPhase);
        size *= animValue;
    }
    
    QColor baseColor = eq.displayColor;
    QColor borderColor = baseColor.darker(150);
    
    if (eq.isHighlighted) {
        baseColor = baseColor.lighter(150);
        size *= 1.3;
    }
    
    painter.setPen(QPen(borderColor, eq.isSelected ? 3 : 1));
    painter.setBrush(baseColor);
    
    // Create diamond shape
    QPolygonF diamond;
    double halfSize = size / 2;
    diamond << QPointF(eq.screenPos.x(), eq.screenPos.y() - halfSize)      // top
            << QPointF(eq.screenPos.x() + halfSize, eq.screenPos.y())      // right
            << QPointF(eq.screenPos.x(), eq.screenPos.y() + halfSize)      // bottom
            << QPointF(eq.screenPos.x() - halfSize, eq.screenPos.y());     // left
    
    painter.drawPolygon(diamond);
}

void EarthquakeMapWidget::renderEarthquakeCross(QPainter &painter, const VisualEarthquake &eq) const
{
    double size = getScaledSize(eq.displaySize);
    
    if (m_settings.animationStyle != AnimationStyle::None && m_settings.enableAnimation) {
        double animValue = getAnimationValue(m_settings.animationStyle, eq.animationPhase);
        size *= animValue;
    }
    
    QColor color = eq.displayColor;
    if (eq.isHighlighted) {
        color = color.lighter(150);
        size *= 1.3;
    }
    
    painter.setPen(QPen(color, eq.isSelected ? 4 : 2));
    
    double halfSize = size / 2;
    
    // Horizontal line
    painter.drawLine(eq.screenPos.x() - halfSize, eq.screenPos.y(),
                    eq.screenPos.x() + halfSize, eq.screenPos.y());
    
    // Vertical line
    painter.drawLine(eq.screenPos.x(), eq.screenPos.y() - halfSize,
                    eq.screenPos.x(), eq.screenPos.y() + halfSize);
}

void EarthquakeMapWidget::renderEarthquakeLabels(QPainter &painter) const
{
    QMutexLocker locker(&m_dataMutex);
    
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 9, QFont::Bold));
    
    for (const VisualEarthquake &eq : m_earthquakes) {
        if (!eq.isVisible || !isInViewport(eq.screenPos)) {
            continue;
        }
        
        QString label;
        if (m_settings.showMagnitudeLabels && eq.data.magnitude >= 4.0) {
            label = QString("M%1").arg(eq.data.magnitude, 0, 'f', 1);
        }
        
        if (m_settings.showTimeLabels) {
            QString timeStr = eq.data.timestamp.toString("hh:mm");
            label += (label.isEmpty() ? "" : " ") + timeStr;
        }
        
        if (!label.isEmpty()) {
            QRectF textRect = painter.fontMetrics().boundingRect(label);
            QPointF textPos = eq.screenPos + QPointF(eq.displaySize/2 + 5, -eq.displaySize/2);
            
            // Background for better readability
            painter.fillRect(textRect.translated(textPos), QColor(0, 0, 0, 128));
            painter.drawText(textPos, label);
        }
    }
}

void EarthquakeMapWidget::renderClusters(QPainter &painter) const
{
    for (const EarthquakeCluster &cluster : m_clusters) {
        if (cluster.earthquakeIds.size() < 2) {
            continue;
        }
        
        painter.save();
        
        // Cluster background circle
        painter.setPen(QPen(cluster.displayColor.darker(), 2));
        painter.setBrush(QColor(cluster.displayColor.red(), cluster.displayColor.green(), 
                               cluster.displayColor.blue(), 180));
        
        double radius = cluster.displaySize;
        QRectF clusterRect(cluster.centerPos.x() - radius, cluster.centerPos.y() - radius,
                          radius * 2, radius * 2);
        painter.drawEllipse(clusterRect);
        
        // Cluster count text
        painter.setPen(Qt::white);
        painter.setFont(QFont("Arial", qMax(10, int(radius/3)), QFont::Bold));
        painter.drawText(clusterRect, Qt::AlignCenter, QString::number(cluster.earthquakeIds.size()));
        
        // Magnitude indicator
        if (cluster.maxMagnitude >= 5.0) {
            painter.setPen(QPen(Qt::yellow, 2));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(clusterRect.adjusted(-3, -3, 3, 3));
        }
        
        painter.restore();
    }
}

void EarthquakeMapWidget::renderSelection(QPainter &painter) const
{
    // Render selection rubber band
    if (m_selectionBand && m_selectionBand->isVisible()) {
        // Qt handles this automatically
    }
    
    // Render selection highlights
    painter.setPen(QPen(QColor(100, 150, 255), 3));
    painter.setBrush(Qt::NoBrush);
    
    QMutexLocker locker(&m_dataMutex);
    for (const VisualEarthquake &eq : m_earthquakes) {
        if (eq.isSelected && isInViewport(eq.screenPos)) {
            double size = getScaledSize(eq.displaySize) + 6;
            QRectF rect(eq.screenPos.x() - size/2, eq.screenPos.y() - size/2, size, size);
            painter.drawEllipse(rect);
        }
    }
}

void EarthquakeMapWidget::renderLegend(QPainter &painter) const
{
    QRect legendRect(width() - 220, 20, 200, 250);
    
    // Background
    painter.fillRect(legendRect, QColor(0, 0, 0, 180));
    painter.setPen(Qt::white);
    painter.drawRect(legendRect);
    
    // Title
    painter.setFont(QFont("Arial", 12, QFont::Bold));
    painter.drawText(legendRect.x() + 10, legendRect.y() + 20, "Earthquake Legend");
    
    // Magnitude scale
    painter.setFont(QFont("Arial", 9));
    int y = legendRect.y() + 45;
    
    for (double mag = 2.0; mag <= 8.0; mag += 1.0) {
        QColor color = getMagnitudeColor(mag);
        double size = DEFAULT_EARTHQUAKE_SIZE * (mag / 4.0);
        size = qBound(4.0, size, 25.0);
        
        painter.setBrush(color);
        painter.setPen(color.darker());
        painter.drawEllipse(legendRect.x() + 15, y - size/2, size, size);
        
        painter.setPen(Qt::white);
        painter.drawText(legendRect.x() + 40, y + 4, QString("M%1").arg(mag, 0, 'f', 1));
        y += 25;
    }
    
    // Color scheme info
    y += 10;
    painter.setFont(QFont("Arial", 9, QFont::Bold));
    painter.drawText(legendRect.x() + 10, y, "Color by:");
    y += 15;
    painter.setFont(QFont("Arial", 8));
    
    QString colorSchemeText;
    switch (m_settings.colorScheme) {
        case ColorScheme::Magnitude: colorSchemeText = "Magnitude"; break;
        case ColorScheme::Depth: colorSchemeText = "Depth"; break;
        case ColorScheme::Age: colorSchemeText = "Age"; break;
        case ColorScheme::AlertLevel: colorSchemeText = "Alert Level"; break;
        case ColorScheme::DataSource: colorSchemeText = "Data Source"; break;
        default: colorSchemeText = "Custom"; break;
    }
    painter.drawText(legendRect.x() + 10, y, colorSchemeText);
    
    // Statistics
    y += 25;
    painter.setFont(QFont("Arial", 8));
    QMutexLocker locker(&m_dataMutex);
    painter.drawText(legendRect.x() + 10, y, QString("Total: %1").arg(m_earthquakes.size()));
    y += 15;
    
    int visible = 0;
    for (const auto &eq : m_earthquakes) {
        if (eq.isVisible) visible++;
    }
    painter.drawText(legendRect.x() + 10, y, QString("Visible: %1").arg(visible));
}

void EarthquakeMapWidget::renderOverlay(QPainter &painter) const
{
    // Render scale bar
    if (m_zoomLevel > 0.1) {
        renderScaleBar(painter);
    }
    
    // Render coordinates display
    if (underMouse()) {
        QPoint mousePos = mapFromGlobal(QCursor::pos());
        if (rect().contains(mousePos)) {
            QPointF latLon = screenToLatLon(mousePos);
            QString coordsText = formatCoordinate(latLon.y(), true) + ", " + 
                               formatCoordinate(latLon.x(), false);
            
            painter.setPen(Qt::white);
            painter.setFont(QFont("Arial", 9));
            painter.fillRect(10, height() - 25, 200, 20, QColor(0, 0, 0, 128));
            painter.drawText(15, height() - 10, coordsText);
        }
    }
}

void EarthquakeMapWidget::renderScaleBar(QPainter &painter) const
{
    // Calculate scale bar length in pixels and km
    double kmPerPixel = (EARTH_RADIUS_KM * 2 * M_PI) / (360.0 * m_zoomLevel * width());
    
    // Find appropriate scale bar length
    QVector<double> scaleOptions = {1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000};
    double scaleKm = 100; // Default
    
    for (double option : scaleOptions) {
        double pixelLength = option / kmPerPixel;
        if (pixelLength >= 50 && pixelLength <= 150) {
            scaleKm = option;
            break;
        }
    }
    
    double scalePixels = scaleKm / kmPerPixel;
    
    // Draw scale bar
    QRect scaleRect(width() - 150, height() - 40, scalePixels, 20);
    
    painter.fillRect(scaleRect, QColor(255, 255, 255, 200));
    painter.setPen(Qt::black);
    painter.drawRect(scaleRect);
    
    // Scale text
    QString scaleText = scaleKm < 1000 ? 
                       QString("%1 km").arg(scaleKm) :
                       QString("%1k km").arg(scaleKm / 1000);
    
    painter.setFont(QFont("Arial", 8));
    painter.drawText(scaleRect.x(), scaleRect.y() - 5, scaleText);
}

void EarthquakeMapWidget::renderDebugInfo(QPainter &painter) const
{
    painter.setPen(Qt::yellow);
    painter.setFont(QFont("Courier", 9));
    
    QStringList debugInfo;
    debugInfo << QString("Center: %1, %2").arg(m_centerLatitude, 0, 'f', 4).arg(m_centerLongitude, 0, 'f', 4);
    debugInfo << QString("Zoom: %1").arg(m_zoomLevel, 0, 'f', 2);
    debugInfo << QString("Earthquakes: %1 total, %2 visible").arg(m_earthquakes.size()).arg(getVisibleEarthquakes().size());
    debugInfo << QString("Clusters: %1").arg(m_clusters.size());
    debugInfo << QString("Animation Frame: %1").arg(m_animationFrame);
    debugInfo << QString("Cache Valid: BG=%1, Layer=%2").arg(m_backgroundCacheValid).arg(m_layerCacheValid);
    
    int y = 10;
    for (const QString &line : debugInfo) {
        painter.fillRect(5, y - 2, painter.fontMetrics().horizontalAdvance(line) + 4, 
                        painter.fontMetrics().height() + 2, QColor(0, 0, 0, 128));
        painter.drawText(7, y + painter.fontMetrics().ascent(), line);
        y += painter.fontMetrics().height() + 2;
    }
}

// Mouse event handling
void EarthquakeMapWidget::mousePressEvent(QMouseEvent *event)
{
    setFocus();
    m_lastMousePos = event->pos();
    
    if (event->button() == Qt::LeftButton) {
        if (event->modifiers() & Qt::ControlModifier) {
            // Start selection
            m_isSelecting = true;
            m_selectionStart = event->pos();
            
            if (!m_selectionBand) {
                m_selectionBand = new QRubberBand(QRubberBand::Rectangle, this);
            }
            m_selectionBand->setGeometry(QRect(m_selectionStart, QSize()));
            m_selectionBand->show();
        } else {
            // Check for earthquake click
            int earthquakeIndex = findEarthquakeAt(event->pos());
            
            if (earthquakeIndex >= 0) {
                const EarthquakeData &earthquake = m_earthquakes[earthquakeIndex].data;
                
                if (event->modifiers() & Qt::ShiftModifier) {
                    // Add to selection
                    if (!m_selectedIds.contains(earthquake.eventId)) {
                        m_selectedIds.append(earthquake.eventId);
                        m_earthquakes[earthquakeIndex].isSelected = true;
                    }
                } else {
                    // Single selection
                    clearSelection();
                    m_selectedIds.append(earthquake.eventId);
                    m_earthquakes[earthquakeIndex].isSelected = true;
                }
                
                emit earthquakeClicked(earthquake);
                emit selectionChanged(getSelectedEarthquakes());
                update();
            } else {
                // Start panning
                m_isPanning = true;
                m_panStartPos = event->pos();
                m_panStartCenter = QPointF(m_centerLongitude, m_centerLatitude);
                setCursor(Qt::ClosedHandCursor);
                
                // Clear selection if no modifiers
                if (event->modifiers() == Qt::NoModifier) {
                    clearSelection();
                }
                
                // Emit map click
                QPointF latLon = screenToLatLon(event->pos());
                emit mapClicked(latLon.y(), latLon.x());
            }
        }
    }
    
    QWidget::mousePressEvent(event);
}

void EarthquakeMapWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isPanning && (event->buttons() & Qt::LeftButton)) {
        // Pan the map
        QPoint delta = event->pos() - m_panStartPos;
        
        // Convert pixel movement to coordinate movement
        double latDelta = -delta.y() * (180.0 / height()) / m_zoomLevel;
        double lonDelta = -delta.x() * (360.0 / width()) / m_zoomLevel;
        
        double newLat = m_panStartCenter.y() + latDelta;
        double newLon = m_panStartCenter.x() + lonDelta;
        
        setCenter(newLat, newLon);
        
    } else if (m_isSelecting && (event->buttons() & Qt::LeftButton)) {
        // Update selection rectangle
        if (m_selectionBand) {
            QRect selectionRect = QRect(m_selectionStart, event->pos()).normalized();
            m_selectionBand->setGeometry(selectionRect);
        }
        
    } else {
        // Handle hover
        int earthquakeIndex = findEarthquakeAt(event->pos());
        
        QString newHoveredId;
        if (earthquakeIndex >= 0) {
            newHoveredId = m_earthquakes[earthquakeIndex].data.eventId;
            setCursor(Qt::PointingHandCursor);
            
            // Show tooltip
            if (m_settings.showTooltips) {
                QString tooltip = formatEarthquakeTooltip(m_earthquakes[earthquakeIndex].data);
                showTooltip(event->pos(), tooltip);
            }
            
            // Emit hover event
            if (newHoveredId != m_hoveredEarthquakeId) {
                emit earthquakeHovered(m_earthquakes[earthquakeIndex].data);
            }
        } else {
            setCursor(Qt::ArrowCursor);
            hideTooltip();
        }
        
        // Update highlighting
        if (newHoveredId != m_hoveredEarthquakeId) {
            // Clear old highlight
            if (!m_hoveredEarthquakeId.isEmpty()) {
                for (auto &eq : m_earthquakes) {
                    if (eq.data.eventId == m_hoveredEarthquakeId) {
                        eq.isHighlighted = false;
                        break;
                    }
                }
            }
            
            // Set new highlight
            if (!newHoveredId.isEmpty()) {
                for (auto &eq : m_earthquakes) {
                    if (eq.data.eventId == newHoveredId) {
                        eq.isHighlighted = true;
                        break;
                    }
                }
            }
            
            m_hoveredEarthquakeId = newHoveredId;
            update();
        }
    }
    
    m_lastMousePos = event->pos();
    QWidget::mouseMoveEvent(event);
}

void EarthquakeMapWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_isPanning) {
            m_isPanning = false;
            setCursor(Qt::ArrowCursor);
            
        } else if (m_isSelecting) {
            // Complete selection
            m_isSelecting = false;
            
            if (m_selectionBand) {
                QRect selectionRect = m_selectionBand->geometry();
                m_selectionBand->hide();
                
                // Find earthquakes in selection
                QVector<int> selectedIndices = findEarthquakesInRect(selectionRect);
                
                if (!(event->modifiers() & Qt::ShiftModifier)) {
                    clearSelection();
                }
                
                for (int index : selectedIndices) {
                    QString eventId = m_earthquakes[index].data.eventId;
                    if (!m_selectedIds.contains(eventId)) {
                        m_selectedIds.append(eventId);
                        m_earthquakes[index].isSelected = true;
                    }
                }
                
                if (!selectedIndices.isEmpty()) {
                    emit selectionChanged(getSelectedEarthquakes());
                    update();
                }
            }
        }
    }
    
    QWidget::mouseReleaseEvent(event);
}

void EarthquakeMapWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        int earthquakeIndex = findEarthquakeAt(event->pos());
        
        if (earthquakeIndex >= 0) {
            // Double-click on earthquake
            const EarthquakeData &earthquake = m_earthquakes[earthquakeIndex].data;
            emit earthquakeDoubleClicked(earthquake);
            
            // Zoom in and center on earthquake
            animateToLocation(earthquake.latitude, earthquake.longitude, m_zoomLevel * 2.0);
        } else {
            // Double-click on empty space - zoom in
            QPointF latLon = screenToLatLon(event->pos());
            animateToLocation(latLon.y(), latLon.x(), m_zoomLevel * 2.0);
        }
    }
    
    QWidget::mouseDoubleClickEvent(event);
}

void EarthquakeMapWidget::wheelEvent(QWheelEvent *event)
{
    // Zoom towards mouse position
    QPointF mouseLatLon = screenToLatLon(event->position().toPoint());
    
    double zoomFactor = event->angleDelta().y() > 0 ? ZOOM_FACTOR : 1.0 / ZOOM_FACTOR;
    double newZoom = qBound(MIN_ZOOM, m_zoomLevel * zoomFactor, MAX_ZOOM);
    
    if (qAbs(newZoom - m_zoomLevel) > 1e-6) {
        // Calculate new center to keep mouse position fixed
        QPointF currentCenter(m_centerLongitude, m_centerLatitude);
        QPointF offset = mouseLatLon - currentCenter;
        offset *= (1.0 - 1.0 / zoomFactor);
        QPointF newCenter = currentCenter + offset;
        
        setCenter(newCenter.y(), newCenter.x());
        setZoomLevel(newZoom);
    }
    
    QWidget::wheelEvent(event);
}

void EarthquakeMapWidget::keyPressEvent(QKeyEvent *event)
{
    const double PAN_STEP = 10.0 / m_zoomLevel;
    
    switch (event->key()) {
        case Qt::Key_Up:
            setCenter(m_centerLatitude + PAN_STEP, m_centerLongitude);
            break;
        case Qt::Key_Down:
            setCenter(m_centerLatitude - PAN_STEP, m_centerLongitude);
            break;
        case Qt::Key_Left:
            setCenter(m_centerLatitude, m_centerLongitude - PAN_STEP);
            break;
        case Qt::Key_Right:
            setCenter(m_centerLatitude, m_centerLongitude + PAN_STEP);
            break;
        case Qt::Key_Plus:
        case Qt::Key_Equal:
            zoomIn();
            break;
        case Qt::Key_Minus:
            zoomOut();
            break;
        case Qt::Key_Home:
            setCenter(0, 0);
            setZoomLevel(1.0);
            break;
        case Qt::Key_F:
            fitToEarthquakes();
            break;
        case Qt::Key_A:
            if (event->modifiers() & Qt::ControlModifier) {
                // Select all visible earthquakes
                QStringList visibleIds;
                for (const auto &eq : m_earthquakes) {
                    if (eq.isVisible) {
                        visibleIds.append(eq.data.eventId);
                    }
                }
                selectEarthquakes(visibleIds);
            }
            break;
        case Qt::Key_Escape:
            clearSelection();
            break;
        default:
            QWidget::keyPressEvent(event);
            return;
    }
    
    event->accept();
}

void EarthquakeMapWidget::resizeEvent(QResizeEvent *event)
{
    updateVisibleBounds();
    updateVisibleEarthquakes();
    
    m_backgroundCacheValid = false;
    m_layerCacheValid = false;
    
    QWidget::resizeEvent(event);
}

void EarthquakeMapWidget::contextMenuEvent(QContextMenuEvent *event)
{
    int earthquakeIndex = findEarthquakeAt(event->pos());
    
    if (earthquakeIndex >= 0) {
        const EarthquakeData &earthquake = m_earthquakes[earthquakeIndex].data;
        emit contextMenuRequested(event->pos(), earthquake);
    }
    
    if (m_contextMenu) {
        m_contextMenu->exec(event->globalPos());
    }
    
    QWidget::contextMenuEvent(event);
}

void EarthquakeMapWidget::leaveEvent(QEvent *event)
{
    // Clear hover state
    if (!m_hoveredEarthquakeId.isEmpty()) {
        for (auto &eq : m_earthquakes) {
            if (eq.data.eventId == m_hoveredEarthquakeId) {
                eq.isHighlighted = false;
                break;
            }
        }
        m_hoveredEarthquakeId.clear();
        update();
    }
    
    hideTooltip();
    setCursor(Qt::ArrowCursor);
    
    QWidget::leaveEvent(event);
}

// Coordinate transformation methods
QPointF EarthquakeMapWidget::latLonToScreen(double latitude, double longitude) const
{
    QPointF projected = projectCoordinate(latitude, longitude);
    
    // Convert to screen coordinates
    double x = (projected.x() - m_centerLongitude) * m_zoomLevel * width() / 360.0 + width() / 2.0;
    double y = (m_centerLatitude - projected.y()) * m_zoomLevel * height() / 180.0 + height() / 2.0;
    
    return QPointF(x, y);
}

QPointF EarthquakeMapWidget::screenToLatLon(const QPointF &screen) const
{
    double longitude = (screen.x() - width() / 2.0) * 360.0 / (m_zoomLevel * width()) + m_centerLongitude;
    double latitude = m_centerLatitude - (screen.y() - height() / 2.0) * 180.0 / (m_zoomLevel * height());
    
    latitude = qBound(-90.0, latitude, 90.0);
    longitude = SpatialUtils::normalizeLongitude(longitude);
    
    return QPointF(longitude, latitude);
}

QPointF EarthquakeMapWidget::projectCoordinate(double latitude, double longitude) const
{
    switch (m_settings.projection) {
        case MapProjection::Mercator:
            return mercatorProjection(latitude, longitude);
        case MapProjection::Equirectangular:
            return equirectangularProjection(latitude, longitude);
        case MapProjection::OrthographicNorthPole:
            return orthographicProjection(latitude, longitude, true);
        case MapProjection::OrthographicSouthPole:
            return orthographicProjection(latitude, longitude, false);
        case MapProjection::Robinson:
            return robinsonProjection(latitude, longitude);
        default:
            return mercatorProjection(latitude, longitude);
    }
}

QPointF EarthquakeMapWidget::mercatorProjection(double lat, double lon) const
{
    double x = lon;
    double y = lat;
    
    // Apply Mercator transformation for y-coordinate
    if (qAbs(lat) < 85.0) { // Avoid singularity at poles
        double latRad = lat * M_PI / 180.0;
        y = log(tan(M_PI/4 + latRad/2)) * 180.0 / M_PI;
    } else {
        y = lat > 0 ? 85.0 : -85.0;
    }
    
    return QPointF(x, y);
}

QPointF EarthquakeMapWidget::equirectangularProjection(double lat, double lon) const
{
    return QPointF(lon, lat);
}

QPointF EarthquakeMapWidget::orthographicProjection(double lat, double lon, bool northPole) const
{
    double latRad = lat * M_PI / 180.0;
    double lonRad = lon * M_PI / 180.0;
    
    double centerLat = northPole ? 90.0 : -90.0;
    double centerLatRad = centerLat * M_PI / 180.0;
    
    double x = cos(latRad) * sin(lonRad);
    double y = cos(centerLatRad) * sin(latRad) - sin(centerLatRad) * cos(latRad) * cos(lonRad);
    
    return QPointF(x * 180.0, y * 180.0);
}

QPointF EarthquakeMapWidget::robinsonProjection(double lat, double lon) const
{
    // Simplified Robinson projection
    double latRad = lat * M_PI / 180.0;
    
    double x = lon * cos(latRad * 0.6);
    double y = lat * 1.3;
    
    return QPointF(x, y);
}

// Color and styling methods
QColor EarthquakeMapWidget::getEarthquakeColor(const EarthquakeData &earthquake) const
{
    switch (m_settings.colorScheme) {
        case ColorScheme::Magnitude:
            return getMagnitudeColor(earthquake.magnitude);
        case ColorScheme::Depth:
            return getDepthColor(earthquake.depth);
        case ColorScheme::Age:
            return getAgeColor(earthquake.timestamp);
        case ColorScheme::AlertLevel:
            return getAlertLevelColor(earthquake.alertLevel);
        case ColorScheme::DataSource:
            // Simple hash-based coloring by data source
            return QColor::fromHsv((qHash(earthquake.dataSource) % 360), 200, 200);
        default:
            return getMagnitudeColor(earthquake.magnitude);
    }
}

QColor EarthquakeMapWidget::getMagnitudeColor(double magnitude) const
{
    if (magnitude < 1.0) return QColor(200, 255, 200);      // Very light green
    else if (magnitude < 2.0) return QColor(150, 255, 150); // Light green
    else if (magnitude < 3.0) return QColor(100, 255, 100); // Green
    else if (magnitude < 4.0) return QColor(255, 255, 100); // Yellow
    else if (magnitude < 5.0) return QColor(255, 200, 100); // Orange
    else if (magnitude < 6.0) return QColor(255, 150, 100); // Light red
    else if (magnitude < 7.0) return QColor(255, 100, 100); // Red
    else if (magnitude < 8.0) return QColor(200, 50, 50);   // Dark red
    else return QColor(150, 0, 150);                        // Purple
}

QColor EarthquakeMapWidget::getDepthColor(double depth) const
{
    // Shallow = red, deep = blue
    double normalizedDepth = qBound(0.0, depth / 700.0, 1.0); // 700km max depth
    
    int red = int(255 * (1.0 - normalizedDepth));
    int blue = int(255 * normalizedDepth);
    int green = int(128 * (1.0 - qAbs(normalizedDepth - 0.5) * 2.0));
    
    return QColor(red, green, blue);
}

QColor EarthquakeMapWidget::getAgeColor(const QDateTime &timestamp) const
{
    qint64 ageSeconds = timestamp.secsTo(QDateTime::currentDateTimeUtc());
    double ageHours = ageSeconds / 3600.0;
    
    if (ageHours < 1.0) return QColor(255, 50, 50);        // Red - very recent
    else if (ageHours < 6.0) return QColor(255, 150, 50);  // Orange
    else if (ageHours < 24.0) return QColor(255, 255, 50); // Yellow
    else if (ageHours < 168.0) return QColor(150, 255, 50); // Green
    else return QColor(100, 100, 200);                     // Blue - old
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

double EarthquakeMapWidget::getEarthquakeSize(const EarthquakeData &earthquake) const
{
    double baseSize = DEFAULT_EARTHQUAKE_SIZE;
    
    // Size based on magnitude (logarithmic scale)
    double size = baseSize * pow(2.0, (earthquake.magnitude - 3.0) / 2.0);
    
    // Minimum and maximum size limits
    size = qBound(3.0, size, 50.0);
    
    return size;
}

double EarthquakeMapWidget::getScaledSize(double baseSize) const
{
    // Scale size based on zoom level
    double scaleFactor = qBound(0.5, sqrt(m_zoomLevel), 3.0);
    return baseSize * scaleFactor;
}

// Animation methods
void EarthquakeMapWidget::updateAnimation()
{
    if (!m_animationEnabled) {
        return;
    }
    
    m_animationFrame = (m_animationFrame + 1) % (ANIMATION_FPS * 6); // 6 second cycle
    
    // Update earthquake animations
    updateEarthquakeAnimations();
    
    emit animationFrameUpdated(m_animationFrame);
    update();
}

void EarthquakeMapWidget::updateEarthquakeAnimations()
{
    QMutexLocker locker(&m_dataMutex);
    
    for (auto &eq : m_earthquakes) {
        eq.animationPhase = calculateAnimationPhase(eq.data);
    }
}

double EarthquakeMapWidget::calculateAnimationPhase(const EarthquakeData &earthquake) const
{
    // Base animation on time since earthquake occurred
    qint64 ageSeconds = earthquake.timestamp.secsTo(QDateTime::currentDateTimeUtc());
    double ageHours = ageSeconds / 3600.0;
    
    // Recent earthquakes animate more
    if (ageHours < 1.0) {
        return sin(m_animationFrame * 0.3) * 0.5 + 0.5; // Fast pulse
    } else if (ageHours < 24.0) {
        return sin(m_animationFrame * 0.1) * 0.3 + 0.7; // Slow pulse
    }
    
    return 1.0; // No animation for old earthquakes
}

double EarthquakeMapWidget::getAnimationValue(AnimationStyle style, double phase) const
{
    switch (style) {
        case AnimationStyle::Pulse:
            return 0.8 + 0.2 * sin(phase * 2 * M_PI);
        case AnimationStyle::Ripple:
            return 1.0 + 0.5 * sin(phase * 4 * M_PI) * exp(-phase * 3);
        case AnimationStyle::Fade:
            return qBound(0.3, 1.0 - phase * 0.7, 1.0);
        case AnimationStyle::Grow:
            return qBound(0.5, 0.5 + phase, 1.5);
        case AnimationStyle::Shake:
            return 1.0 + 0.1 * sin(phase * 8 * M_PI);
        default:
            return 1.0;
    }
}

// Utility and helper methods
void EarthquakeMapWidget::updateVisibleBounds()
{
    QPointF topLeft = screenToLatLon(QPointF(0, 0));
    QPointF bottomRight = screenToLatLon(QPointF(width(), height()));
    
    m_visibleBounds.minLatitude = bottomRight.y();
    m_visibleBounds.maxLatitude = topLeft.y();
    m_visibleBounds.minLongitude = topLeft.x();
    m_visibleBounds.maxLongitude = bottomRight.x();
    
    // Handle longitude wraparound
    if (m_visibleBounds.minLongitude > m_visibleBounds.maxLongitude) {
        m_visibleBounds.maxLongitude += 360.0;
    }
}

void EarthquakeMapWidget::updateVisibleEarthquakes()
{
    QMutexLocker locker(&m_dataMutex);
    
    for (auto &eq : m_earthquakes) {
        eq.screenPos = latLonToScreen(eq.data.latitude, eq.data.longitude);
        eq.isVisible = isEarthquakeVisible(eq.data) && passesFilters(eq.data);
        eq.displaySize = getEarthquakeSize(eq.data);
        eq.displayColor = getEarthquakeColor(eq.data);
    }
}

QString EarthquakeMapWidget::formatEarthquakeTooltip(const EarthquakeData &earthquake) const
{
    QString tooltip = QString("<b>M%1 Earthquake</b><br>").arg(earthquake.magnitude, 0, 'f', 1);
    tooltip += QString("<b>Location:</b> %1<br>").arg(earthquake.location);
    tooltip += QString("<b>Depth:</b> %1 km<br>").arg(earthquake.depth, 0, 'f', 1);
    tooltip += QString("<b>Time:</b> %1 UTC<br>").arg(earthquake.timestamp.toString("yyyy-MM-dd hh:mm:ss"));
    tooltip += QString("<b>Event ID:</b> %1<br>").arg(earthquake.eventId);
    
    if (!earthquake.dataSource.isEmpty()) {
        tooltip += QString("<b>Source:</b> %1<br>").arg(earthquake.dataSource);
    }
    
    if (earthquake.tsunamiFlag == "Yes") {
        tooltip += "<b><font color='red'>⚠️ TSUNAMI POSSIBLE</font></b>";
    }
    
    return tooltip;
}

void EarthquakeMapWidget::saveSettings() const
{
    QSettings settings("EarthquakeAlertSystem", "MapWidget");
    
    settings.setValue("centerLatitude", m_centerLatitude);
    settings.setValue("centerLongitude", m_centerLongitude);
    settings.setValue("zoomLevel", m_zoomLevel);
    settings.setValue("projection", static_cast<int>(m_settings.projection));
    settings.setValue("displayMode", static_cast<int>(m_settings.displayMode));
    settings.setValue("colorScheme", static_cast<int>(m_settings.colorScheme));
    settings.setValue("showGrid", m_settings.showGrid);
    settings.setValue("showLegend", m_settings.showLegend);
    settings.setValue("enableClustering", m_settings.enableClustering);
    settings.setValue("animationEnabled", m_animationEnabled);
}

void EarthquakeMapWidget::loadSettings()
{
    QSettings settings("EarthquakeAlertSystem", "MapWidget");
    
    m_centerLatitude = settings.value("centerLatitude", 0.0).toDouble();
    m_centerLongitude = settings.value("centerLongitude", 0.0).toDouble();
    m_zoomLevel = settings.value("zoomLevel", 1.0).toDouble();
    
    m_settings.projection = static_cast<MapProjection>(
        settings.value("projection", static_cast<int>(MapProjection::Mercator)).toInt());
    m_settings.displayMode = static_cast<EarthquakeDisplayMode>(
        settings.value("displayMode", static_cast<int>(EarthquakeDisplayMode::Circles)).toInt());
    m_settings.colorScheme = static_cast<ColorScheme>(
        settings.value("colorScheme", static_cast<int>(ColorScheme::Magnitude)).toInt());
    m_settings.showGrid = settings.value("showGrid", true).toBool();
    m_settings.showLegend = settings.value("showLegend", true).toBool();
    m_settings.enableClustering = settings.value("enableClustering", true).toBool();
    m_animationEnabled = settings.value("animationEnabled", true).toBool();
    
    updateVisibleBounds();
}

// Additional utility methods implementation
bool EarthquakeMapWidget::isEarthquakeVisible(const EarthquakeData &earthquake) const
{
    return m_visibleBounds.contains(earthquake.latitude, earthquake.longitude);
}

bool EarthquakeMapWidget::passesFilters(const EarthquakeData &earthquake) const
{
    // Magnitude filter
    if (earthquake.magnitude < m_minMagnitude || earthquake.magnitude > m_maxMagnitude) {
        return false;
    }
    
    // Depth filter
    if (earthquake.depth < m_minDepth || earthquake.depth > m_maxDepth) {
        return false;
    }
    
    // Time filter
    if (m_startTime.isValid() && earthquake.timestamp < m_startTime) {
        return false;
    }
    if (m_endTime.isValid() && earthquake.timestamp > m_endTime) {
        return false;
    }
    
    // Location filter
    if (m_hasLocationFilter && !m_locationFilter.contains(earthquake.latitude, earthquake.longitude)) {
        return false;
    }
    
    return true;
}

bool EarthquakeMapWidget::isInViewport(const QPointF &screenPos) const
{
    return rect().contains(screenPos.toPoint());
}

int EarthquakeMapWidget::findEarthquakeAt(const QPoint &point) const
{
    QMutexLocker locker(&m_dataMutex);
    
    double minDistance = std::numeric_limits<double>::max();
    int closestIndex = -1;
    
    for (int i = 0; i < m_earthquakes.size(); ++i) {
        if (!m_earthquakes[i].isVisible) continue;
        
        double distance = distanceToEarthquake(point, i);
        double threshold = getScaledSize(m_earthquakes[i].displaySize) / 2.0 + 5.0; // 5px tolerance
        
        if (distance <= threshold && distance < minDistance) {
            minDistance = distance;
            closestIndex = i;
        }
    }
    
    return closestIndex;
}

QVector<int> EarthquakeMapWidget::findEarthquakesInRect(const QRect &rect) const
{
    QMutexLocker locker(&m_dataMutex);
    QVector<int> indices;
    
    for (int i = 0; i < m_earthquakes.size(); ++i) {
        if (m_earthquakes[i].isVisible && rect.contains(m_earthquakes[i].screenPos.toPoint())) {
            indices.append(i);
        }
    }
    
    return indices;
}

double EarthquakeMapWidget::distanceToEarthquake(const QPoint &point, int earthquakeIndex) const
{
    if (earthquakeIndex < 0 || earthquakeIndex >= m_earthquakes.size()) {
        return std::numeric_limits<double>::max();
    }
    
    const QPointF &eqPos = m_earthquakes[earthquakeIndex].screenPos;
    double dx = point.x() - eqPos.x();
    double dy = point.y() - eqPos.y();
    return sqrt(dx * dx + dy * dy);
}

bool EarthquakeMapWidget::shouldSkipRendering(const VisualEarthquake &eq) const
{
    // Skip if too far outside viewport
    QPointF pos = eq.screenPos;
    QRect extended = rect().adjusted(-100, -100, 100, 100);
    if (!extended.contains(pos.toPoint())) {
        return true;
    }
    
    // Skip very small earthquakes at low zoom levels
    if (m_zoomLevel < m_lodThreshold && eq.data.magnitude < 3.0) {
        return true;
    }
    
    return false;
}

void EarthquakeMapWidget::updateClusters()
{
    if (!m_settings.enableClustering) {
        clearClusters();
        return;
    }
    
    QMutexLocker locker(&m_dataMutex);
    clearClusters();
    
    QVector<bool> clustered(m_earthquakes.size(), false);
    
    for (int i = 0; i < m_earthquakes.size(); ++i) {
        if (clustered[i] || !m_earthquakes[i].isVisible) continue;
        
        QVector<int> clusterIndices;
        clusterIndices.append(i);
        clustered[i] = true;
        
        // Find nearby earthquakes
        for (int j = i + 1; j < m_earthquakes.size(); ++j) {
            if (clustered[j] || !m_earthquakes[j].isVisible) continue;
            
            if (shouldCluster(m_earthquakes[i], m_earthquakes[j])) {
                clusterIndices.append(j);
                clustered[j] = true;
            }
        }
        
        // Create cluster if multiple earthquakes
        if (clusterIndices.size() > 1) {
            EarthquakeCluster cluster = createCluster(clusterIndices);
            m_clusters.append(cluster);
            
            // Update earthquake cluster IDs
            for (int idx : clusterIndices) {
                m_earthquakes[idx].clusterId = m_clusters.size() - 1;
            }
        }
    }
    
    qDebug() << "Updated clusters:" << m_clusters.size() << "clusters created";
}

void EarthquakeMapWidget::clearClusters()
{
    m_clusters.clear();
    
    QMutexLocker locker(&m_dataMutex);
    for (auto &eq : m_earthquakes) {
        eq.clusterId = -1;
        eq.isClusterCenter = false;
    }
}

bool EarthquakeMapWidget::shouldCluster(const VisualEarthquake &eq1, const VisualEarthquake &eq2) const
{
    double distance = sqrt(pow(eq1.screenPos.x() - eq2.screenPos.x(), 2) + 
                          pow(eq1.screenPos.y() - eq2.screenPos.y(), 2));
    return distance <= m_settings.clusterDistance;
}

EarthquakeCluster EarthquakeMapWidget::createCluster(const QVector<int> &earthquakeIds)
{
    EarthquakeCluster cluster;
    cluster.earthquakeIds = earthquakeIds;
    
    // Calculate center position
    double totalX = 0, totalY = 0;
    double totalMag = 0, maxMag = 0;
    QDateTime latestTime;
    
    for (int id : earthquakeIds) {
        const VisualEarthquake &eq = m_earthquakes[id];
        totalX += eq.screenPos.x();
        totalY += eq.screenPos.y();
        totalMag += eq.data.magnitude;
        maxMag = qMax(maxMag, eq.data.magnitude);
        
        if (!latestTime.isValid() || eq.data.timestamp > latestTime) {
            latestTime = eq.data.timestamp;
        }
    }
    
    cluster.centerPos = QPointF(totalX / earthquakeIds.size(), totalY / earthquakeIds.size());
    cluster.avgMagnitude = totalMag / earthquakeIds.size();
    cluster.maxMagnitude = maxMag;
    cluster.latestTime = latestTime;
    cluster.displayColor = getMagnitudeColor(cluster.avgMagnitude);
    cluster.displaySize = qBound(15.0, 10.0 + earthquakeIds.size() * 2.0, 50.0);
    cluster.isExpanded = false;
    
    return cluster;
}

MapBounds EarthquakeMapWidget::calculateBounds(const QVector<EarthquakeData> &earthquakes) const
{
    if (earthquakes.isEmpty()) {
        return MapBounds{-90, 90, -180, 180};
    }
    
    MapBounds bounds;
    bounds.minLatitude = earthquakes.first().latitude;
    bounds.maxLatitude = earthquakes.first().latitude;
    bounds.minLongitude = earthquakes.first().longitude;
    bounds.maxLongitude = earthquakes.first().longitude;
    
    for (const auto &eq : earthquakes) {
        bounds.minLatitude = qMin(bounds.minLatitude, eq.latitude);
        bounds.maxLatitude = qMax(bounds.maxLatitude, eq.latitude);
        bounds.minLongitude = qMin(bounds.minLongitude, eq.longitude);
        bounds.maxLongitude = qMax(bounds.maxLongitude, eq.longitude);
    }
    
    // Add padding
    double latPadding = (bounds.maxLatitude - bounds.minLatitude) * 0.1;
    double lonPadding = (bounds.maxLongitude - bounds.minLongitude) * 0.1;
    
    bounds.minLatitude -= latPadding;
    bounds.maxLatitude += latPadding;
    bounds.minLongitude -= lonPadding;
    bounds.maxLongitude += lonPadding;
    
    return bounds;
}

double EarthquakeMapWidget::calculateOptimalZoom(const MapBounds &bounds) const
{
    if (!bounds.isValid()) {
        return 1.0;
    }
    
    double latZoom = 180.0 / bounds.height();
    double lonZoom = 360.0 / bounds.width();
    
    double zoom = qMin(latZoom, lonZoom) * 0.8; // 80% to leave some margin
    return qBound(MIN_ZOOM, zoom, MAX_ZOOM);
}

QString EarthquakeMapWidget::formatCoordinate(double value, bool isLatitude) const
{
    QString suffix = isLatitude ? (value >= 0 ? "N" : "S") : (value >= 0 ? "E" : "W");
    return QString("%1°%2").arg(qAbs(value), 0, 'f', 4).arg(suffix);
}

void EarthquakeMapWidget::showTooltip(const QPoint &pos, const QString &text)
{
    QToolTip::showText(mapToGlobal(pos), text, this, QRect(), 5000);
}

void EarthquakeMapWidget::hideTooltip()
{
    QToolTip::hideText();
}

// Selection and filtering methods
QVector<EarthquakeData> EarthquakeMapWidget::getSelectedEarthquakes() const
{
    QMutexLocker locker(&m_dataMutex);
    QVector<EarthquakeData> selected;
    
    for (const auto &eq : m_earthquakes) {
        if (eq.isSelected) {
            selected.append(eq.data);
        }
    }
    
    return selected;
}

void EarthquakeMapWidget::selectEarthquake(const QString &eventId)
{
    clearSelection();
    selectEarthquakes({eventId});
}

void EarthquakeMapWidget::selectEarthquakes(const QStringList &eventIds)
{
    QMutexLocker locker(&m_dataMutex);
    
    for (const QString &id : eventIds) {
        if (!m_selectedIds.contains(id)) {
            m_selectedIds.append(id);
        }
        
        for (auto &eq : m_earthquakes) {
            if (eq.data.eventId == id) {
                eq.isSelected = true;
                break;
            }
        }
    }
    
    emit selectionChanged(getSelectedEarthquakes());
    update();
}

void EarthquakeMapWidget::clearSelection()
{
    if (m_selectedIds.isEmpty()) return;
    
    QMutexLocker locker(&m_dataMutex);
    
    for (auto &eq : m_earthquakes) {
        eq.isSelected = false;
    }
    
    m_selectedIds.clear();
    
    emit selectionChanged(QVector<EarthquakeData>());
    update();
}

void EarthquakeMapWidget::setMagnitudeFilter(double minMag, double maxMag)
{
    m_minMagnitude = minMag;
    m_maxMagnitude = maxMag;
    updateVisibleEarthquakes();
    update();
}

void EarthquakeMapWidget::setDepthFilter(double minDepth, double maxDepth)
{
    m_minDepth = minDepth;
    m_maxDepth = maxDepth;
    updateVisibleEarthquakes();
    update();
}

void EarthquakeMapWidget::setTimeFilter(const QDateTime &startTime, const QDateTime &endTime)
{
    m_startTime = startTime;
    m_endTime = endTime;
    updateVisibleEarthquakes();
    update();
}

// Export methods
QPixmap EarthquakeMapWidget::renderToPixmap(const QSize &size) const
{
    QSize renderSize = size.isValid() ? size : this->size();
    QPixmap pixmap(renderSize);
    pixmap.fill(m_settings.backgroundColor);
    
    QPainter painter(&pixmap);
    if (m_highQualityRendering) {
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
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
    
    return pixmap;
}

void EarthquakeMapWidget::exportToImage(const QString &fileName, const QSize &size) const
{
    QPixmap pixmap = renderToPixmap(size);
    
    QString actualFileName = fileName;
    if (actualFileName.isEmpty()) {
        actualFileName = QString("earthquake_map_%1.png")
                        .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss"));
    }
    
    if (pixmap.save(actualFileName)) {
        qDebug() << "Map exported to:" << actualFileName;
    } else {
        qWarning() << "Failed to export map to:" << actualFileName;
    }
}

// Getter methods
MapBounds EarthquakeMapWidget::getVisibleBounds() const
{
    return m_visibleBounds;
}

QVector<EarthquakeData> EarthquakeMapWidget::getAllEarthquakes() const
{
    QMutexLocker locker(&m_dataMutex);
    QVector<EarthquakeData> all;
    
    for (const auto &eq : m_earthquakes) {
        all.append(eq.data);
    }
    
    return all;
}

QVector<EarthquakeData> EarthquakeMapWidget::getVisibleEarthquakes() const
{
    QMutexLocker locker(&m_dataMutex);
    QVector<EarthquakeData> visible;
    
    for (const auto &eq : m_earthquakes) {
        if (eq.isVisible) {
            visible.append(eq.data);
        }
    }
    
    return visible;
}

int EarthquakeMapWidget::getEarthquakeCount() const
{
    QMutexLocker locker(&m_dataMutex);
    return m_earthquakes.size();
}

// Network slots
void EarthquakeMapWidget::onNetworkReplyFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        // Process downloaded map data
        QPixmap mapImage;
        if (mapImage.loadFromData(data)) {
            setBackgroundMap(mapImage);
            emit backgroundMapLoaded();
        }
    }
    
    reply->deleteLater();
}

void EarthquakeMapWidget::onAnimationFinished()
{
    // Animation finished - could trigger follow-up actions
}

void EarthquakeMapWidget::onSelectionAnimationFinished()
{
    // Selection animation finished
}

// Settings methods
void EarthquakeMapWidget::setMapSettings(const MapSettings &settings)
{
    m_settings = settings;
    
    m_backgroundCacheValid = false;
    m_layerCacheValid = false;
    
    updateVisibleEarthquakes();
    if (m_settings.enableClustering) {
        updateClusters();
    } else {
        clearClusters();
    }
    
    update();
}

MapSettings EarthquakeMapWidget::getMapSettings() const
{
    return m_settings;
}

void EarthquakeMapWidget::setProjection(MapProjection projection)
{
    m_settings.projection = projection;
    m_backgroundCacheValid = false;
    updateVisibleEarthquakes();
    update();
}

void EarthquakeMapWidget::setDisplayMode(EarthquakeDisplayMode mode)
{
    m_settings.displayMode = mode;
    update();
}

void EarthquakeMapWidget::setColorScheme(ColorScheme scheme)
{
    m_settings.colorScheme = scheme;
    updateVisibleEarthquakes(); // Recalculate colors
    update();
}

void EarthquakeMapWidget::setAnimationStyle(AnimationStyle style)
{
    m_settings.animationStyle = style;
    update();
}

// Animation control
void EarthquakeMapWidget::startAnimation()
{
    m_animationEnabled = true;
    if (m_animationTimer && !m_animationTimer->isActive()) {
        m_animationTimer->start(1000 / ANIMATION_FPS);
    }
}

void EarthquakeMapWidget::stopAnimation()
{
    m_animationEnabled = false;
    if (m_animationTimer) {
        m_animationTimer->stop();
    }
}

void EarthquakeMapWidget::setAnimationSpeed(double speed)
{
    m_settings.animationSpeed = qBound(0.1, speed, 5.0);
    if (m_animationTimer) {
        int interval = int(1000.0 / (ANIMATION_FPS * m_settings.animationSpeed));
        m_animationTimer->setInterval(interval);
    }
}

void EarthquakeMapWidget::highlightEarthquake(const QString &eventId, int durationMs)
{
    QMutexLocker locker(&m_dataMutex);
    
    for (auto &eq : m_earthquakes) {
        if (eq.data.eventId == eventId) {
            eq.isHighlighted = true;
            break;
        }
    }
    
    update();
    
    if (durationMs > 0) {
        QTimer::singleShot(durationMs, [this, eventId]() {
            QMutexLocker locker(&m_dataMutex);
            for (auto &eq : m_earthquakes) {
                if (eq.data.eventId == eventId) {
                    eq.isHighlighted = false;
                    break;
                }
            }
            update();
        });
    }
}

//////////////////////////////////////////////////////////////////////////

        QPainter cachePainter(&m_backgroundCache);
        if (m_highQualityRendering) {
            cachePainter.setRenderHint(QPainter::Antialiasing);
        }
        
        renderBackground(cachePainter);
        renderMapLayers(cachePainter);
        
        m_backgroundCacheValid = true;
    }
    
    painter.drawPixmap(0, 0, m_backgroundCache);
    
    // Render earthquakes and overlays
    renderEarthquakes(painter);
    if (m_settings.enableClustering) {
        renderClusters(painter);
    }
    renderSelection(painter);
    
    if (m_settings.showLegend) {
        renderLegend(painter);
    }
    
    renderOverlay(painter);
    
    #ifdef QT_DEBUG
    if (QApplication::keyboardModifiers() & Qt::ControlModifier) {
        renderDebugInfo(painter);
    }
    #endif
}

void EarthquakeMapWidget::renderBackground(QPainter &painter) const
{
    // Draw background map if available
    if (!m_backgroundMap.isNull()) {
        painter.drawPixmap(rect(), m_backgroundMap, m_backgroundMap.rect());
    }
}

void EarthquakeMapWidget::renderMapLayers(QPainter &painter) const
{
    if (m_settings.enabledLayers.contains(MapLayer::Continents)) {
        renderContinents(painter);
    }
    
    if (m_settings.enabledLayers.contains(MapLayer::Countries)) {
        renderCountries(painter);
    }
    
    if (m_settings.showGrid) {
        renderGrid(painter);
    }
}

void EarthquakeMapWidget::renderContinents(QPainter &painter) const
{
    painter.setPen(QPen(m_settings.coastlineColor, 1));
    painter.setBrush(QColor(40, 60, 80, 128));
    
    for (const QPolygonF &continent : m_continentPolygons) {
        QPolygonF screenPolygon;
        for (const QPointF &point : continent) {
            QPointF screenPoint = latLonToScreen(point.y(), point.x());
            if (rect().contains(screenPoint.toPoint())) {
                screenPolygon.append(screenPoint);
            }
        }
        
        if (screenPolygon.size() >= 3) {
            painter.drawPolygon(screenPolygon);
        }
    }
}

void EarthquakeMapWidget::renderCountries(QPainter &painter) const
{
    painter.setPen(QPen(m_settings.coastlineColor.lighter(), 1, Qt::DotLine));
    
    for (const QPolygonF &country : m_countryPolygons) {
        QPolygonF screenPolygon;
        for (const QPointF &point : country) {
            QPointF screenPoint = latLonToScreen(point.y(), point.x());
            screenPolygon.append(screenPoint);
        }
        
        if (screenPolygon.size() >= 2) {
            painter.drawPolyline(screenPolygon);
        }
    }
}

void EarthquakeMapWidget::renderGrid(QPainter &painter) const
{
    painter.setPen(QPen(m_settings.gridColor, 1, Qt::DotLine));
    
    double spacing = m_settings.gridSpacing;
    
    // Latitude lines
    for (double lat = -90; lat <= 90; lat += spacing) {
        QPointF start = latLonToScreen(lat, -180);
        QPointF end = latLonToScreen(lat, 180);
        
        if (rect().intersects(QRectF(start, end).normalized().toRect())) {
            painter.drawLine(start, end);
        }
    }
    
    // Longitude lines  
    for (double lon = -180; lon <= 180; lon += spacing) {
        QPointF start = latLonToScreen(-90, lon);
        QPointF end = latLonToScreen(90, lon);
        
        if (rect().intersects(QRectF(start, end).normalized().toRect())) {
            painter.drawLine(start, end);
        }
    }
}

void EarthquakeMapWidget::renderEarthquakes(QPainter &painter) const
{
    QMutexLocker locker(&m_dataMutex);
    
    // Sort earthquakes by size (render smaller ones first)
    QVector<int> indices;
    for (int i = 0; i < m_earthquakes.size(); ++i) {
        if (m_earthquakes[i].isVisible && !shouldSkipRendering(m_earthquakes[i])) {
            indices.append(i);
        }
    }
    
    std::sort(indices.begin(), indices.end(), [this](int a, int b) {
        return m_earthquakes[a].displaySize < m_earthquakes[b].displaySize;
    });
    
    // Limit rendering count for performance
    int maxRender = qMin(indices.size(), m_maxRenderingEarthquakes);
    
    for (int i = 0; i < maxRender; ++i) {
        const VisualEarthquake &eq = m_earthquakes[indices[i]];
        renderEarthquake(painter, eq);
    }
    
    // Render labels separately on top
    if (m_settings.showMagnitudeLabels || m_settings.showTimeLabels) {
        renderEarthquakeLabels(painter);
    }
}

void EarthquakeMapWidget::renderEarthquake(QPainter &painter, const VisualEarthquake &eq) const
{
    if (!isInViewport(eq.screenPos)) {
        return;
    }
    
    painter.save();
    painter.setOpacity(eq.opacity * m_animationOpacity);
    
    switch (m_settings.displayMode) {
        case EarthquakeDisplayMode::Circles:
            renderEarthquakeCircle(painter, eq);
            break;
        case EarthquakeDisplayMode::Squares:
            renderEarthquakeSquare(painter, eq);
            break;
        case EarthquakeDisplayMode::Diamonds:
            renderEarthquakeDiamond(painter, eq);
            break;
        case EarthquakeDisplayMode::Crosses:
            renderEarthquakeCross(painter, eq);
            break;
        case EarthquakeDisplayMode::Heatmap:
            // Handled separately in renderEarthquakeHeatmap
            break;
        default:
            renderEarthquakeCircle(painter, eq);
            break;
    }
    
    painter.restore();
}

void EarthquakeMapWidget::renderEarthquakeCircle(QPainter &painter, const VisualEarthquake &eq) const
{
    double size = getScaledSize(eq.displaySize);
    
    // Apply animation effects
    if (m_settings.animationStyle != AnimationStyle::None && m_settings.enableAnimation) {
        double animValue = getAnimationValue(m_settings.animationStyle, eq.animationPhase);
        size *= animValue;
    }
    
    QColor baseColor = eq.displayColor;
    QColor borderColor = baseColor.darker(150);
    
    // Highlight effect
    if (eq.isHighlighted) {
        baseColor = baseColor.lighter(150);
        borderColor = QColor(255, 255, 100);
        size *= 1.3;
    }
    
    // Selection effect
    if (eq.isSelected) {
        borderColor = QColor(100, 150, 255);
        painter.setPen(QPen(borderColor, 3));
    } else {
        painter.setPen(QPen(borderColor, 1));
    }
    
    painter.setBrush(baseColor);
    
    QRectF rect(eq.screenPos.x() - size/2, eq.screenPos.y() - size/2, size, size);
    painter.drawEllipse(rect);
    
    // Draw magnitude text for larger earthquakes
    if (eq.data.magnitude >= 5.0 && size > 15) {
        painter.setPen(Qt::white);
        painter.setFont(QFont("Arial", qMax(8, int(size/3))));
        QString magText = QString::number(eq.data.magnitude, 'f', 1);
        painter.drawText(rect, Qt::AlignCenter, magText);
    }
}

void EarthquakeMapWidget::renderEarthquakeSquare(QPainter &painter, const VisualEarthquake &eq) const
{
    double size = getScaledSize(eq.displaySize);
    
    if (m_settings.animationStyle != AnimationStyle::None && m_settings.enableAnimation) {
        double animValue = getAnimationValue(m_settings.animationStyle, eq.animationPhase);
        size *= animValue;
    }
    
    QColor baseColor = eq.displayColor;
    QColor borderColor = baseColor.darker(150);
    
    if (eq.isHighlighted) {
        baseColor = baseColor.lighter(150);
        size *= 1.3;
    }
    
    painter.setPen(QPen(borderColor, eq.isSelected ? 3 : 1));
    painter.setBrush(baseColor);
    
    QRectF rect(eq.screenPos.x() - size/2, eq.screenPos.y() - size/2, size, size);
    painter.drawRect(rect);
}

void EarthquakeMapWidget::renderEarthquakeDiamond(QPainter &painter, const VisualEarthquake &eq) const
{
    double size = getScaledSize(eq.displaySize);
    
    if (m_settings.animationStyle != AnimationStyle::None && m_settings.enableAnimation) {
        double animValue = getAnimationValue(m_settings.animationStyle, eq.animationPhase);
        size *= animValue;
    }
    
    QColor baseColor = eq.displayColor;
    QColor borderColor = baseColor.darker(150);
    
    if (eq.isHighlighted) {
        baseColor = baseColor.lighter(150);
        size *= 1.3;
    }
    
    painter.setPen(QPen(borderColor, eq.isSelected ? 3 : 1));
    painter.setBrush(baseColor);
    
    // Create diamond shape
    QPolygonF diamond;
    double halfSize = size / 2;
    diamond << QPointF(eq.screenPos.x(), eq.screenPos.y() - halfSize)      // top
            << QPointF(eq.screenPos.x() + halfSize, eq.screenPos.y())      // right
            << QPointF(eq.screenPos.x(), eq.screenPos.y() + halfSize)      // bottom
            << QPointF(eq.screenPos.x() - halfSize, eq.screenPos.y());     // left
    
    painter.drawPolygon(diamond);
}

void EarthquakeMapWidget::renderEarthquakeCross(QPainter &painter, const VisualEarthquake &eq) const
{
    double size = getScaledSize(eq.displaySize);
    
    if (m_settings.animationStyle != AnimationStyle::None && m_settings.enableAnimation) {
        double animValue = getAnimationValue(m_settings.animationStyle, eq.animationPhase);
        size *= animValue;
    }
    
    QColor color = eq.displayColor;
    if (eq.isHighlighted) {
        color = color.lighter(150);
        size *= 1.3;
    }
    
    painter.setPen(QPen(color, eq.isSelected ? 4 : 2));
    
    double halfSize = size / 2;
    
    // Horizontal line
    painter.drawLine(eq.screenPos.x() - halfSize, eq.screenPos.y(),
                    eq.screenPos.x() + halfSize, eq.screenPos.y());
    
    // Vertical line
    painter.drawLine(eq.screenPos.x(), eq.screenPos.y() - halfSize,
                    eq.screenPos.x(), eq.screenPos.y() + halfSize);
}

void EarthquakeMapWidget::renderEarthquakeLabels(QPainter &painter) const
{
    QMutexLocker locker(&m_dataMutex);
    
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 9, QFont::Bold));
    
    for (const VisualEarthquake &eq : m_earthquakes) {
        if (!eq.isVisible || !isInViewport(eq.screenPos)) {
            continue;
        }
        
        QString label;
        if (m_settings.showMagnitudeLabels && eq.data.magnitude >= 4.0) {
            label = QString("M%1").arg(eq.data.magnitude, 0, 'f', 1);
        }
        
        if (m_settings.showTimeLabels) {
            QString timeStr = eq.data.timestamp.toString("hh:mm");
            label += (label.isEmpty() ? "" : " ") + timeStr;
        }
        
        if (!label.isEmpty()) {
            QRectF textRect = painter.fontMetrics().boundingRect(label);
            QPointF textPos = eq.screenPos + QPointF(eq.displaySize/2 + 5, -eq.displaySize/2);
            
            // Background for better readability
            painter.fillRect(textRect.translated(textPos), QColor(0, 0, 0, 128));
            painter.drawText(textPos, label);
        }
    }
}

void EarthquakeMapWidget::renderClusters(QPainter &painter) const
{
    for (const EarthquakeCluster &cluster : m_clusters) {
        if (cluster.earthquakeIds.size() < 2) {
            continue;
        }
        
        painter.save();
        
        // Cluster background circle
        painter.setPen(QPen(cluster.displayColor.darker(), 2));
        painter.setBrush(QColor(cluster.displayColor.red(), cluster.displayColor.green(), 
                               cluster.displayColor.blue(), 180));
        
        double radius = cluster.displaySize;
        QRectF clusterRect(cluster.centerPos.x() - radius, cluster.centerPos.y() - radius,
                          radius * 2, radius * 2);
        painter.drawEllipse(clusterRect);
        
        // Cluster count text
        painter.setPen(Qt::white);
        painter.setFont(QFont("Arial", qMax(10, int(radius/3)), QFont::Bold));
        painter.drawText(clusterRect, Qt::AlignCenter, QString::number(cluster.earthquakeIds.size()));
        
        // Magnitude indicator
        if (cluster.maxMagnitude >= 5.0) {
            painter.setPen(QPen(Qt::yellow, 2));
            painter.setBrush(Qt::NoBrush);
            painter.drawEllipse(clusterRect.adjusted(-3, -3, 3, 3));
        }
        
        painter.restore();
    }
}

void EarthquakeMapWidget::renderSelection(QPainter &painter) const
{
    // Render selection rubber band
    if (m_selectionBand && m_selectionBand->isVisible()) {
        // Qt handles this automatically
    }
    
    // Render selection highlights
    painter.setPen(QPen(QColor(100, 150, 255), 3));
    painter.setBrush(Qt::NoBrush);
    
    QMutexLocker locker(&m_dataMutex);
    for (const VisualEarthquake &eq : m_earthquakes) {
        if (eq.isSelected && isInViewport(eq.screenPos)) {
            double size = getScaledSize(eq.displaySize) + 6;
            QRectF rect(eq.screenPos.x() - size/2, eq.screenPos.y() - size/2, size, size);
            painter.drawEllipse(rect);
        }
    }
}

void EarthquakeMapWidget::renderLegend(QPainter &painter) const
{
    QRect legendRect(width() - 220, 20, 200, 250);
    
    // Background
    painter.fillRect(legendRect, QColor(0, 0, 0, 180));
    painter.setPen(Qt::white);
    painter.drawRect(legendRect);
    
    // Title
    painter.setFont(QFont("Arial", 12, QFont::Bold));
    painter.drawText(legendRect.x() + 10, legendRect.y() + 20, "Earthquake Legend");
    
    // Magnitude scale
    painter.setFont(QFont("Arial", 9));
    int y = legendRect.y() + 45;
    
    for (double mag = 2.0; mag <= 8.0; mag += 1.0) {
        QColor color = getMagnitudeColor(mag);
        double size = DEFAULT_EARTHQUAKE_SIZE * (mag / 4.0);
        size = qBound(4.0, size, 25.0);
        
        painter.setBrush(color);
        painter.setPen(color.darker());
        painter.drawEllipse(legendRect.x() + 15, y - size/2, size, size);
        
        painter.setPen(Qt::white);
        painter.drawText(legendRect.x() + 40, y + 4, QString("M%1").arg(mag, 0, 'f', 1));
        y += 25;
    }
    
    // Color scheme info
    y += 10;
    painter.setFont(QFont("Arial", 9, QFont::Bold));
    painter.drawText(legendRect.x() + 10, y, "Color by:");
    y += 15;
    painter.setFont(QFont("Arial", 8));
    
    QString colorSchemeText;
    switch (m_settings.colorScheme) {
        case ColorScheme::Magnitude: colorSchemeText = "Magnitude"; break;
        case ColorScheme::Depth: colorSchemeText = "Depth"; break;
        case ColorScheme::Age: colorSchemeText = "Age"; break;
        case ColorScheme::AlertLevel: colorSchemeText = "Alert Level"; break;
        case ColorScheme::DataSource: colorSchemeText = "Data Source"; break;
        default: colorSchemeText = "Custom"; break;
    }
    painter.drawText(legendRect.x() + 10, y, colorSchemeText);
    
    // Statistics
    y += 25;
    painter.setFont(QFont("Arial", 8));
    QMutexLocker locker(&m_dataMutex);
    painter.drawText(legendRect.x() + 10, y, QString("Total: %1").arg(m_earthquakes.size()));
    y += 15;
    
    int visible = 0;
    for (const auto &eq : m_earthquakes) {
        if (eq.isVisible) visible++;
    }
    painter.drawText(legendRect.x() + 10, y, QString("Visible: %1").arg(visible));
}

void EarthquakeMapWidget::renderOverlay(QPainter &painter) const
{
    // Render scale bar
    if (m_zoomLevel > 0.1) {
        renderScaleBar(painter);
    }
    
    // Render coordinates display
    if (underMouse()) {
        QPoint mousePos = mapFromGlobal(QCursor::pos());
        if (rect().contains(mousePos)) {
            QPointF latLon = screenToLatLon(mousePos);
            QString coordsText = formatCoordinate(latLon.y(), true) + ", " + 
                               formatCoordinate(latLon.x(), false);
            
            painter.setPen(Qt::white);
            painter.setFont(QFont("Arial", 9));
            painter.fillRect(10, height() - 25, 200, 20, QColor(0, 0, 0, 128));
            painter.drawText(15, height() - 10, coordsText);
        }
    }
}

void EarthquakeMapWidget::renderScaleBar(QPainter &painter) const
{
    // Calculate scale bar length in pixels and km
    double kmPerPixel = (EARTH_RADIUS_KM * 2 * M_PI) / (360.0 * m_zoomLevel * width());
    
    // Find appropriate scale bar length
    QVector<double> scaleOptions = {1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000};
    double scaleKm = 100; // Default
    
    for (double option : scaleOptions) {
        double pixelLength = option / kmPerPixel;
        if (pixelLength >= 50 && pixelLength <= 150) {
            scaleKm = option;
            break;
        }
    }
    
    double scalePixels = scaleKm / kmPerPixel;
    
    // Draw scale bar
    QRect scaleRect(width() - 150, height() - 40, scalePixels, 20);
    
    painter.fillRect(scaleRect, QColor(255, 255, 255, 200));
    painter.setPen(Qt::black);
    painter.drawRect(scaleRect);
    
    // Scale text
    QString scaleText = scaleKm < 1000 ? 
                       QString("%1 km").arg(scaleKm) :
                       QString("%1k km").arg(scaleKm / 1000);
    
    painter.setFont(QFont("Arial", 8));
    painter.drawText(scaleRect.x(), scaleRect.y() - 5, scaleText);
}

void EarthquakeMapWidget::renderDebugInfo(QPainter &painter) const
{
    painter.setPen(Qt::yellow);
    painter.setFont(QFont("Courier", 9));
    
    QStringList debugInfo;
    debugInfo << QString("Center: %1, %2").arg(m_centerLatitude, 0, 'f', 4).arg(m_centerLongitude, 0, 'f', 4);
    debugInfo << QString("Zoom: %1").arg(m_zoomLevel, 0, 'f', 2);
    debugInfo << QString("Earthquakes: %1 total, %2 visible").arg(m_earthquakes.size()).arg(getVisibleEarthquakes().size());
    debugInfo << QString("Clusters: %1").arg(m_clusters.size());
    debugInfo << QString("Animation Frame: %1").arg(m_animationFrame);
    debugInfo << QString("Cache Valid: BG=%1, Layer=%2").arg(m_backgroundCacheValid).arg(m_layerCacheValid);
    
    int y = 10;
    for (const QString &line : debugInfo) {
        painter.fillRect(5, y - 2, painter.fontMetrics().horizontalAdvance(line) + 4, 
                        painter.fontMetrics().height() + 2, QColor(0, 0, 0, 128));
        painter.drawText(7, y + painter.fontMetrics().ascent(), line);
        y += painter.fontMetrics().height() + 2;
    }
}

// Mouse event handling
void EarthquakeMapWidget::mousePressEvent(QMouseEvent *event)
{
    setFocus();
    m_lastMousePos = event->pos();
    
    if (event->button() == Qt::LeftButton) {
        if (event->modifiers() & Qt::ControlModifier) {
            // Start selection
            m_isSelecting = true;
            m_selectionStart = event->pos();
            
            if (!m_selectionBand) {
                m_selectionBand = new QRubberBand(QRubberBand::Rectangle, this);
            }
            m_selectionBand->setGeometry(QRect(m_selectionStart, QSize()));
            m_selectionBand->show();
        } else {
            // Check for earthquake click
            int earthquakeIndex = findEarthquakeAt(event->pos());
            
            if (earthquakeIndex >= 0) {
                const EarthquakeData &earthquake = m_earthquakes[earthquakeIndex].data;
                
                if (event->modifiers() & Qt::ShiftModifier) {
                    // Add to selection
                    if (!m_selectedIds.contains(earthquake.eventId)) {
                        m_selectedIds.append(earthquake.eventId);
                        m_earthquakes[earthquakeIndex].isSelected = true;
                    }
                } else {
                    // Single selection
                    clearSelection();
                    m_selectedIds.append(earthquake.eventId);
                    m_earthquakes[earthquakeIndex].isSelected = true;
                }
                
                emit earthquakeClicked(earthquake);
                emit selectionChanged(getSelectedEarthquakes());
                update();
            } else {
                // Start panning
                m_isPanning = true;
                m_panStartPos = event->pos();
                m_panStartCenter = QPointF(m_centerLongitude, m_centerLatitude);
                setCursor(Qt::ClosedHandCursor);
                
                // Clear selection if no modifiers
                if (event->modifiers() == Qt::NoModifier) {
                    clearSelection();
                }
                
                // Emit map click
                QPointF latLon = screenToLatLon(event->pos());
                emit mapClicked(latLon.y(), latLon.x());
            }
        }
    }
    
    QWidget::mousePressEvent(event);
}

void EarthquakeMapWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isPanning && (event->buttons() & Qt::LeftButton)) {
        // Pan the map
        QPoint delta = event->pos() - m_panStartPos;
        
        // Convert pixel movement to coordinate movement
        double latDelta = -delta.y() * (180.0 / height()) / m_zoomLevel;
        double lonDelta = -delta.x() * (360.0 / width()) / m_zoomLevel;
        
        double newLat = m_panStartCenter.y() + latDelta;
        double newLon = m_panStartCenter.x() + lonDelta;
        
        setCenter(newLat, newLon);
        
    } else if (m_isSelecting && (event->buttons() & Qt::LeftButton)) {
        // Update selection rectangle
        if (m_selectionBand) {
            QRect selectionRect = QRect(m_selectionStart, event->pos()).normalized();
            m_selectionBand->setGeometry(selectionRect);
        }
        
    } else {
        // Handle hover
        int earthquakeIndex = findEarthquakeAt(event->pos());
        
        QString newHoveredId;
        if (earthquakeIndex >= 0) {
            newHoveredId = m_earthquakes[earthquakeIndex].data.eventId;
            setCursor(Qt::PointingHandCursor);
            
            // Show tooltip
            if (m_settings.showTooltips) {
                QString tooltip = formatEarthquakeTooltip(m_earthquakes[earthquakeIndex].data);
                showTooltip(event->pos(), tooltip);
            }
            
            // Emit hover event
            if (newHoveredId != m_hoveredEarthquakeId) {
                emit earthquakeHovered(m_earthquakes[earthquakeIndex].data);
            }
        } else {
            setCursor(Qt::ArrowCursor);
            hideTooltip();
        }
        
        // Update highlighting
        if (newHoveredId != m_hoveredEarthquakeId) {
            // Clear old highlight
            if (!m_hoveredEarthquakeId.isEmpty()) {
                for (auto &eq : m_earthquakes) {
                    if (eq.data.eventId == m_hoveredEarthquakeId) {
                        eq.isHighlighted = false;
                        break;
                    }
                }
            }
            
            // Set new highlight
            if (!newHoveredId.isEmpty()) {
                for (auto &eq : m_earthquakes) {
                    if (eq.data.eventId == newHoveredId) {
                        eq.isHighlighted = true;
                        break;
                    }
                }
            }
            
            m_hoveredEarthquakeId = newHoveredId;
            update();
        }
    }
    
    m_lastMousePos = event->pos();
    QWidget::mouseMoveEvent(event);
}

void EarthquakeMapWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_isPanning) {
            m_isPanning = false;
            setCursor(Qt::ArrowCursor);
            
        } else if (m_isSelecting) {
            // Complete selection
            m_isSelecting = false;
            
            if (m_selectionBand) {
                QRect selectionRect = m_selectionBand->geometry();
                m_selectionBand->hide();
                
                // Find earthquakes in selection
                QVector<int> selectedIndices = findEarthquakesInRect(selectionRect);
                
                if (!(event->modifiers() & Qt::ShiftModifier)) {
                    clearSelection();
                }
                
                for (int index : selectedIndices) {
                    QString eventId = m_earthquakes[index].data.eventId;
                    if (!m_selectedIds.contains(eventId)) {
                        m_selectedIds.append(eventId);
                        m_earthquakes[index].isSelected = true;
                    }
                }
                
                if (!selectedIndices.isEmpty()) {
                    emit selectionChanged(getSelectedEarthquakes());
                    update();
                }
            }
        }
    }
    
    QWidget::mouseReleaseEvent(event);
}

void EarthquakeMapWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        int earthquakeIndex = findEarthquakeAt(event->pos());
        
        if (earthquakeIndex >= 0) {
            // Double-click on earthquake
            const EarthquakeData &earthquake = m_earthquakes[earthquakeIndex].data;
            emit earthquakeDoubleClicked(earthquake);
            
            // Zoom in and center on earthquake
            animateToLocation(earthquake.latitude, earthquake.longitude, m_zoomLevel * 2.0);
        } else {
            // Double-click on empty space - zoom in
            QPointF latLon = screenToLatLon(event->pos());
            animateToLocation(latLon.y(), latLon.x(), m_zoomLevel * 2.0);
        }
    }
    
    QWidget::mouseDoubleClickEvent(event);
}

void EarthquakeMapWidget::wheelEvent(QWheelEvent *event)
{
    // Zoom towards mouse position
    QPointF mouseLatLon = screenToLatLon(event->position().toPoint());
    
    double zoomFactor = event->angleDelta().y() > 0 ? ZOOM_FACTOR : 1.0 / ZOOM_FACTOR;
    double newZoom = qBound(MIN_ZOOM, m_zoomLevel * zoomFactor, MAX_ZOOM);
    
    if (qAbs(newZoom - m_zoomLevel) > 1e-6) {
        // Calculate new center to keep mouse position fixed
        QPointF currentCenter(m_centerLongitude, m_centerLatitude);
        QPointF offset = mouseLatLon - currentCenter;
        offset *= (1.0 - 1.0 / zoomFactor);
        QPointF newCenter = currentCenter + offset;
        
        setCenter(newCenter.y(), newCenter.x());
        setZoomLevel(newZoom);
    }
    
    QWidget::wheelEvent(event);
}

void EarthquakeMapWidget::keyPressEvent(QKeyEvent *event)
{
    const double PAN_STEP = 10.0 / m_zoomLevel;
    
    switch (event->key()) {
        case Qt::Key_Up:
            setCenter(m_centerLatitude + PAN_STEP, m_centerLongitude);
            break;
        case Qt::Key_Down:
            setCenter(m_centerLatitude - PAN_STEP, m_centerLongitude);
            break;
        case Qt::Key_Left:
            setCenter(m_centerLatitude, m_centerLongitude - PAN_STEP);
            break;
        case Qt::Key_Right:
            setCenter(m_centerLatitude, m_centerLongitude + PAN_STEP);
            break;
        case Qt::Key_Plus:
        case Qt::Key_Equal:
            zoomIn();
            break;
        case Qt::Key_Minus:
            zoomOut();
            break;
        case Qt::Key_Home:
            setCenter(0, 0);
            setZoomLevel(1.0);
            break;
        case Qt::Key_F:
            fitToEarthquakes();
            break;
        case Qt::Key_A:
            if (event->modifiers() & Qt::ControlModifier) {
                // Select all visible earthquakes
                QStringList visibleIds;
                for (const auto &eq : m_earthquakes) {
                    if (eq.isVisible) {
                        visibleIds.append(eq.data.eventId);
                    }
                }
                selectEarthquakes(visibleIds);
            }
            break;
        case Qt::Key_Escape:
            clearSelection();
            break;
        default:
            QWidget::keyPressEvent(event);
            return;
    }
    
    event->accept();
}

void EarthquakeMapWidget::resizeEvent(QResizeEvent *event)
{
    updateVisibleBounds();
    updateVisibleEarthquakes();
    
    m_backgroundCacheValid = false;
    m_layerCacheValid = false;
    
    QWidget::resizeEvent(event);
}

void EarthquakeMapWidget::contextMenuEvent(QContextMenuEvent *event)
{
    int earthquakeIndex = findEarthquakeAt(event->pos());
    
    if (earthquakeIndex >= 0) {
        const EarthquakeData &earthquake = m_earthquakes[earthquakeIndex].data;
        emit contextMenuRequested(event->pos(), earthquake);
    }
    
    if (m_contextMenu) {
        m_contextMenu->exec(event->globalPos());
    }
    
    QWidget::contextMenuEvent(event);
}

void EarthquakeMapWidget::leaveEvent(QEvent *event)
{
    // Clear hover state
    if (!m_hoveredEarthquakeId.isEmpty()) {
        for (auto &eq : m_earthquakes) {
            if (eq.data.eventId == m_hoveredEarthquakeId) {
                eq.isHighlighted = false;
                break;
            }
        }
        m_hoveredEarthquakeId.clear();
        update();
    }
    
    hideTooltip();
    setCursor(Qt::ArrowCursor);
    
    QWidget::leaveEvent(event);
}

// Coordinate transformation methods
QPointF EarthquakeMapWidget::latLonToScreen(double latitude, double longitude) const
{
    QPointF projected = projectCoordinate(latitude, longitude);
    
    // Convert to screen coordinates
    double x = (projected.x() - m_centerLongitude) * m_zoomLevel * width() / 360.0 + width() / 2.0;
    double y = (m_centerLatitude - projected.y()) * m_zoomLevel * height() / 180.0 + height() / 2.0;
    
    return QPointF(x, y);
}

QPointF EarthquakeMapWidget::screenToLatLon(const QPointF &screen) const
{
    double longitude = (screen.x() - width() / 2.0) * 360.0 / (m_zoomLevel * width()) + m_centerLongitude;
    double latitude = m_centerLatitude - (screen.y() - height() / 2.0) * 180.0 / (m_zoomLevel * height());
    
    latitude = qBound(-90.0, latitude, 90.0);
    longitude = SpatialUtils::normalizeLongitude(longitude);
    
    return QPointF(longitude, latitude);
}

QPointF EarthquakeMapWidget::projectCoordinate(double latitude, double longitude) const
{
    switch (m_settings.projection) {
        case MapProjection::Mercator:
            return mercatorProjection(latitude, longitude);
        case MapProjection::Equirectangular:
            return equirectangularProjection(latitude, longitude);
        case MapProjection::OrthographicNorthPole:
            return orthographicProjection(latitude, longitude, true);
        case MapProjection::OrthographicSouthPole:
            return orthographicProjection(latitude, longitude, false);
        case MapProjection::Robinson:
            return robinsonProjection(latitude, longitude);
        default:
            return mercatorProjection(latitude, longitude);
    }
}

QPointF EarthquakeMapWidget::mercatorProjection(double lat, double lon) const
{
    double x = lon;
    double y = lat;
    
    // Apply Mercator transformation for y-coordinate
    if (qAbs(lat) < 85.0) { // Avoid singularity at poles
        double latRad = lat * M_PI / 180.0;
        y = log(tan(M_PI/4 + latRad/2)) * 180.0 / M_PI;
    } else {
        y = lat > 0 ? 85.0 : -85.0;
    }
    
    return QPointF(x, y);
}

QPointF EarthquakeMapWidget::equirectangularProjection(double lat, double lon) const
{
    return QPointF(lon, lat);
}

QPointF EarthquakeMapWidget::orthographicProjection(double lat, double lon, bool northPole) const
{
    double latRad = lat * M_PI / 180.0;
    double lonRad = lon * M_PI / 180.0;
    
    double centerLat = northPole ? 90.0 : -90.0;
    double centerLatRad = centerLat * M_PI / 180.0;
    
    double x = cos(latRad) * sin(lonRad);
    double y = cos(centerLatRad) * sin(latRad) - sin(centerLatRad) * cos(latRad) * cos(lonRad);
    
    return QPointF(x * 180.0, y * 180.0);
}

QPointF EarthquakeMapWidget::robinsonProjection(double lat, double lon) const
{
    // Simplified Robinson projection
    double latRad = lat * M_PI / 180.0;
    
    double x = lon * cos(latRad * 0.6);
    double y = lat * 1.3;
    
    return QPointF(x, y);
}

// Color and styling methods
QColor EarthquakeMapWidget::getEarthquakeColor(const EarthquakeData &earthquake) const
{
    switch (m_settings.colorScheme) {
        case ColorScheme::Magnitude:
            return getMagnitudeColor(earthquake.magnitude);
        case ColorScheme::Depth:
            return getDepthColor(earthquake.depth);
        case ColorScheme::Age:
            return getAgeColor(earthquake.timestamp);
        case ColorScheme::AlertLevel:
            return getAlertLevelColor(earthquake.alertLevel);
        case ColorScheme::DataSource:
            // Simple hash-based coloring by data source
            return QColor::fromHsv((qHash(earthquake.dataSource) % 360), 200, 200);
        default:
            return getMagnitudeColor(earthquake.magnitude);
    }
}

QColor EarthquakeMapWidget::getMagnitudeColor(double magnitude) const
{
    if (magnitude < 1.0) return QColor(200, 255, 200);      // Very light green
    else if (magnitude < 2.0) return QColor(150, 255, 150); // Light green
    else if (magnitude < 3.0) return QColor(100, 255, 100); // Green
    else if (magnitude < 4.0) return QColor(255, 255, 100); // Yellow
    else if (magnitude < 5.0) return QColor(255, 200, 100); // Orange
    else if (magnitude < 6.0) return QColor(255, 150, 100); // Light red
    else if (magnitude < 7.0) return QColor(255, 100, 100); // Red
    else if (magnitude < 8.0) return QColor(200, 50, 50);   // Dark red
    else return QColor(150, 0, 150);                        // Purple
}

QColor EarthquakeMapWidget::getDepthColor(double depth) const
{
    // Shallow = red, deep = blue
    double normalizedDepth = qBound(0.0, depth / 700.0, 1.0); // 700km max depth
    
    int red = int(255 * (1.0 - normalizedDepth));
    int blue = int(255 * normalizedDepth);
    int green = int(128 * (1.0 - qAbs(normalizedDepth - 0.5) * 2.0));
    
    return QColor(red, green, blue);
}

QColor EarthquakeMapWidget::getAgeColor(const QDateTime &timestamp) const
{
    qint64 ageSeconds = timestamp.secsTo(QDateTime::currentDateTimeUtc());
    double ageHours = ageSeconds / 3600.0;
    
    if (ageHours < 1.0) return QColor(255, 50, 50);        // Red - very recent
    else if (ageHours < 6.0) return QColor(255, 150, 50);  // Orange
    else if (ageHours < 24.0) return QColor(255, 255, 50); // Yellow
    else if (ageHours < 168.0) return QColor(150, 255, 50); // Green
    else return QColor(100, 100, 200);                     // Blue - old
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

double EarthquakeMapWidget::getEarthquakeSize(const EarthquakeData &earthquake) const
{
    double baseSize = DEFAULT_EARTHQUAKE_SIZE;
    
    // Size based on magnitude (logarithmic scale)
    double size = baseSize * pow(2.0, (earthquake.magnitude - 3.0) / 2.0);
    
    // Minimum and maximum size limits
    size = qBound(3.0, size, 50.0);
    
    return size;
}

double EarthquakeMapWidget::getScaledSize(double baseSize) const
{
    // Scale size based on zoom level
    double scaleFactor = qBound(0.5, sqrt(m_zoomLevel), 3.0);
    return baseSize * scaleFactor;
}

// Animation methods
void EarthquakeMapWidget::updateAnimation()
{
    if (!m_animationEnabled) {
        return;
    }
    
    m_animationFrame = (m_animationFrame + 1) % (ANIMATION_FPS * 6); // 6 second cycle
    
    // Update earthquake animations
    updateEarthquakeAnimations();
    
    emit animationFrameUpdated(m_animationFrame);
    update();
}

void EarthquakeMapWidget::updateEarthquakeAnimations()
{
    QMutexLocker locker(&m_dataMutex);
    
    for (auto &eq : m_earthquakes) {
        eq.animationPhase = calculateAnimationPhase(eq.data);
    }
}

double EarthquakeMapWidget::calculateAnimationPhase(const EarthquakeData &earthquake) const
{
    // Base animation on time since earthquake occurred
    qint64 ageSeconds = earthquake.timestamp.secsTo(QDateTime::currentDateTimeUtc());
    double ageHours = ageSeconds / 3600.0;
    
    // Recent earthquakes animate more
    if (ageHours < 1.0) {
        return sin(m_animationFrame * 0.3) * 0.5 + 0.5; // Fast pulse
    } else if (ageHours < 24.0) {
        return sin(m_animationFrame * 0.1) * 0.3 + 0.7; // Slow pulse
    }
    
    return 1.0; // No animation for old earthquakes
}

double EarthquakeMapWidget::getAnimationValue(AnimationStyle style, double phase) const
{
    switch (style) {
        case AnimationStyle::Pulse:
            return 0.8 + 0.2 * sin(phase * 2 * M_PI);
        case AnimationStyle::Ripple:
            return 1.0 + 0.5 * sin(phase * 4 * M_PI) * exp(-phase * 3);
        case AnimationStyle::Fade:
            return qBound(0.3, 1.0 - phase * 0.7, 1.0);
        case AnimationStyle::Grow:
            return qBound(0.5, 0.5 + phase, 1.5);
        case AnimationStyle::Shake:
            return 1.0 + 0.1 * sin(phase * 8 * M_PI);
        default:
            return 1.0;
    }
}

// Utility and helper methods
void EarthquakeMapWidget::updateVisibleBounds()
{
    QPointF topLeft = screenToLatLon(QPointF(0, 0));
    QPointF bottomRight = screenToLatLon(QPointF(width(), height()));
    
    m_visibleBounds.minLatitude = bottomRight.y();
    m_visibleBounds.maxLatitude = topLeft.y();
    m_visibleBounds.minLongitude = topLeft.x();
    m_visibleBounds.maxLongitude = bottomRight.x();
    
    // Handle longitude wraparound
    if (m_visibleBounds.minLongitude > m_visibleBounds.maxLongitude) {
        m_visibleBounds.maxLongitude += 360.0;
    }
}

void EarthquakeMapWidget::updateVisibleEarthquakes()
{
    QMutexLocker locker(&m_dataMutex);
    
    for (auto &eq : m_earthquakes) {
        eq.screenPos = latLonToScreen(eq.data.latitude, eq.data.longitude);
        eq.isVisible = isEarthquakeVisible(eq.data) && passesFilters(eq.data);
        eq.displaySize = getEarthquakeSize(eq.data);
        eq.displayColor = getEarthquakeColor(eq.data);
    }
}

QString EarthquakeMapWidget::formatEarthquakeTooltip(const EarthquakeData &earthquake) const
{
    QString tooltip = QString("<b>M%1 Earthquake</b><br>").arg(earthquake.magnitude, 0, 'f', 1);
    tooltip += QString("<b>Location:</b> %1<br>").arg(earthquake.location);
    tooltip += QString("<b>Depth:</b> %1 km<br>").arg(earthquake.depth, 0, 'f', 1);
    tooltip += QString("<b>Time:</b> %1 UTC<br>").arg(earthquake.timestamp.toString("yyyy-MM-dd hh:mm:ss"));
    tooltip += QString("<b>Event ID:</b> %1<br>").arg(earthquake.eventId);
    
    if (!earthquake.dataSource.isEmpty()) {
        tooltip += QString("<b>Source:</b> %1<br>").arg(earthquake.dataSource);
    }
    
    if (earthquake.tsunamiFlag == "Yes") {
        tooltip += "<b><font color='red'>⚠️ TSUNAMI POSSIBLE</font></b>";
    }
    
    return tooltip;
}

void EarthquakeMapWidget::saveSettings() const
{
    QSettings settings("EarthquakeAlertSystem", "MapWidget");
    
    settings.setValue("centerLatitude", m_centerLatitude);
    settings.setValue("centerLongitude", m_centerLongitude);
    settings.setValue("zoomLevel", m_zoomLevel);
    settings.setValue("projection", static_cast<int>(m_settings.projection));
    settings.setValue("displayMode", static_cast<int>(m_settings.displayMode));
    settings.setValue("colorScheme", static_cast<int>(m_settings.colorScheme));
    settings.setValue("showGrid", m_settings.showGrid);
    settings.setValue("showLegend", m_settings.showLegend);
    settings.setValue("enableClustering", m_settings.enableClustering);
    settings.setValue("animationEnabled", m_animationEnabled);
}

void EarthquakeMapWidget::loadSettings()
{
    QSettings settings("EarthquakeAlertSystem", "MapWidget");
    
    m_centerLatitude = settings.value("centerLatitude", 0.0).toDouble();
    m_centerLongitude = settings.value("centerLongitude", 0.0).toDouble();
    m_zoomLevel = settings.value("zoomLevel", 1.0).toDouble();
    
    m_settings.projection = static_cast<MapProjection>(
        settings.value("projection", static_cast<int>(MapProjection::Mercator)).toInt());
    m_settings.displayMode = static_cast<EarthquakeDisplayMode>(
        settings.value("displayMode", static_cast<int>(EarthquakeDisplayMode::Circles)).toInt());
    m_settings.colorScheme = static_cast<ColorScheme>(
        settings.value("colorScheme", static_cast<int>(ColorScheme::Magnitude)).toInt());
    m_settings.showGrid = settings.value("showGrid", true).toBool();
    m_settings.showLegend = settings.value("showLegend", true).toBool();
    m_settings.enableClustering = settings.value("enableClustering", true).toBool();
    m_animationEnabled = settings.value("animationEnabled", true).toBool();
    
    updateVisibleBounds();
}

// Additional utility methods implementation
bool EarthquakeMapWidget::isEarthquakeVisible(const EarthquakeData &earthquake) const
{
    return m_visibleBounds.contains(earthquake.latitude, earthquake.longitude);
}

bool EarthquakeMapWidget::passesFilters(const EarthquakeData &earthquake) const
{
    // Magnitude filter
    if (earthquake.magnitude < m_minMagnitude || earthquake.magnitude > m_maxMagnitude) {
        return false;
    }
    
    // Depth filter
    if (earthquake.depth < m_minDepth || earthquake.depth > m_maxDepth) {
        return false;
    }
    
    // Time filter
    if (m_startTime.isValid() && earthquake.timestamp < m_startTime) {
        return false;
    }
    if (m_endTime.isValid() && earthquake.timestamp > m_endTime) {
        return false;
    }
    
    // Location filter
    if (m_hasLocationFilter && !m_locationFilter.contains(earthquake.latitude, earthquake.longitude)) {
        return false;
    }
    
    return true;
}

bool EarthquakeMapWidget::isInViewport(const QPointF &screenPos) const
{
    return rect().contains(screenPos.toPoint());
}

int EarthquakeMapWidget::findEarthquakeAt(const QPoint &point) const
{
    QMutexLocker locker(&m_dataMutex);
    
    double minDistance = std::numeric_limits<double>::max();
    int closestIndex = -1;
    
    for (int i = 0; i < m_earthquakes.size(); ++i) {
        if (!m_earthquakes[i].isVisible) continue;
        
        double distance = distanceToEarthquake(point, i);
        double threshold = getScaledSize(m_earthquakes[i].displaySize) / 2.0 + 5.0; // 5px tolerance
        
        if (distance <= threshold && distance < minDistance) {
            minDistance = distance;
            closestIndex = i;
        }
    }
    
    return closestIndex;
}

QVector<int> EarthquakeMapWidget::findEarthquakesInRect(const QRect &rect) const
{
    QMutexLocker locker(&m_dataMutex);
    QVector<int> indices;
    
    for (int i = 0; i < m_earthquakes.size(); ++i) {
        if (m_earthquakes[i].isVisible && rect.contains(m_earthquakes[i].screenPos.toPoint())) {
            indices.append(i);
        }
    }
    
    return indices;
}

double EarthquakeMapWidget::distanceToEarthquake(const QPoint &point, int earthquakeIndex) const
{
    if (earthquakeIndex < 0 || earthquakeIndex >= m_earthquakes.size()) {
        return std::numeric_limits<double>::max();
    }
    
    const QPointF &eqPos = m_earthquakes[earthquakeIndex].screenPos;
    double dx = point.x() - eqPos.x();
    double dy = point.y() - eqPos.y();
    return sqrt(dx * dx + dy * dy);
}

bool EarthquakeMapWidget::shouldSkipRendering(const VisualEarthquake &eq) const
{
    // Skip if too far outside viewport
    QPointF pos = eq.screenPos;
    QRect extended = rect().adjusted(-100, -100, 100, 100);
    if (!extended.contains(pos.toPoint())) {
        return true;
    }
    
    // Skip very small earthquakes at low zoom levels
    if (m_zoomLevel < m_lodThreshold && eq.data.magnitude < 3.0) {
        return true;
    }
    
    return false;
}

void EarthquakeMapWidget::updateClusters()
{
    if (!m_settings.enableClustering) {
        clearClusters();
        return;
    }
    
    QMutexLocker locker(&m_dataMutex);
    clearClusters();
    
    QVector<bool> clustered(m_earthquakes.size(), false);
    
    for (int i = 0; i < m_earthquakes.size(); ++i) {
        if (clustered[i] || !m_earthquakes[i].isVisible) continue;
        
        QVector<int> clusterIndices;
        clusterIndices.append(i);
        clustered[i] = true;
        
        // Find nearby earthquakes
        for (int j = i + 1; j < m_earthquakes.size(); ++j) {
            if (clustered[j] || !m_earthquakes[j].isVisible) continue;
            
            if (shouldCluster(m_earthquakes[i], m_earthquakes[j])) {
                clusterIndices.append(j);
                clustered[j] = true;
            }
        }
        
        // Create cluster if multiple earthquakes
        if (clusterIndices.size() > 1) {
            EarthquakeCluster cluster = createCluster(clusterIndices);
            m_clusters.append(cluster);
            
            // Update earthquake cluster IDs
            for (int idx : clusterIndices) {
                m_earthquakes[idx].clusterId = m_clusters.size() - 1;
            }
        }
    }
    
    qDebug() << "Updated clusters:" << m_clusters.size() << "clusters created";
}

void EarthquakeMapWidget::clearClusters()
{
    m_clusters.clear();
    
    QMutexLocker locker(&m_dataMutex);
    for (auto &eq : m_earthquakes) {
        eq.clusterId = -1;
        eq.isClusterCenter = false;
    }
}

bool EarthquakeMapWidget::shouldCluster(const VisualEarthquake &eq1, const VisualEarthquake &eq2) const
{
    double distance = sqrt(pow(eq1.screenPos.x() - eq2.screenPos.x(), 2) + 
                          pow(eq1.screenPos.y() - eq2.screenPos.y(), 2));
    return distance <= m_settings.clusterDistance;
}

EarthquakeCluster EarthquakeMapWidget::createCluster(const QVector<int> &earthquakeIds)
{
    EarthquakeCluster cluster;
    cluster.earthquakeIds = earthquakeIds;
    
    // Calculate center position
    double totalX = 0, totalY = 0;
    double totalMag = 0, maxMag = 0;
    QDateTime latestTime;
    
    for (int id : earthquakeIds) {
        const VisualEarthquake &eq = m_earthquakes[id];
        totalX += eq.screenPos.x();
        totalY += eq.screenPos.y();
        totalMag += eq.data.magnitude;
        maxMag = qMax(maxMag, eq.data.magnitude);
        
        if (!latestTime.isValid() || eq.data.timestamp > latestTime) {
            latestTime = eq.data.timestamp;
        }
    }
    
    cluster.centerPos = QPointF(totalX / earthquakeIds.size(), totalY / earthquakeIds.size());
    cluster.avgMagnitude = totalMag / earthquakeIds.size();
    cluster.maxMagnitude = maxMag;
    cluster.latestTime = latestTime;
    cluster.displayColor = getMagnitudeColor(cluster.avgMagnitude);
    cluster.displaySize = qBound(15.0, 10.0 + earthquakeIds.size() * 2.0, 50.0);
    cluster.isExpanded = false;
    
    return cluster;
}

MapBounds EarthquakeMapWidget::calculateBounds(const QVector<EarthquakeData> &earthquakes) const
{
    if (earthquakes.isEmpty()) {
        return MapBounds{-90, 90, -180, 180};
    }
    
    MapBounds bounds;
    bounds.minLatitude = earthquakes.first().latitude;
    bounds.maxLatitude = earthquakes.first().latitude;
    bounds.minLongitude = earthquakes.first().longitude;
    bounds.maxLongitude = earthquakes.first().longitude;
    
    for (const auto &eq : earthquakes) {
        bounds.minLatitude = qMin(bounds.minLatitude, eq.latitude);
        bounds.maxLatitude = qMax(bounds.maxLatitude, eq.latitude);
        bounds.minLongitude = qMin(bounds.minLongitude, eq.longitude);
        bounds.maxLongitude = qMax(bounds.maxLongitude, eq.longitude);
    }
    
    // Add padding
    double latPadding = (bounds.maxLatitude - bounds.minLatitude) * 0.1;
    double lonPadding = (bounds.maxLongitude - bounds.minLongitude) * 0.1;
    
    bounds.minLatitude -= latPadding;
    bounds.maxLatitude += latPadding;
    bounds.minLongitude -= lonPadding;
    bounds.maxLongitude += lonPadding;
    
    return bounds;
}

double EarthquakeMapWidget::calculateOptimalZoom(const MapBounds &bounds) const
{
    if (!bounds.isValid()) {
        return 1.0;
    }
    
    double latZoom = 180.0 / bounds.height();
    double lonZoom = 360.0 / bounds.width();
    
    double zoom = qMin(latZoom, lonZoom) * 0.8; // 80% to leave some margin
    return qBound(MIN_ZOOM, zoom, MAX_ZOOM);
}

QString EarthquakeMapWidget::formatCoordinate(double value, bool isLatitude) const
{
    QString suffix = isLatitude ? (value >= 0 ? "N" : "S") : (value >= 0 ? "E" : "W");
    return QString("%1°%2").arg(qAbs(value), 0, 'f', 4).arg(suffix);
}

void EarthquakeMapWidget::showTooltip(const QPoint &pos, const QString &text)
{
    QToolTip::showText(mapToGlobal(pos), text, this, QRect(), 5000);
}

void EarthquakeMapWidget::hideTooltip()
{
    QToolTip::hideText();
}

// Selection and filtering methods
QVector<EarthquakeData> EarthquakeMapWidget::getSelectedEarthquakes() const
{
    QMutexLocker locker(&m_dataMutex);
    QVector<EarthquakeData> selected;
    
    for (const auto &eq : m_earthquakes) {
        if (eq.isSelected) {
            selected.append(eq.data);
        }
    }
    
    return selected;
}

void EarthquakeMapWidget::selectEarthquake(const QString &eventId)
{
    clearSelection();
    selectEarthquakes({eventId});
}

void EarthquakeMapWidget::selectEarthquakes(const QStringList &eventIds)
{
    QMutexLocker locker(&m_dataMutex);
    
    for (const QString &id : eventIds) {
        if (!m_selectedIds.contains(id)) {
            m_selectedIds.append(id);
        }
        
        for (auto &eq : m_earthquakes) {
            if (eq.data.eventId == id) {
                eq.isSelected = true;
                break;
            }
        }
    }
    
    emit selectionChanged(getSelectedEarthquakes());
    update();
}

void EarthquakeMapWidget::clearSelection()
{
    if (m_selectedIds.isEmpty()) return;
    
    QMutexLocker locker(&m_dataMutex);
    
    for (auto &eq : m_earthquakes) {
        eq.isSelected = false;
    }
    
    m_selectedIds.clear();
    
    emit selectionChanged(QVector<EarthquakeData>());
    update();
}

void EarthquakeMapWidget::setMagnitudeFilter(double minMag, double maxMag)
{
    m_minMagnitude = minMag;
    m_maxMagnitude = maxMag;
    updateVisibleEarthquakes();
    update();
}

void EarthquakeMapWidget::setDepthFilter(double minDepth, double maxDepth)
{
    m_minDepth = minDepth;
    m_maxDepth = maxDepth;
    updateVisibleEarthquakes();
    update();
}

void EarthquakeMapWidget::setTimeFilter(const QDateTime &startTime, const QDateTime &endTime)
{
    m_startTime = startTime;
    m_endTime = endTime;
    updateVisibleEarthquakes();
    update();
}

// Export methods
QPixmap EarthquakeMapWidget::renderToPixmap(const QSize &size) const
{
    QSize renderSize = size.isValid() ? size : this->size();
    QPixmap pixmap(renderSize);
    pixmap.fill(m_settings.backgroundColor);
    
    QPainter painter(&pixmap);
    if (m_highQualityRendering) {
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
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
    
    return pixmap;
}

void EarthquakeMapWidget::exportToImage(const QString &fileName, const QSize &size) const
{
    QPixmap pixmap = renderToPixmap(size);
    
    QString actualFileName = fileName;
    if (actualFileName.isEmpty()) {
        actualFileName = QString("earthquake_map_%1.png")
                        .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss"));
    }
    
    if (pixmap.save(actualFileName)) {
        qDebug() << "Map exported to:" << actualFileName;
    } else {
        qWarning() << "Failed to export map to:" << actualFileName;
    }
}

// Getter methods
MapBounds EarthquakeMapWidget::getVisibleBounds() const
{
    return m_visibleBounds;
}

QVector<EarthquakeData> EarthquakeMapWidget::getAllEarthquakes() const
{
    QMutexLocker locker(&m_dataMutex);
    QVector<EarthquakeData> all;
    
    for (const auto &eq : m_earthquakes) {
        all.append(eq.data);
    }
    
    return all;
}

QVector<EarthquakeData> EarthquakeMapWidget::getVisibleEarthquakes() const
{
    QMutexLocker locker(&m_dataMutex);
    QVector<EarthquakeData> visible;
    
    for (const auto &eq : m_earthquakes) {
        if (eq.isVisible) {
            visible.append(eq.data);
        }
    }
    
    return visible;
}

int EarthquakeMapWidget::getEarthquakeCount() const
{
    QMutexLocker locker(&m_dataMutex);
    return m_earthquakes.size();
}

// Network slots
void EarthquakeMapWidget::onNetworkReplyFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        // Process downloaded map data
        QPixmap mapImage;
        if (mapImage.loadFromData(data)) {
            setBackgroundMap(mapImage);
            emit backgroundMapLoaded();
        }
    }
    
    reply->deleteLater();
}

void EarthquakeMapWidget::onAnimationFinished()
{
    // Animation finished - could trigger follow-up actions
}

void EarthquakeMapWidget::onSelectionAnimationFinished()
{
    // Selection animation finished
}

// Settings methods
void EarthquakeMapWidget::setMapSettings(const MapSettings &settings)
{
    m_settings = settings;
    
    m_backgroundCacheValid = false;
    m_layerCacheValid = false;
    
    updateVisibleEarthquakes();
    if (m_settings.enableClustering) {
        updateClusters();
    } else {
        clearClusters();
    }
    
    update();
}

MapSettings EarthquakeMapWidget::getMapSettings() const
{
    return m_settings;
}

void EarthquakeMapWidget::setProjection(MapProjection projection)
{
    m_settings.projection = projection;
    m_backgroundCacheValid = false;
    updateVisibleEarthquakes();
    update();
}

void EarthquakeMapWidget::setDisplayMode(EarthquakeDisplayMode mode)
{
    m_settings.displayMode = mode;
    update();
}

void EarthquakeMapWidget::setColorScheme(ColorScheme scheme)
{
    m_settings.colorScheme = scheme;
    updateVisibleEarthquakes(); // Recalculate colors
    update();
}

void EarthquakeMapWidget::setAnimationStyle(AnimationStyle style)
{
    m_settings.animationStyle = style;
    update();
}

// Animation control
void EarthquakeMapWidget::startAnimation()
{
    m_animationEnabled = true;
    if (m_animationTimer && !m_animationTimer->isActive()) {
        m_animationTimer->start(1000 / ANIMATION_FPS);
    }
}

void EarthquakeMapWidget::stopAnimation()
{
    m_animationEnabled = false;
    if (m_animationTimer) {
        m_animationTimer->stop();
    }
}

void EarthquakeMapWidget::setAnimationSpeed(double speed)
{
    m_settings.animationSpeed = qBound(0.1, speed, 5.0);
    if (m_animationTimer) {
        int interval = int(1000.0 / (ANIMATION_FPS * m_settings.animationSpeed));
        m_animationTimer->setInterval(interval);
    }
}

void EarthquakeMapWidget::highlightEarthquake(const QString &eventId, int durationMs)
{
    QMutexLocker locker(&m_dataMutex);
    
    for (auto &eq : m_earthquakes) {
        if (eq.data.eventId == eventId) {
            eq.isHighlighted = true;
            break;
        }
    }
    
    update();
    
    if (durationMs > 0) {
        QTimer::singleShot(durationMs, [this, eventId]() {
            QMutexLocker locker(&m_dataMutex);
            for (auto &eq : m_earthquakes) {
                if (eq.data.eventId == eventId) {
                    eq.isHighlighted = false;
                    break;
                }
            }
            update();
        });
    }
}
