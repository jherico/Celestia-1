#include "CelestiaWindow.h"

#include <QtGui/QMouseEvent>
#include <celapp/celestiacore.h>
#include "CelestiaVrApplication.h"
#include <glm/glm.hpp>

using namespace Eigen;

static const int screen_width = 800;
static const int screen_height = 600;

/**
* Get a normalized vector from the center of the virtual ball O to a
* point P on the virtual ball surface, such that P is aligned on
* screen's (X,Y) coordinates.  If (X,Y) is too far away from the
* sphere, return the nearest point on the virtual ball surface.
*/
glm::vec3 get_arcball_vector(const QPointF& point) {
    const auto x = point.x();
    const auto y = point.y();
    glm::vec3 P = glm::vec3(1.0f * x / screen_width * 2 - 1.0f, 1.0f * y / screen_height * 2 - 1.0f, 0);
    P.y = -P.y;
    float OP_squared = P.x * P.x + P.y * P.y;
    if (OP_squared <= 1 * 1)
        P.z = sqrt(1 * 1 - OP_squared);  // Pythagore
    else
        P = glm::normalize(P);  // nearest point
    return P;
}

static Quaternionf mouseDeltaToRotation(const QPointF& point, const Quaternionf& currentOrientation) {
    //static QPointF lastPoint{ screen_height / 2 , screen_width / 2 };
    //glm::vec3 va = get_arcball_vector(lastPoint);
    //glm::vec3 vb = get_arcball_vector(point);
    //float angle = acosf(std::min(1.0f, glm::dot(va, vb)));
    //glm::vec3 axis_in_camera_coord = glm::cross(va, vb);
    //glm::mat3 camera2object = glm::inverse(glm::mat3(transforms[MODE_CAMERA]) * glm::mat3(mesh.object2world));
    //glm::vec3 axis_in_object_coord = camera2object * axis_in_camera_coord;
    //mesh.object2world = glm::rotate(mesh.object2world, glm::degrees(angle), axis_in_object_coord);
    //lastPoint = point;
    Vector3f startZ = currentOrientation * -Vector3f::UnitZ();
    Vector3f newZ = startZ;
    auto yaw = Quaternionf(AngleAxisf(point.x() * 0.01f, Vector3f::UnitY()));
    newZ = yaw * newZ;
    auto pitch = Quaternionf(AngleAxisf(point.y() * -0.01f, Vector3f::UnitX()));
    newZ = pitch * newZ;
    return Quaternionf::FromTwoVectors(startZ, newZ).normalized();
}

void CelestiaWindow::mouseMoveEvent(QMouseEvent* m) {
    Parent::mouseMoveEvent(m);
    QPointF deltaPos = m->windowPos() - _lastMouse;
    if (0 != (m->buttons() & Qt::MouseButton::LeftButton)) {
        auto orientation = _celestiaCore->getSimulation()->getActiveObserver()->getOrientationf();
        Eigen::Quaternionf rotation = mouseDeltaToRotation(deltaPos, orientation);
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
