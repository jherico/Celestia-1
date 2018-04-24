#include "Logging.h"

#if defined(Q_OS_ANDROID)
#include <android/log.h>
#endif

#if defined(Q_OS_WIN)
extern "C" {
    extern void __stdcall OutputDebugStringA(const char*);
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
