#include "earthquake_application.hpp"
// #include "version.hpp"
#include <QApplication>
#include <QDebug>


int main(int argc, char *argv[])
{
    // Enable high DPI support before QApplication creation
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    EarthquakeApplication app(argc, argv);
    if (!app.initialize()) {
        qDebug() << "Application initialization failed";
        return 1;
    }
    qDebug() << "Application started successfully - entering event loop";

    const auto res = app.exec();

    // All cleanup is done in ~EarthquakeApplication destructor
    qDebug() << "Application exiting with code:" << res;
    return res;
}
