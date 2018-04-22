#include <QtGui/QWindow>
#include <QtGui/QGuiApplication>
#include <QtCore/QDir>
#include <QtCore/QDebug>
#include <QtCore/QTimer>

#include <Windows.h>

#include <celutil/debug.h>
#include <celapp/celestiacore.h>

auto core = std::make_shared<CelestiaCore>();

#if defined(Q_OS_WIN)
#pragma comment(lib, "legacy_stdio_definitions.lib")
extern "C" {
FILE __iob_func[3] = { *stdin, *stdout, *stderr };
}
#endif

void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message) {
    const auto& logMessage = message;

    if (!logMessage.isEmpty()) {
#if defined(Q_OS_ANDROID)
        const char* local = logMessage.toStdString().c_str();
        switch (type) {
            case QtDebugMsg:
                __android_log_write(ANDROID_LOG_DEBUG, "Interface", local);
                break;
            case QtInfoMsg:
                __android_log_write(ANDROID_LOG_INFO, "Interface", local);
                break;
            case QtWarningMsg:
                __android_log_write(ANDROID_LOG_WARN, "Interface", local);
                break;
            case QtCriticalMsg:
                __android_log_write(ANDROID_LOG_ERROR, "Interface", local);
                break;
            case QtFatalMsg:
            default:
                __android_log_write(ANDROID_LOG_FATAL, "Interface", local);
                abort();
        }
#elif defined(Q_OS_WIN)
        OutputDebugStringA(message.toStdString().c_str());
        OutputDebugStringA("\n");
#endif
    }
}

// Progress notifier class receives update messages from CelestiaCore
// at startup. This simple implementation just forwards messages on
// to the main Celestia window.
class AppProgressNotifier : public ProgressNotifier {
public:
    AppProgressNotifier() {}

    void update(const std::string& s) { qDebug() << s.c_str(); }
};

QWindow* mainWindow{ nullptr };

void initApplication() {
}

void tick() {
    core->tick();
}

int main(int argc, char* argv[]) {
    QCoreApplication::setApplicationName("CelestiaVR");
    QGuiApplication application{ argc, argv };
    qInstallMessageHandler(messageHandler);
    QDir::setCurrent("C:/Users/bdavi/Git/celestia/resources/");
    SetDebugVerbosity(5);
    core->initSimulation("", {}, std::make_shared<AppProgressNotifier>());

    mainWindow = new QWindow();
    mainWindow->show();

    QTimer::singleShot(100, [] { mainWindow->setIcon(QIcon(":/icons/celestia.png")); });

    QTimer* timer = new QTimer(&application);
    QObject::connect(timer, &QTimer::timeout, [] { tick(); });
    timer->setInterval(10);
    timer->start();

    QObject::connect(&application, &QGuiApplication::aboutToQuit, [timer] { timer->stop(); });

    return application.exec();
}
