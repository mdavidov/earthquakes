
#include "earthquake_application.hpp"

EarthquakeApplication::EarthquakeApplication(int argc, char* argv[])
    : QApplication(argc, argv)
    , m_mainWindow(nullptr)
    , m_apiClient(nullptr)
    , m_notificationManager(nullptr)
    , m_splashScreen(nullptr)
    , m_initializationComplete(false)
{
    qRegisterMetaType<EarthquakeApplication>("EarthquakeApplication");
    setupApplication();
    parseCommandLine();
    setupLogging();
    setupApplicationPaths();
    checkSystemRequirements();
    setupSplashScreen();
    initializeComponents();
    // connectSignals();
}
