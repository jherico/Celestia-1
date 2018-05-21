#pragma once

#include <memory>

#include <QtGui/QGuiApplication>

class Renderer;
class CelestiaCore;
class CelestiaWindow;

class CelestiaVrApplication : public QGuiApplication {
    Q_OBJECT

public:
    CelestiaVrApplication(int, char* []);

private slots:
    void onTimer();
    void onAboutToQuit();

private:
    bool _aboutToQuit{ false };
    std::shared_ptr<CelestiaCore> _celestiaCore;
    CelestiaWindow* _window{ nullptr };
    QTimer* _timer{ nullptr };
};
