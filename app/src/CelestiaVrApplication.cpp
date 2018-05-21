#include "CelestiaVrApplication.h"

#include <mutex>

#include <QtCore/QDateTime>
#include <QtCore/QTimer>
#include <QtCore/QTimeZone>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>

#include <QtGui/QWindow>

#include <vks/context.hpp>
#include <celapp/celestiacore.h>
#include <celutil/debug.h>

#include "Logging.h"
#include "VulkanRenderer.h"
#include "CelestiaWindow.h"


class AppProgressNotifier : public ProgressNotifier {
public:
    AppProgressNotifier() {}

    void update(const std::string& s) { qDebug() << s.c_str(); }
};

//extern __stdcall void SetCurrentDirectoryA(const char*);
QString getResourceRoot() {
    static QString resourceRoot;
    static std::once_flag once;
    std::call_once(once, [&] {
        resourceRoot = QDir::cleanPath(QFileInfo(__FILE__).absoluteDir().absoluteFilePath("../../resources"));
    });
    return resourceRoot;
}


CelestiaVrApplication::CelestiaVrApplication(int argc, char* argv[]) : QGuiApplication(argc, argv) {
    QDir::setCurrent(getResourceRoot());
    qInstallMessageHandler(messageHandler);
    setApplicationName("CelestiaVR");
    SetDebugVerbosity(5);


    auto now = QDateTime::currentDateTime();
    auto timespec = now.timeSpec();
    auto tz = now.timeZone();
    auto timezoneBias = now.offsetFromUtc();
    double curtime = (double)now.toMSecsSinceEpoch() / 1000.0;
    auto curtimeUnix = time(nullptr);

    _celestiaCore = std::make_shared<CelestiaCore>();

    _window = new CelestiaWindow();
    _window->_celestiaCore = _celestiaCore;
    _window->setGeometry(100, 100, 800, 600);
    _window->show();
    _window->setIcon(QIcon(":/icons/celestia.png"));
    _celestiaCore->setRenderer(std::make_shared<VulkanRenderer>(_window));
    QObject::connect(this, &QGuiApplication::aboutToQuit, this, &CelestiaVrApplication::onAboutToQuit);
    _celestiaCore->initSimulation("", {}, std::make_shared<AppProgressNotifier>());
    // Set up the default time zone name and offset from UTC
    _celestiaCore->start(astro::UTCtoTDB(curtime / 86400.0 + (double)astro::Date(1970, 1, 1)));
    _celestiaCore->setTimeZoneBias(timezoneBias);
    _celestiaCore->setTimeZoneName(tz.abbreviation(now).toStdString());

    _timer = new QTimer();
    _timer->setInterval(15);
    connect(_timer, &QTimer::timeout, this, &CelestiaVrApplication::onTimer);
    _timer->start();
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
