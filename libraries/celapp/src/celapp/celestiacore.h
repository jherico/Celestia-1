// celestiacore.h
//
// Copyright (C) 2001-2009, the Celestia Development Team
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#ifndef _CELESTIACORE_H_
#define _CELESTIACORE_H_

#include <memory>
// ---- Virtual Key -------
#define VK_0 0x30
#define VK_1 0x31
#define VK_2 0x32
#define VK_3 0x33
#define VK_4 0x34
#define VK_5 0x35
#define VK_6 0x36
#define VK_7 0x37
#define VK_8 0x38
#define VK_9 0x39
#define VK_A 0x41
#define VK_B 0x42
#define VK_C 0x43
#define VK_D 0x44
#define VK_E 0x45
#define VK_F 0x46
#define VK_G 0x47
#define VK_H 0x48
#define VK_I 0x49
#define VK_J 0x4A
#define VK_K 0x4B
#define VK_L 0x4C
#define VK_M 0x4D
#define VK_N 0x4E
#define VK_O 0x4F
#define VK_P 0x50
#define VK_Q 0x51
#define VK_R 0x52
#define VK_S 0x53
#define VK_T 0x54
#define VK_U 0x55
#define VK_V 0x56
#define VK_W 0x57
#define VK_X 0x58
#define VK_Y 0x59
#define VK_Z 0x5A
#define VK_RETURN 0x0D

#define VK_OEM_1 0xBA       // ';:' for US
#define VK_OEM_PLUS 0xBB    // '+' any country
#define VK_OEM_COMMA 0xBC   // ',' any country
#define VK_OEM_MINUS 0xBD   // '-' any country
#define VK_OEM_PERIOD 0xBE  // '.' any country
#define VK_OEM_2 0xBF       // '/?' for US

#define VK_OEM_4 0xDB  //  '[{' for US
#define VK_OEM_5 0xDC  //  '\|' for US
#define VK_OEM_6 0xDD  //  ']}' for US
#define VK_OEM_7 0xDE  //  ''"' for US
//--------------------------

#include <celutil/timer.h>
#include <celutil/watcher.h>
// #include <celutil/watchable.h>
#include <celengine/solarsys.h>
#include <celengine/texture.h>
#include <celengine/universe.h>
#include <celengine/simulation.h>

#include "configfile.h"
#include "favorites.h"
#include "destination.h"

#define MAX_CHANNELS 8

class Url;
using UrlPtr = std::shared_ptr<Url>;

// class CelestiaWatcher;
class CelestiaCore;

// class astro::Date;

using CelestiaWatcher = Watcher<CelestiaCore>;
//using CelestiaWatcherPtr = std::shared_ptr<CelestiaWatcher>;

class ProgressNotifier {
public:
    ProgressNotifier(){};
    virtual ~ProgressNotifier(){};

    virtual void update(const std::string&) = 0;
};

using ProgressNotifierPtr = std::shared_ptr<ProgressNotifier>;

class View;
using ViewPtr = std::shared_ptr<View>;

class View {
public:
    enum Type
    {
        ViewWindow = 1,
        HorizontalSplit = 2,
        VerticalSplit = 3,
    };

    View(Type, const ObserverPtr&, float, float, float, float);

    void mapWindowToView(float, float, float&, float&) const;

public:
    Type type;

    ObserverPtr observer;
    ViewPtr parent;
    ViewPtr child1;
    ViewPtr child2;
    float x;
    float y;
    float width;
    float height;
    int renderFlags{ 0 };
    int labelMode{ 0 };
    float zoom{ 1 };
    float alternateZoom{ 1 };

    void walkTreeResize(const ViewPtr&, int);
    bool walkTreeResizeDelta(const ViewPtr&, float, bool);
};

class CelestiaCore : public std::enable_shared_from_this<CelestiaCore>  // : public Watchable<CelestiaCore>
{
public:
    enum
    {
        LeftButton = 0x01,
        MiddleButton = 0x02,
        RightButton = 0x04,
        ShiftKey = 0x08,
        ControlKey = 0x10,
    };

    enum CursorShape
    {
        ArrowCursor = 0,
        UpArrowCursor = 1,
        CrossCursor = 2,
        InvertedCrossCursor = 3,
        WaitCursor = 4,
        BusyCursor = 5,
        IbeamCursor = 6,
        SizeVerCursor = 7,
        SizeHorCursor = 8,
        SizeBDiagCursor = 9,
        SizeFDiagCursor = 10,
        SizeAllCursor = 11,
        SplitVCursor = 12,
        SplitHCursor = 13,
        PointingHandCursor = 14,
        ForbiddenCursor = 15,
        WhatsThisCursor = 16,
    };

    enum
    {
        Joy_XAxis = 0,
        Joy_YAxis = 1,
        Joy_ZAxis = 2,
    };

    enum
    {
        JoyButton1 = 0,
        JoyButton2 = 1,
        JoyButton3 = 2,
        JoyButton4 = 3,
        JoyButton5 = 4,
        JoyButton6 = 5,
        JoyButton7 = 6,
        JoyButton8 = 7,
        JoyButtonCount = 8,
    };

    enum
    {
        Key_Left = 1,
        Key_Right = 2,
        Key_Up = 3,
        Key_Down = 4,
        Key_Home = 5,
        Key_End = 6,
        Key_PageUp = 7,
        Key_PageDown = 8,
        Key_Insert = 9,
        Key_Delete = 10,
        Key_F1 = 11,
        Key_F2 = 12,
        Key_F3 = 13,
        Key_F4 = 14,
        Key_F5 = 15,
        Key_F6 = 16,
        Key_F7 = 17,
        Key_F8 = 18,
        Key_F9 = 19,
        Key_F10 = 20,
        Key_F11 = 21,
        Key_F12 = 22,
        Key_NumPadDecimal = 23,
        Key_NumPad0 = 24,
        Key_NumPad1 = 25,
        Key_NumPad2 = 26,
        Key_NumPad3 = 27,
        Key_NumPad4 = 28,
        Key_NumPad5 = 29,
        Key_NumPad6 = 30,
        Key_NumPad7 = 31,
        Key_NumPad8 = 32,
        Key_NumPad9 = 33,
        Key_BackTab = 127,
        KeyCount = 128,
    };

    enum
    {
        LabelFlagsChanged = 1,
        RenderFlagsChanged = 2,
        VerbosityLevelChanged = 4,
        TimeZoneChanged = 8,
        AmbientLightChanged = 16,
        FaintestChanged = 32,
        HistoryChanged = 64,
        TextEnterModeChanged = 128,
        GalaxyLightGainChanged = 256,
    };

    enum
    {
        ShowNoElement = 0x001,
        ShowTime = 0x002,
        ShowVelocity = 0x004,
        ShowSelection = 0x008,
        ShowFrame = 0x010,
    };

    typedef void (*ContextMenuFunc)(float, float, Selection);

public:
    bool initSimulation(const std::string& = "",
                        const std::vector<std::string>& extrasDirs = {},
                        const ProgressNotifierPtr& progressNotifier = nullptr);
    void setRenderer(const RendererPtr& newRenderer);
    void start(double t);
    void getLightTravelDelay(double distance, int&, int&, float&);
    void setLightTravelDelay(double distance);

    // URLs and history navigation
    void setStartURL(std::string url);
    void goToUrl(const std::string& url);
    void addToHistory();
    void back();
    void forward();
    const std::vector<UrlPtr>& getHistory() const { return history; }
    std::vector<UrlPtr>::size_type getHistoryCurrent() const { return historyCurrent; }
    void setHistoryCurrent(std::vector<Url*>::size_type curr);
    void tick();
    void render();

    const SimulationPtr& getSimulation() const { return sim; }

    void readFavoritesFile();
    void writeFavoritesFile();
    void activateFavorite(FavoritesEntry&);
    void addFavorite(const std::string& name, const std::string& parentFolder) {
        addFavorite(name, parentFolder, favorites.end());
    }
    void addFavorite(const std::string& name, const std::string& parentFolder, const FavoritesList::const_iterator& after);
    void addFavoriteFolder(const std::string& name) { addFavoriteFolder(name, favorites.end()); }
    void addFavoriteFolder(const std::string& name, const FavoritesList::const_iterator& after);
    const FavoritesList& getFavorites() { return favorites; }
    const DestinationList& getDestinations() { return destinations; }

    int getTimeZoneBias() const { return timeZoneBias; }
    void setTimeZoneBias(int);
    const std::string& getTimeZoneName() const { return timeZoneName; }
    void setTimeZoneName(const std::string& zone) { timeZoneName = zone; }

    int getHudDetail() { return hudDetail; }
    void setHudDetail(int);
    astro::Date::Format getDateFormat() const { return dateFormat; }
    void setDateFormat(astro::Date::Format format);

    void setContextMenuCallback(ContextMenuFunc callback) { contextMenuCallback = callback; }

    void addWatcher(CelestiaWatcher*);
    void removeWatcher(CelestiaWatcher*);
    /// Set the faintest visible star magnitude; adjust the renderer's
    /// brightness parameters appropriately.
    void setFaintest(float magnitude) { sim->setFaintestVisible(magnitude); }
    void setFaintestAutoMag();
    bool getActiveFrameVisible() const { return showActiveViewFrame; }
    void setActiveFrameVisible(bool visible) { showActiveViewFrame = visible; }
    bool getLightDelayActive() const { return lightTravelFlag; }
    void setLightDelayActive(bool lightDelayActive) { lightTravelFlag = lightDelayActive; }
    bool getAltAzimuthMode() const { return altAzimuthMode; }
    void setAltAzimuthMode(bool enable) { altAzimuthMode = enable; }

    const CelestiaConfigPtr& getConfig() const { return config; }

    void notifyWatchers(int);

    class Alerter {
    public:
        virtual ~Alerter(){};
        virtual void fatalError(const std::string&) = 0;
    };

    using AlerterPtr = std::shared_ptr<Alerter>;

    void setAlerter(const AlerterPtr& a) { alerter = a; }

    const AlerterPtr& getAlerter() const { return alerter; }

    void toggleReferenceMark(const std::string& refMark, Selection sel = {});
    bool referenceMarkEnabled(const std::string& refMark, Selection sel = {}) const;

    void rotateObserver(const Eigen::Quaternionf& rotation);

private:
    bool readStars(const CelestiaConfig&, const ProgressNotifierPtr&);
    void fatalError(const std::string&);

private:
    CelestiaConfigPtr config;

    UniversePtr universe;

    FavoritesList favorites;
    DestinationList destinations;
    SimulationPtr sim;
    RendererPtr renderer;
    int width{ 1 };
    int height{ 1 };

    std::string messageText;
    int messageHOrigin{ 0 };
    int messageVOrigin{ 0 };
    int messageHOffset{ 0 };
    int messageVOffset{ 0 };
    double messageStart{ 0 };
    double messageDuration{ 0 };

    double imageStart{ 0 };
    double imageDuration{ 0 };
    float imageXoffset{ 0 };
    float imageYoffset{ 0 };
    float imageAlpha{ 0 };
    int imageFitscreen{ 0 };
    std::string scriptImageFilename;

    std::string typedText;
    std::vector<std::string> typedTextCompletion;
    int typedTextCompletionIdx{ -1 };
    int hudDetail{ 2 };
    astro::Date::Format dateFormat{ astro::Date::Locale };
    int dateStrWidth{ 0 };
    int overlayElements{ ShowTime | ShowVelocity | ShowSelection | ShowFrame };
    bool wireframe{ false };
    bool editMode{ false };
    bool altAzimuthMode{ false };
    bool showConsole{ false };
    bool lightTravelFlag{ false };
    double flashFrameStart{ 0 };

    TimerPtr timer{ CreateTimer() };

    int timeZoneBias{ 0 };     // Diff in secs between local time and GMT
    std::string timeZoneName;  // Name of the current time zone

    double sysTime{ 0 };
    double currentTime{ 0 };
    bool viewChanged{ true };
    ContextMenuFunc contextMenuCallback{ nullptr };
    AlerterPtr alerter;

    std::vector<CelestiaWatcher*> watchers;
    std::vector<UrlPtr> history;
    std::vector<UrlPtr>::size_type historyCurrent{ 0 };
    std::string startURL;

    std::list<ViewPtr> views;
    std::list<ViewPtr>::iterator activeView{ views.begin() };
    bool showActiveViewFrame{ false };
    ViewPtr resizeSplit;

    Selection lastSelection;
    std::string selectionNames;


public:
    void setScriptImage(double, float, float, float, const std::string&, int);
};

#endif  // _CELESTIACORE_H_
