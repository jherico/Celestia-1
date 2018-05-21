#pragma once

#include <QtGui/QWindow>


class CelestiaCore;

class CelestiaWindow : public QWindow {
    using Parent = QWindow;

protected:
    void mouseMoveEvent(QMouseEvent* m) override;
    void mousePressEvent(QMouseEvent* m) override;
    void mouseReleaseEvent(QMouseEvent* m) override;
    void wheelEvent(QWheelEvent* w) override;

public:
    QPointF _lastMouse;
    std::shared_ptr<CelestiaCore> _celestiaCore;
};
