#include "CloseEventFilter.h"

#include <QtGui/QWindow>
#include <QtCore/QEvent>

CloseEventFilter::CloseEventFilter(QWindow* window) : QObject(window) {
    window->installEventFilter(this);
}

bool CloseEventFilter::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::Close) {
        emit closing();
    }

    return QObject::eventFilter(obj, event);
}
