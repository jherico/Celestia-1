#include "CelestiaVrApplication.h"

#include <QtGui/QWindow>
#include <QtCore/QTimer>
#include <QtCore/QDateTime>

#include <vks/context.hpp>
#include <celapp/celestiacore.h>
#include <celutil/debug.h>

#include "Logging.h"

class AppProgressNotifier : public ProgressNotifier {
public:
    AppProgressNotifier() {}

    void update(const std::string& s) { qDebug() << s.c_str(); }
};

#define timezone 1
#define daylight 8
#define tzname "tzname"

CelestiaVrApplication::CelestiaVrApplication(int argc, char* argv[]) : QGuiApplication(argc, argv) {
    qInstallMessageHandler(messageHandler);
    setApplicationName("CelestiaVR");
    SetDebugVerbosity(5);

    _window = new QWindow();
    _window->show();
    _window->setIcon(QIcon(":/images/celestia.png"));

    _timer = new QTimer();
    _timer->setInterval(15);
    connect(_timer, &QTimer::timeout, this, &CelestiaVrApplication::onTimer);
    _timer->start();
    _vkContext = std::make_shared<vks::Context>();
    _celestiaCore = std::make_shared<CelestiaCore>();
    QObject::connect(this, &QGuiApplication::aboutToQuit, this, &CelestiaVrApplication::onAboutToQuit);
    //_celestiaCore->initSimulation("", {}, std::make_shared<AppProgressNotifier>());

    QDateTime now = QDateTime::currentDateTime();
    auto timespec = now.timeSpec();
    auto offset = now.offsetFromUtc();
    time_t curtime = now.toMSecsSinceEpoch();

    // Set up the default time zone name and offset from UTC
    time_t curtime = time(NULL);
    _celestiaCore->start(astro::UTCtoTDB((double)curtime / 86400.0 + (double)astro::Date(1970, 1, 1)));
    localtime(&curtime);  // Only doing this to set timezone as a side effect
    _celestiaCore->setTimeZoneBias(-timezone + 3600 * daylight);
    _celestiaCore->setTimeZoneName("temp");

    //// If LocalTime is set to false, set the time zone bias to zero.
    //if (settings.contains("LocalTime")) {
    //    bool useLocalTime = settings.value("LocalTime").toBool();
    //    if (!useLocalTime)
    //        _celestiaCore->setTimeZoneBias(0);
    //}

    //if (settings.contains("TimeZoneName")) {
    //    m_appCore->setTimeZoneName(settings.value("TimeZoneName").toString().toUtf8().data());
    //}

}



void CelestiaVrApplication::onTimer() {
    //_celestiaCore->tick();
}

void CelestiaVrApplication::onAboutToQuit() {
    _timer->stop();
}
