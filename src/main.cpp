
int main(int argc, char *argv[])
{
    // Enable high DPI support before QApplication creation
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    
    // Create application instance
    EarthquakeApplication app(argc, argv);
    
    // Setup signal handlers for graceful shutdown
#ifdef Q_OS_UNIX
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
#endif
    
    // Check for another instance already running
    QString lockFilePath = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + 
                          "/earthquake_alert_system.lock";
    QFile lockFile(lockFilePath);
    
    if (lockFile.exists()) {
        QMessageBox::information(nullptr, APP_NAME,
            "Another instance of " APP_NAME " is already running.\n"
            "Please close the existing instance before starting a new one.");
        return 1;
    }
    
    // Create lock file
    if (lockFile.open(QIODevice::WriteOnly)) {
        QTextStream stream(&lockFile);
        stream << QCoreApplication::applicationPid() << "\n";
        stream << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
        lockFile.close();
    }
    
    // Initialize application
    bool initSuccess = app.initialize();
    if (!initSuccess) {
        qCCritical(main) << "Application initialization failed";
        
        // Clean up lock file
        lockFile.remove();
        return 1;
    }
    
    qCInfo(main) << "Application started successfully - entering event loop";
    
    // Run application
    int result = app.exec();
    
    // Cleanup
    qCInfo(main) << "Application exiting with code:" << result;
    
    // Remove lock file
    lockFile.remove();
    
    return result;
}

// Include MOC file for Q_OBJECT in source file
#include "main.moc"
