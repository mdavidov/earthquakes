// EarthquakeMainWindow.h
#pragma once

#include "earthquake_map_widget.hpp"

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QDockWidget>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QSlider>
#include <QtCore/QTimer>
#include <QtCore/QSettings>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtWidgets/QSystemTrayIcon>
#include <QtMultimedia/QSoundEffect>


class EarthquakeMainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit EarthquakeMainWindow(QWidget* parent = nullptr);
    ~EarthquakeMainWindow();

    void addEarthquake(const EarthquakeData& earthquake);
    void updateDataTimestamp();

protected:
    void closeEvent(QCloseEvent* event) override;

public slots:
    // Data management
    void fetchEarthquakeData();
    void onDataReceived(QNetworkReply* reply);
    void onNetworkError(int error);
    void refreshData();
    void exportData();
    void importData();
    
    // Map controls
    void onMapCenterChanged();
    void onZoomChanged(int value);
    void onFilterChanged();
    void resetMapView();
    void toggleFullscreen();
    
    // Earthquake list
    void onEarthquakeSelected();
    void onEarthquakeDoubleClicked();
    void sortEarthquakeList();
    
    // Alerts and notifications
    void checkForAlerts();
    void showAlert(const EarthquakeData &earthquake);
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void toggleAlertSound(bool enabled);
    
    // Settings and preferences
    void showSettingsDialog();
    void onSettingsChanged();
    void saveSettings();
    void loadSettings();
    
    // Help and about
    void showAboutDialog();
    void showHelpDialog();

private:
    void setupUI();
    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void setupDockWidgets();
    void setupSystemTray();
    void connectSignals();
    void setupControlPanels();
    void setupEarthquakeList();
    void setupDetailsPane();

    void updateEarthquakeList();
    void updateStatistics();
    void updateStatusBar();
    void applyFilters();
    
    QString formatMagnitude(double magnitude) const;
    QString formatDepth(double depth) const;
    QString formatTimeAgo(const QDateTime &timestamp) const;
    QString getAlertLevelText(int level) const;
    
    bool passesFilter(const EarthquakeData &eq) const;
    void playAlertSound(int alertLevel);
    
    // UI Components
    EarthquakeMapWidget* m_mapWidget;
    QSplitter* m_mainSplitter;
    QSplitter* m_rightSplitter;

    // Earthquake list and details
    QTableWidget* m_earthquakeTable;
    QTextEdit* m_detailsPane;
    QListWidget* m_alertsList;

    // Control panels
    QGroupBox* m_mapControlsGroup;
    QGroupBox* m_filterGroup;
    QGroupBox* m_alertGroup;
    
    // Map controls
    QDoubleSpinBox* m_latSpinBox;
    QDoubleSpinBox* m_lonSpinBox;
    QSlider* m_zoomSlider;
    QCheckBox* m_showGridCheckBox;
    QCheckBox* m_showLegendCheckBox;
    QPushButton* m_resetViewBtn;
    QPushButton* m_fullscreenBtn;

    // Filters
    QDoubleSpinBox* m_minMagnitudeSpinBox;
    QDoubleSpinBox* m_maxMagnitudeSpinBox;
    QSpinBox* m_maxAgeHoursSpinBox;
    QComboBox* m_alertLevelCombo;
    QCheckBox* m_recentOnlyCheckBox;

    // Alert settings
    QCheckBox* m_alertsEnabledCheckBox;
    QCheckBox* m_soundEnabledCheckBox;
    QComboBox* m_alertThresholdCombo;
    QProgressBar* m_alertIndicator;
    
    // Statistics display
    QLabel* m_totalEarthquakesLabel;
    QLabel* m_recentEarthquakesLabel;
    QLabel* m_highestMagnitudeLabel;
    QLabel* m_lastUpdateLabel;

    // Network and data
    QNetworkAccessManager* m_networkManager;
    QNetworkReply* m_currentReply;
    QTimer* m_refreshTimer;
    QTimer* m_alertTimer;

    // System tray and notifications
    QSystemTrayIcon* m_trayIcon;
    QSoundEffect* m_alertSound;

    // Data storage
    QVector<EarthquakeData> m_allEarthquakes;
    QVector<EarthquakeData> m_filteredEarthquakes;
    
    // Settings
    QSettings* m_settings;
    QString m_dataSourceUrl;
    int m_refreshIntervalMinutes;
    bool m_alertsEnabled;
    bool m_soundEnabled;
    double m_alertThreshold;
    bool m_startMinimized;
};

Q_DECLARE_METATYPE(EarthquakeMainWindow)
