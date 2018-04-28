#pragma once

#include <memory>

#include <QtGui/QGuiApplication>

class CelestiaCore;
class Renderer;

class CelestiaVrApplication : public QGuiApplication {
    Q_OBJECT

public:
    CelestiaVrApplication(int, char*[]);

private slots:
    void onTimer();
    void onAboutToQuit();

private:
    bool _aboutToQuit{ false };
    std::shared_ptr<CelestiaCore> _celestiaCore;
    QWindow* _window{ nullptr };
    QTimer* _timer{ nullptr };
};
