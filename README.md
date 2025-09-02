# Earthquakes Alert System

This project is an Earthquakes Alert System that monitors earthquake data from the USGS (United States Geological Survey) and sends email alerts for significant earthquakes based on user-defined criteria.

## Core Features

### Data Management

* Real-time data fetching from USGS earthquake feeds
* JSON parsing of earthquake data with error handling
* Data export to CSV and JSON formats
* Import capabilities for external data sources
* EarthquakeData: Structured data class for earthquake information
* EarthquakeDatabase: SQLite integration with optimized queries and indexing
* Spatial indexing: High-performance spatial queries for large datasets

### USGS API Integration

* EarthquakeApiClient: Fetches real-time data from USGS GeoJSON API
* GeoJsonParser: Efficient parsing with performance profiling
* Auto-refresh: Configurable automatic data updates

### User Interface

* Modern Qt6 interface with dark theme
* Real-time earthquake table with magnitude-based color coding
* Advanced filtering: By magnitude, time window, and geographic region
* Interactive details: Double-click for earthquake information
* Pulsing animation for recent earthquakes (within the last hour)
* Grid overlay with latitude/longitude lines
* Interactive legend showing magnitude and alert level scales
* Mouse interaction for panning and zooming

### Alert System

* NotificationManager: System tray notifications for significant earthquakes
* Configurable thresholds: Set minimum magnitude for alerts
* Duplicate prevention: Tracks already-notified earthquakes

### Performance & Profiling

* Profiler utilities: Macro-based timing for GeoJSON parsing and spatial queries
* Spatial optimization: Grid-based indexing for fast region queries
* Memory efficiency: Optimized data structures and database transactions

## Advanced Features

### Geographic Regions

Pre-configured regions (California, Alaska, Japan, Chile, Indonesia) with precise bounding boxes

### Spatial Utilities

* Haversine distance calculations
* Radius-based earthquake searches
* Earthquake density analysis
* Nearest earthquake finder

### Database Optimization

* Indexed queries on magnitude, timestamp, and location
* Batch insertions with transactions
* Automatic cleanup of old records

## Modern C++ Features

* C++20 standard with smart pointers and RAII
* Template-based profiling system
* Structured bindings and constexpr optimizations
* Exception safety throughout

## Build System

* Complete CMakeLists.txt and .pro files for both CMake and qmake build systems.
* Performance Profiling
* The system includes comprehensive profiling for:

## GeoJSON parsing performance comparison

* Spatial index building and querying
* Database operation timing
* Memory usage optimization

This is a production-ready earthquake monitoring system that can handle real-time data streams, provide instant notifications for significant seismic events, and efficiently query large datasets using advanced spatial indexing techniques.

## SpatialUtils

A comprehensive utility class for spatial calculations and earthquake-specific computations:

### Core Functions

* Distance calculations using Haversine formula for accurate Earth distances
* Coordinate transformations including Mercator projection
* Seismic calculations for shake intensity, energy, and wave arrival times
* Spatial clustering for grouping nearby earthquakes
* Geometric utilities for bearing, destination points, and polygon operations

### Key Earthquake Features

* Estimates shake intensity based on magnitude and distance
* Calculates P-wave and S-wave arrival times
* Converts to Mercalli intensity scale
* Provides spatial clustering for earthquake swarm detection

### Key Design Features

* Real-time Visualization: The map widget uses smooth animations and color coding to show earthquake severity and recency.
* Scientific Accuracy: Distance calculations use proper spherical geometry, and seismic estimates follow established geological formulas.
* Interactive Controls: Full mouse support for panning and zooming, with proper coordinate transformations.
* Extensible Design: Easy to add new earthquake data sources and customize visualization parameters.
* Performance Optimized: Uses efficient rendering techniques and caches map data where possible.

## Advanced Functionality

### Interactive Map Controls

* Pan and zoom with coordinate display
* Center point adjustment via latitude/longitude spinboxes
* Grid and legend toggle options
* Fullscreen mode for detailed analysis

### Smart Filtering System

* Magnitude range filtering (min/max)
* Time-based filtering (age in hours, recent-only option)
* Alert level filtering (Info through Critical)
* Real-time filter application with instant visual updates

### Alert & Notification System

* Configurable alert thresholds based on magnitude
* System tray integration with notifications
* Sound alerts with volume scaling by severity
* Visual alert indicators with color-coded status
* Alert history list maintaining recent alerts

### Professional Features

* Settings persistence using QSettings
* Window state restoration (geometry, dock positions)
* System tray minimization for background monitoring
* Comprehensive help system with user documentation
* Network error handling with user feedback

### Technical Highlights

* Modular Design: Clean separation between UI, data management, and visualization
* Thread Safety: Proper network request handling with cancellation support
* Resource Management: Efficient memory usage with smart data filtering
* User Experience: Intuitive interface with comprehensive keyboard shortcuts
* Cross-Platform: Uses Qt's cross-platform APIs for Windows, macOS, and Linux

## Earthquake API Client

### Multi-Source API Support

* USGS (United States Geological Survey) - Multiple feed types (hourly, daily, weekly, monthly, significant)
* EMSC (European-Mediterranean Seismological Centre) - European earthquake data
* JMA (Japan Meteorological Agency) - Japanese earthquake data
* Custom API endpoints - Configurable for additional data sources

### Advanced Request Management

* Asynchronous request processing with queue management
* Rate limiting to respect API quotas (configurable calls per minute)
* Automatic retry logic with exponential backoff for failed requests
* Request timeout handling with configurable timeout periods
* Concurrent request limiting to prevent overwhelming APIs

### Smart Caching System

* Response caching with configurable expiry times
* Cache size management with automatic cleanup of old entries
* Cache validation to ensure fresh data when needed
* Memory-efficient storage of API responses

### Flexible Data Fetching

* Geographic filtering by latitude/longitude bounds
* Magnitude filtering with min/max thresholds
* Time range queries for historical data analysis
* Specific event lookup by earthquake ID
* Recent earthquake monitoring with customizable time windows

### Robust Error Handling

* Network error detection and recovery
* SSL error management for secure connections
* JSON parsing validation with detailed error reporting
* Data validation to ensure earthquake data integrity
* Connection status monitoring with automatic reconnection

### Professional API Features

* Configurable API keys for authenticated services
* Custom user agent strings for API identification
* Request logging for debugging and monitoring
* Statistics tracking (success/failure rates, request counts)
* Auto-refresh capability for real-time monitoring

### Key Technical Highlights

* Thread Safety: Uses QMutex for safe concurrent access to request queues
* Memory Management: Smart pointer usage and proper cleanup of network replies
* Signal-Slot Architecture: Qt-native event handling for seamless integration
* Modular Design: Easy to extend with additional earthquake data sources
* Production Ready: Comprehensive error handling and logging for deployment

## Notification Manager

Multi-Channel Notification System:

* System Tray Integration - Native desktop notifications with context menu
* Desktop Notifications - Cross-platform notification display
* Sound Alerts - Configurable audio alerts with different sound types
* Email Notifications - SMTP integration for email alerts
* SMS Alerts - Integration with SMS services (Twilio, etc.)
* Push Notifications - Mobile/web push notification support
* File Logging - Persistent notification logging
* Console Output - Debug and development logging

Intelligent Alert Rules System:

* Magnitude-Based Rules - Configure alerts based on earthquake magnitude
* Geographic Rules - Region-specific and proximity-based alerts
* Depth Filtering - Shallow vs deep earthquake differentiation
* Time-Based Cooldowns - Prevent notification spam
* Priority Levels - Emergency, Critical, High, Normal, Low classifications
* Custom Messages - Template-based notification content

Advanced Management:

* Rate Limiting - Configurable maximum notifications per hour
* Quiet Hours - Automatic muting during specified time periods
* Notification Grouping - Similar events grouped to reduce noise
* Acknowledgment System - Mark notifications as read/handled
* Persistent Storage - Critical alerts survive application restarts
* Expiry Management - Automatic cleanup of old notifications

### Professional Features of Notification Manager

### User Experience

* Location-Based Alerts - Proximity alerts based on user's location
* Sound Customization - Different alert tones for different severity levels
* Visual Indicators - System tray icons and tooltips show notification status
* Statistics Tracking - Daily counters and delivery success rates

### Developer Integration

* Signal-Slot Architecture - Qt-native event system for seamless integration
* Thread Safety - Mutex protection for concurrent access
* Error Handling - Comprehensive error reporting and retry logic
* Extensible Design - Easy to add new notification channels

### Production Ready

* Settings Persistence - All preferences saved automatically
* Network Integration - HTTP-based external notification services
* Resource Management - Efficient memory usage and cleanup
* Logging System - Detailed logs for debugging and monitoring

### Key Technical Highlights of Notification Manager

* Modular Channel System: Each notification channel is implemented separately, allowing easy extension
* Smart Rule Evaluation: Complex logic for determining when and how to notify users
* Robust Error Handling: Graceful degradation when notification channels fail
* Memory Efficient: Automatic cleanup and size limits prevent memory bloat
* User-Centric Design: Respects user preferences like quiet hours and notification limits

## Earthquake Application and main()

### Key Features

### Professional Application Setup

* Complete Qt Application initialization with proper metadata, icons, and styling
* Command-line argument parsing with comprehensive options (debug, offline, location, etc.)
* High DPI scaling support for modern displays
* Dark theme support with professional styling
* Internationalization setup for multi-language support

### Robust Initialization System

* Splash screen with progress indicators and branding
* Component initialization with proper error handling and rollback
* System requirements checking (SSL, network, system tray availability)
* Resource verification to ensure all required files are present
* Application paths setup for data, logs, cache directories

### Advanced Application Management

* Single instance enforcement with lock file mechanism
* Graceful signal handling for clean shutdown (SIGINT, SIGTERM)
* Comprehensive logging system with configurable levels and file output
* Network connectivity testing before starting data operations
* Memory and disk space monitoring

### Component Integration

* Seamless connection between EarthquakeMainWindow, EarthquakeApiClient, and NotificationManager
* Event-driven architecture using Qt's signal-slot system
* Proper data flow from API → Main Window → Notifications
* Error propagation and handling across all components

### Development & Production Features

### Command Line Options

* --debug - Enable verbose logging and debug features
* --offline - Start in offline mode without network requests
* --minimized - Start minimized to system tray
* --no-splash - Skip splash screen for faster startup
* --location lat,lon - Set user location for proximity alerts
* --config file - Use custom configuration file
* --log-level - Control logging verbosity

### Advanced Capabilities

* Crash handling with automatic crash logs
* Update checking against remote version server
* Desktop integration (shortcuts, file associations)
* System information logging for troubleshooting
* Resource management with automatic cleanup

### Build System Support

The code includes complete examples for both CMake and qmake build systems:

### CMake Features

* Cross-platform configuration
* Automatic Qt component detection
* Platform-specific packaging (NSIS, DEB, RPM, DMG)
* Installation rules for desktop integration

### qmake Features

* Traditional Qt project file
* Platform-specific settings
* Debug/Release configurations
* Installation targets for Linux

### Professional Deployment

The main.cpp includes all necessary components for professional software deployment:

* Version Management - Centralized version information and update checking
* Error Handling - Comprehensive error reporting with user-friendly messages
* Resource Management - Automatic creation of required directories and files
* System Integration - Desktop shortcuts, file associations, system tray
* Logging Infrastructure - Production-ready logging with rotation and levels
* Security Features - Single instance checking and graceful shutdown handling

## Earthquake Map Widget implementation

Complete Feature Set:
Advanced Interactive Mapping:

Multi-projection support - Mercator, Equirectangular, Orthographic (North/South pole), Robinson
Smooth navigation - Pan with mouse drag, zoom with wheel, keyboard controls
Professional interactions - Context menus, tooltips, rubber band selection
Animated transitions - Smooth animated movement to locations with easing curves

Sophisticated Earthquake Visualization:

Multiple display modes - Circles, squares, diamonds, crosses, heatmap visualization
Dynamic color schemes - Color by magnitude, depth, age, alert level, or data source
Smart clustering - Automatic grouping of nearby earthquakes with expandable clusters
Real-time animations - Pulse, ripple, fade, grow, shake effects for recent earthquakes
Adaptive sizing - Logarithmic earthquake sizing that scales with zoom level

Professional Rendering Engine:

High-performance caching - Background and layer caching for smooth interaction
Level-of-detail optimization - Automatic quality adjustment based on zoom and data density
Smart culling - Only renders visible elements for optimal performance
Anti-aliased graphics - Professional quality rendering with smooth edges
Thread-safe operations - Mutex-protected data access for real-time updates

Rich User Interface:

Interactive legend - Dynamic legend showing magnitude scale, statistics, and color scheme
Coordinate display - Real-time lat/lon coordinates at cursor position
Scale bar - Adaptive scale indicator with appropriate distance units
Selection tools - Single-click, multi-select with Ctrl/Shift, rubber band selection
Comprehensive tooltips - Detailed earthquake information on hover

Advanced Data Management:

Real-time updates - Add, remove, update earthquakes without interruption
Smart filtering - Filter by magnitude, depth, time range, geographic bounds
Spatial indexing - Efficient hit-testing and spatial queries
Export capabilities - High-quality image export in multiple formats
Settings persistence - Automatic save/restore of all user preferences

Professional Development Features:

Debug overlay - Development information display (Ctrl+hover)
Performance monitoring - Built-in performance optimization and monitoring
Extensible architecture - Easy to add new map layers, projections, and display modes
Memory efficient - Smart cache management and resource cleanup
Cross-platform - Native Qt implementation works on Windows, macOS, Linux

Integration Ready:
The widget integrates seamlessly with the other system components:

Signal/slot architecture - Qt-native event system for clean integration
Thread-safe API - Safe to call from any thread with mutex protection
Settings integration - Works with QSettings for preference persistence
Network ready - Built-in support for downloading map tiles and data
Animation system - Smooth 30 FPS animations with configurable timing

This EarthquakeMapWidget provides a professional-grade mapping component that rivals commercial GIS applications while being specifically optimized for earthquake data visualization. It handles everything from small datasets to thousands of earthquakes with smooth performance and rich interactivity.

