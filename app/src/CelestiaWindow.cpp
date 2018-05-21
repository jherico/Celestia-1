#include "CelestiaWindow.h"

#include <QtGui/QMouseEvent>
#include <celapp/celestiacore.h>
#include "CelestiaVrApplication.h"

using namespace Eigen;

static Quaternionf mouseDeltaToRotation(const QPointF& point) {
    return Quaternionf(AngleAxisf(point.y() * -0.01f, Vector3f::UnitX()))  * Quaternionf(AngleAxisf(point.x() * 0.01f, Vector3f::UnitY()));
}

void CelestiaWindow::mouseMoveEvent(QMouseEvent* m) {
    Parent::mouseMoveEvent(m);
    QPointF deltaPos = m->windowPos() - _lastMouse;
    if (0 != (m->buttons() & Qt::MouseButton::LeftButton)) {
        Eigen::Quaternionf rotation = mouseDeltaToRotation(deltaPos);
        _celestiaCore->rotateObserver(rotation);
    }
    _lastMouse = m->windowPos();

#if 0
    int x = (int)m->x();
    int y = (int)m->y();

    int buttons = 0;
    if (m->buttons() & LeftButton)
        buttons |= CelestiaCore::LeftButton;
    if (m->buttons() & MidButton)
        buttons |= CelestiaCore::MiddleButton;
    if (m->buttons() & RightButton)
        buttons |= CelestiaCore::RightButton;
    if (m->modifiers() & ShiftModifier)
        buttons |= CelestiaCore::ShiftKey;
    if (m->modifiers() & ControlModifier)
        buttons |= CelestiaCore::ControlKey;

#ifdef TARGET_OS_MAC
    // On the Mac, right dragging is be simulated with Option+left drag.
    // We may want to enable this on other platforms, though it's mostly only helpful
    // for users with single button mice.
    if (m->modifiers() & AltModifier) {
        buttons &= ~CelestiaCore::LeftButton;
        buttons |= CelestiaCore::RightButton;
    }
#endif

    if ((m->buttons() & (LeftButton | RightButton)) != 0) {
        appCore->mouseMove(x - lastMouse.x, y - lastMouse.y, buttons);

        // Infinite mouse: allow the user to rotate and zoom continuously, i.e.,
        // without being stopped when the pointer reaches the window borders.
        QPoint pt;
        pt.setX(lastMouse.x);
        pt.setY(lastMouse.y);
        pt = mapToGlobal(pt);

        // First mouse drag event.
        // Hide the cursor and set its position to the center of the window.
        if (cursorVisible) {
            // Hide the cursor.
            setCursor(QCursor(Qt::BlankCursor));
            cursorVisible = false;

            // Save the cursor position.
            saveCursorPos = pt;

            // Compute the center point of the OpenGL Widget.
            QPoint center;
            center.setX(width() / 2);
            center.setY(height() / 2);

            // Set the cursor position to the center of the OpenGL Widget.
            x = center.rx() + (x - lastMouse.x);
            y = center.ry() + (y - lastMouse.y);
            lastMouse.x = (int)center.rx();
            lastMouse.y = (int)center.ry();

            center = mapToGlobal(center);
            QCursor::setPos(center);
        } else {
            if (x - lastMouse.x != 0 || y - lastMouse.y != 0)
                QCursor::setPos(pt);
        }
    } else {
        appCore->mouseMove(x, y);

        lastMouse.x = x;
        lastMouse.y = y;
    }
#endif
}

void CelestiaWindow::mousePressEvent(QMouseEvent* m) {
    Parent::mousePressEvent(m);
#if 0
    lastMouse.x = (int)m->x();
    lastMouse.y = (int)m->y();

    if (m->button() == LeftButton)
        appCore->mouseButtonDown(m->x(), m->y(), CelestiaCore::LeftButton);
    else if (m->button() == MidButton)
        appCore->mouseButtonDown(m->x(), m->y(), CelestiaCore::MiddleButton);
    else if (m->button() == RightButton)
        appCore->mouseButtonDown(m->x(), m->y(), CelestiaCore::RightButton);
#endif
}

void CelestiaWindow::mouseReleaseEvent(QMouseEvent* m) {
    Parent::mouseReleaseEvent(m);
#if 0
    if (m->button() == LeftButton) {
        if (!cursorVisible) {
            // Restore the cursor position and make it visible again.
            setCursor(QCursor(Qt::CrossCursor));
            cursorVisible = true;
            QCursor::setPos(saveCursorPos);
        }
        appCore->mouseButtonUp(m->x(), m->y(), CelestiaCore::LeftButton);
    } else if (m->button() == MidButton) {
        lastMouse.x = (int)m->x();
        lastMouse.y = (int)m->y();
        appCore->mouseButtonUp(m->x(), m->y(), CelestiaCore::MiddleButton);
    } else if (m->button() == RightButton) {
        if (!cursorVisible) {
            // Restore the cursor position and make it visible again.
            setCursor(QCursor(Qt::CrossCursor));
            cursorVisible = true;
            QCursor::setPos(saveCursorPos);
        }
        appCore->mouseButtonUp(m->x(), m->y(), CelestiaCore::RightButton);
    }
#endif
}
extern float fov;

void CelestiaWindow::wheelEvent(QWheelEvent* w) {
    Parent::wheelEvent(w);
    auto delta = w->delta();
        fov += (float)delta * 0.001f;
#if 0 
    if (w->delta() > 0) {
        appCore->mouseWheel(-1.0f, 0);
    } else if (w->delta() < 0) {
        appCore->mouseWheel(1.0f, 0);
    }
#endif
}
