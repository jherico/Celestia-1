#pragma once
#include <QtCore/QObject>

class QWindow;

class CloseEventFilter : public QObject {
    Q_OBJECT
public:
    CloseEventFilter(QWindow* parent);

signals:
    void closing();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
};
