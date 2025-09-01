
#pragma once
#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QTimer>
#include <set>
#include "earthquake_data.hpp"

class NotificationManager : public QObject {
    Q_OBJECT
    
public:
    explicit NotificationManager(QObject* parent = nullptr);
    
    void setMagnitudeThreshold(double threshold);
    void setNotificationsEnabled(bool enabled);
    void checkForAlerts(const std::vector<EarthquakeData>& earthquakes);
    
signals:
    void alertTriggered(const EarthquakeData& earthquake);
    
private slots:
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void clearNotifiedEarthquakes();
    
private:
    QSystemTrayIcon* trayIcon;
    QMenu* trayMenu;
    QAction* toggleAction;
    QTimer* cleanupTimer;
    
    double magnitudeThreshold;
    bool notificationsEnabled;
    std::set<QString> notifiedEarthquakes;
    
    void setupTrayIcon();
    void showEarthquakeNotification(const EarthquakeData& earthquake);
};
