#pragma once

#include <memory>

#include <QtGui/QGuiApplication>

namespace vks {
struct Context;
}
class CelestiaCore;

class CelestiaVrApplication : public QGuiApplication {
    Q_OBJECT

public:
    CelestiaVrApplication(int, char*[]);

private slots:
    void onTimer();
    void onAboutToQuit();

private:
    std::shared_ptr<vks::Context> _vkContext;
    std::shared_ptr<CelestiaCore> _celestiaCore;
    QWindow* _window{ nullptr };
    QTimer* _timer{ nullptr };
};
