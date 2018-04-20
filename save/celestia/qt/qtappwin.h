// qtappwin.h
//
// Copyright (C) 2007-2008, Celestia Development Team
// celestia-developers@lists.sourceforge.net
//
// Main window for Celestia Qt front-end.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#ifndef _QTAPPWIN_H_
#define _QTAPPWIN_H_

#include <QtGui/QWindow>

#include "celestiacore.h"

class QCloseEvent;
class QUrl;

class CelestiaGlWidget;
class CelestialBrowser;
class InfoPanel;
class EventFinder;
class CelestiaActions;

class PreferencesDialog;
class BookmarkManager;
class BookmarkToolBar;

class CelestiaAppWindow : public QWindow {
    Q_OBJECT

public:
    CelestiaAppWindow(QWidget* parent = 0);
    ~CelestiaAppWindow();

    void init(const QString& configFileName, const QStringList& extrasDirectories);

    void readSettings();
    void writeSettings();
    bool loadBookmarks();
    void saveBookmarks();

    void contextMenu(float x, float y, Selection sel);

    void loadingProgressUpdate(const QString& s);

public slots:
    void celestia_tick();
    void slotShowSelectionContextMenu(const QPoint& pos, Selection& sel);
    void slotManual();

private slots:
    void slotGrabImage();
    void slotCaptureVideo();
    void slotCopyImage();
    void slotCopyURL();
    void slotPasteURL();

    void centerSelection();
    void gotoSelection();
    void selectSun();

    void slotPreferences();

    void slotSplitViewVertically();
    void slotSplitViewHorizontally();
    void slotCycleView();
    void slotSingleView();
    void slotDeleteView();
    void slotToggleFramesVisible();
    void slotToggleActiveFrameVisible();
    void slotToggleSyncTime();

    void slotShowObjectInfo(Selection& sel);

    void slotOpenScriptDialog();
    void slotOpenScript();

    void slotShowTimeDialog();

    void slotToggleFullScreen();

    void slotShowAbout();
    void slotShowGLInfo();

    void slotAddBookmark();
    void slotOrganizeBookmarks();
    void slotBookmarkTriggered(const QString& url);

    void handleCelUrl(const QUrl& url);

signals:
    void progressUpdate(const QString& s, int align, const QColor& c);

private:
    void initAppDataDirectory();

    void createActions();
    void createMenus();
    QMenu* buildScriptsMenu();
    void populateBookmarkMenu();

    void closeEvent(QCloseEvent* event);

private:
    CelestialBrowser* celestialBrowser;
    CelestiaCore* m_appCore;
    CelestiaActions* actions;

    InfoPanel* infoPanel;
    EventFinder* eventFinder;

    CelestiaCore::AlerterPtr alerter;

    PreferencesDialog* m_preferencesDialog;
    BookmarkManager* m_bookmarkManager;
    BookmarkToolBar* m_bookmarkToolBar;

    QString m_dataDirPath;
};

#endif  // _QTAPPWIN_H_
