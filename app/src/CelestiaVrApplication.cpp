#include "CelestiaVrApplication.h"

#include <QtCore/QDateTime>
#include <QtCore/QTimer>
#include <QtCore/QTimeZone>

#include <QtGui/QWindow>

#include <vks/context.hpp>
#include <celapp/celestiacore.h>
#include <celutil/debug.h>

#include "Logging.h"
#include "VulkanRenderer.h"

class AppProgressNotifier : public ProgressNotifier {
public:
    AppProgressNotifier() {}

    void update(const std::string& s) { qDebug() << s.c_str(); }
};

CelestiaVrApplication::CelestiaVrApplication(int argc, char* argv[]) : QGuiApplication(argc, argv) {
    qInstallMessageHandler(messageHandler);
    setApplicationName("CelestiaVR");
    SetDebugVerbosity(5);

    _timer = new QTimer();
    _timer->setInterval(15);
    connect(_timer, &QTimer::timeout, this, &CelestiaVrApplication::onTimer);
    _timer->start();

    auto now = QDateTime::currentDateTime();
    auto timespec = now.timeSpec();
    auto tz = now.timeZone();
    auto timezoneBias = now.offsetFromUtc();
    double curtime = (double)now.toMSecsSinceEpoch() / 1000.0;
    auto curtimeUnix = time(nullptr);

    _celestiaCore = std::make_shared<CelestiaCore>();

    _window = new QWindow();
    _window->setGeometry(100, 100, 800, 600);
    _window->show();
    _window->setIcon(QIcon(":/icons/celestia.png"));

    _celestiaCore->setRenderer(std::make_shared<VulkanRenderer>(_window));

    QObject::connect(this, &QGuiApplication::aboutToQuit, this, &CelestiaVrApplication::onAboutToQuit);
    _celestiaCore->initSimulation("", {}, std::make_shared<AppProgressNotifier>());
    qDebug() << "Init";

    // Set up the default time zone name and offset from UTC
    _celestiaCore->start(astro::UTCtoTDB(curtime / 86400.0 + (double)astro::Date(1970, 1, 1)));
    _celestiaCore->setTimeZoneBias(timezoneBias);
    _celestiaCore->setTimeZoneName(tz.abbreviation(now).toStdString());

}



void CelestiaVrApplication::onTimer() {
    if (_aboutToQuit) {
        return;
    }
    _celestiaCore->tick();
    _celestiaCore->render();
}

void CelestiaVrApplication::onAboutToQuit() {
    _aboutToQuit = true;
    _timer->stop();
    _celestiaCore->setRenderer(nullptr);
}
