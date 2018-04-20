// celestiacore.cpp
//
// Platform-independent UI handling and initialization for Celestia.
// winmain, gtkmain, and glutmain are thin, platform-specific modules
// that sit directly on top of CelestiaCore and feed it mouse and
// keyboard events.  CelestiaCore then turns those events into calls
// to Renderer and Simulation.
//
// Copyright (C) 2001-2009, the Celestia Development Team
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#include "celestiacore.h"

#include <cstdio>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <cstdlib>
#include <cctype>
#include <cstring>
#include <cassert>
#include <ctime>

#include <celutil/util.h>
#include <celutil/filetype.h>
#include <celutil/directory.h>
#include <celutil/formatnum.h>
#include <celutil/debug.h>
#include <celutil/utf8.h>
#include <celmath/geomutil.h>
#include <celastro/astro.h>
#include <celengine/asterism.h>
#include <celengine/boundaries.h>
#include <celengine/multitexture.h>
#include <celephem/spiceinterface.h>
#include <celengine/visibleregion.h>
#include <celengine/eigenport.h>
#include <celengine/render.h>
#include <celengine/axisarrow.h>
#include <celengine/planetgrid.h>

#include "favorites.h"
#include "url.h"

#ifdef _WIN32
#define TIMERATE_PRINTF_FORMAT "%.12g"
#else
#define TIMERATE_PRINTF_FORMAT "%'.12g"
#endif

using namespace Eigen;
using namespace std;

// Perhaps you'll want to put this stuff in configuration file.
static const double CoarseTimeScaleFactor = 10.0;
static const double FineTimeScaleFactor = 2.0;
static const double fMinSlewRate = 3.0;
static const double fMaxKeyAccel = 20.0;
static const float fAltitudeThreshold = 4.0f;
static const float RotationBraking = 10.0f;
static const float RotationDecay = 2.0f;
static const double MaximumTimeRate = 1.0e15;
static const double MinimumTimeRate = 1.0e-15;

static void warning(string s) {
    cout << s;
}

struct OverlayImage {
    Texture* texture;
    int xSize;
    int ySize;
    int left;
    int bottom;
};

vector<OverlayImage> overlayImages;

// If right dragging to rotate, adjust the rotation rate based on the
// distance from the reference object.  This makes right drag rotation
// useful even when the camera is very near the surface of an object.
// Disable adjustments if the reference is a deep sky object, since they
// have no true surface (and the observer is likely to be inside one.)
float ComputeRotationCoarseness(Simulation& sim) {
    float coarseness = 1.5f;

    Selection selection = sim.getActiveObserver()->getFrame()->getRefObject();
    if (selection.getType() == Selection::Type_Star || selection.getType() == Selection::Type_Body) {
        double radius = selection.radius();
        double t = sim.getTime();
        UniversalCoord observerPosition = sim.getActiveObserver()->getPosition();
        UniversalCoord selectionPosition = selection.getPosition(t);
        double distance = observerPosition.distanceFromKm(selectionPosition);
        double altitude = distance - radius;
        if (altitude > 0.0 && altitude < radius) {
            coarseness *= (float)max(0.01, altitude / radius);
        }
    }

    return coarseness;
}

View::View(View::Type _type, const ObserverPtr& _observer, float _x, float _y, float _width, float _height) :
    type(_type), observer(_observer), x(_x), y(_y), width(_width), height(_height) {
}

void View::mapWindowToView(float wx, float wy, float& vx, float& vy) const {
    vx = (wx - x) / width;
    vy = (wy + (y + height - 1)) / height;
    vx = (vx - 0.5f) * (width / height);
    vy = 0.5f - vy;
}

void View::walkTreeResize(const ViewPtr& sibling, int sign) {
    float ratio;
    switch (parent->type) {
        case View::HorizontalSplit:
            ratio = parent->height / (parent->height - height);
            sibling->height *= ratio;
            if (sign == 1) {
                sibling->y = parent->y + (sibling->y - parent->y) * ratio;
            } else {
                sibling->y = parent->y + (sibling->y - (y + height)) * ratio;
            }
            break;

        case View::VerticalSplit:
            ratio = parent->width / (parent->width - width);
            sibling->width *= ratio;
            if (sign == 1) {
                sibling->x = parent->x + (sibling->x - parent->x) * ratio;
            } else {
                sibling->x = parent->x + (sibling->x - (x + width)) * ratio;
            }
            break;
        case View::ViewWindow:
            break;
    }
    if (sibling->child1)
        walkTreeResize(sibling->child1, sign);
    if (sibling->child2)
        walkTreeResize(sibling->child2, sign);
}

bool View::walkTreeResizeDelta(const ViewPtr& v, float delta, bool check) {
    auto p = v;
    int sign = -1;
    float ratio;
    double newSize;

    if (v->child1) {
        if (!walkTreeResizeDelta(v->child1, delta, check))
            return false;
    }

    if (v->child2) {
        if (!walkTreeResizeDelta(v->child2, delta, check))
            return false;
    }

    while (p != child1 && p != child2 && (p = p->parent))
        ;
    if (p == child1)
        sign = 1;
    switch (type) {
        case View::HorizontalSplit:
            delta = -delta;
            ratio = (p->height + sign * delta) / p->height;
            newSize = v->height * ratio;
            if (newSize <= .1)
                return false;
            if (check)
                return true;
            v->height = (float)newSize;
            if (sign == 1) {
                v->y = p->y + (v->y - p->y) * ratio;
            } else {
                v->y = p->y + delta + (v->y - p->y) * ratio;
            }
            break;

        case View::VerticalSplit:
            ratio = (p->width + sign * delta) / p->width;
            newSize = v->width * ratio;
            if (newSize <= .1)
                return false;
            if (check)
                return true;
            v->width = (float)newSize;
            if (sign == 1) {
                v->x = p->x + (v->x - p->x) * ratio;
            } else {
                v->x = p->x + delta + (v->x - p->x) * ratio;
            }
            break;
        case View::ViewWindow:
            break;
    }

    return true;
}

CelestiaCore::CelestiaCore() {
}

CelestiaCore::~CelestiaCore() {
}

void CelestiaCore::readFavoritesFile() {
    // Set up favorites list
    if (config->favoritesFile != "") {
        ifstream in(config->favoritesFile.c_str(), ios::in);

        if (in.good()) {
            if (!ReadFavoritesList(in, favorites)) {
                warning(_("Error reading favorites file."));
            }
        }
    }
}

void CelestiaCore::writeFavoritesFile() {
    if (config->favoritesFile != "") {
        ofstream out(config->favoritesFile.c_str(), ios::out);
        if (out.good())
            WriteFavoritesList(favorites, out);
    }
}

void CelestiaCore::activateFavorite(FavoritesEntry& fav) {
    sim->cancelMotion();
    sim->setTime(fav.jd);
    sim->setObserverPosition(fav.position);
    sim->setObserverOrientation(fav.orientation);
    sim->setSelection(sim->findObjectFromPath(fav.selectionName));
    sim->setFrame(fav.coordSys, sim->getSelection());
}

void CelestiaCore::addFavorite(string name, string parentFolder) {
    addFavorite(name, parentFolder, favorites.end());
}

void CelestiaCore::addFavorite(string name, string parentFolder, const FavoritesList::const_iterator& pos) {
    FavoritesEntry fav;
    fav.jd = sim->getTime();
    fav.position = sim->getObserver().getPosition();
    fav.orientation = sim->getObserver().getOrientationf();
    fav.name = name;
    fav.isFolder = false;
    fav.parentFolder = parentFolder;

    Selection sel = sim->getSelection();
    if (sel.deepsky() != NULL)
        fav.selectionName = sim->getUniverse()->getDSOCatalog()->getDSOName(sel.deepsky());
    else
        fav.selectionName = sel.getName();

    fav.coordSys = sim->getFrame()->getCoordinateSystem();

    favorites.insert(pos, fav);
}

void CelestiaCore::addFavoriteFolder(string name) {
    addFavoriteFolder(name, favorites.end());
}

void CelestiaCore::addFavoriteFolder(string name, const FavoritesList::const_iterator& pos) {
    FavoritesEntry fav;
    fav.name = name;
    fav.isFolder = true;
    favorites.insert(pos, fav);
}

void CelestiaCore::getLightTravelDelay(double distanceKm, int& hours, int& mins, float& secs) {
    // light travel time in hours
    double lt = distanceKm / (3600.0 * astro::speedOfLight);
    hours = (int)lt;
    double mm = (lt - hours) * 60;
    mins = (int)mm;
    secs = (float)((mm - mins) * 60);
}

void CelestiaCore::setLightTravelDelay(double distanceKm) {
    // light travel time in days
    double lt = distanceKm / (86400.0 * astro::speedOfLight);
    sim->setTime(sim->getTime() - lt);
}

bool CelestiaCore::getAltAzimuthMode() const {
    return altAzimuthMode;
}

void CelestiaCore::setAltAzimuthMode(bool enable) {
    altAzimuthMode = enable;
}

void CelestiaCore::start(double t) {
#if 0
    if (config->initScriptFile != "") {
        // using the KdeAlerter in runScript would create an infinite loop,
        // break it here by resetting config->initScriptFile:
        string filename = config->initScriptFile;
        config->initScriptFile = "";
        runScript(filename);
    }
#endif

    // Set the simulation starting time to the current system time
    sim->setTime(t);
    sim->update(0.0);

    sysTime = timer->getTime();

    if (startURL != "")
        goToUrl(startURL);
}

void CelestiaCore::setStartURL(string url) {
    if (!url.substr(0, 4).compare("cel:")) {
        startURL = url;
        config->initScriptFile = "";
    } else {
        config->initScriptFile = url;
    }
}

void CelestiaCore::tick() {
    double lastTime = sysTime;
    sysTime = timer->getTime();

    // The time step is normally driven by the system clock; however, when
    // recording a movie, we fix the time step the frame rate of the movie.
    double dt = 0.0;
    currentTime += dt;

    Selection refObject = sim->getFrame()->getRefObject();
    if (!refObject.empty()) {
        sim->orbit(Quaternionf::Identity());
    }

    sim->update(dt);
}

// Return true if anything changed that requires re-rendering. Otherwise, we
// can skip rendering, keep the GPU idle, and save power.
bool CelestiaCore::viewUpdateRequired() const {
#if 1
    // Enable after 1.5.0
    return true;
#else
    bool isPaused = sim->getPauseState() || sim->getTimeScale() == 0.0;

    // See if the camera in any of the views is moving
    bool observersMoving = false;
    for (vector<View*>::const_iterator iter = views.begin(); iter != views.end(); iter++) {
        View* v = *iter;
        if (v->observer->getAngularVelocity().length() > 1.0e-10 || v->observer->getVelocity().length() > 1.0e-12) {
            observersMoving = true;
            break;
        }
    }

    if (viewChanged || !isPaused || observersMoving || dollyMotion != 0.0 || zoomMotion != 0.0 ||
        scriptState == ScriptRunning || renderer->settingsHaveChanged()) {
        return true;
    } else {
        return false;
    }
#endif
}

void CelestiaCore::setViewChanged() {
    viewChanged = true;
}

bool CelestiaCore::getActiveFrameVisible() const {
    return showActiveViewFrame;
}

void CelestiaCore::setActiveFrameVisible(bool visible) {
    setViewChanged();

    showActiveViewFrame = visible;
}

void CelestiaCore::setContextMenuCallback(ContextMenuFunc callback) {
    contextMenuCallback = callback;
}

class SolarSystemLoader : public EnumFilesHandler {
public:
    const UniversePtr& universe;
    const ProgressNotifierPtr& notifier;
    SolarSystemLoader(const UniversePtr& u, const ProgressNotifierPtr& pn) : universe(u), notifier(pn){};

    bool process(const string& filename) {
        if (DetermineFileType(filename) == Content_CelestiaCatalog) {
            string fullname = getPath() + '/' + filename;
            clog << _("Loading solar system catalog: ") << fullname << '\n';
            if (notifier)
                notifier->update(filename);

            ifstream solarSysFile(fullname.c_str(), ios::in);
            if (solarSysFile.good()) {
                LoadSolarSystemObjects(solarSysFile, *universe, getPath());
            }
        }

        return true;
    };
};

template <class OBJDB>
class CatalogLoader : public EnumFilesHandler {
public:
    const std::shared_ptr<OBJDB>& objDB;
    string typeDesc;
    ContentType contentType;
    const ProgressNotifierPtr& notifier;

    CatalogLoader(const std::shared_ptr<OBJDB>& db,
                  const std::string& typeDesc,
                  const ContentType& contentType,
                  const ProgressNotifierPtr& pn) :
        objDB(db),
        typeDesc(typeDesc), contentType(contentType), notifier(pn) {}

    bool process(const string& filename) {
        if (DetermineFileType(filename) == contentType) {
            string fullname = getPath() + '/' + filename;
            clog << _("Loading ") << typeDesc << " catalog: " << fullname << '\n';
            if (notifier)
                notifier->update(filename);

            ifstream catalogFile(fullname.c_str(), ios::in);
            if (catalogFile.good()) {
                bool success = objDB->load(catalogFile, getPath());
                if (!success) {
                    //DPRINTF(0, _("Error reading star file: %s\n"), fullname.c_str());
                    DPRINTF(0, "Error reading %s catalog file: %s\n", typeDesc.c_str(), fullname.c_str());
                }
            }
        }
        return true;
    }
};

typedef CatalogLoader<StarDatabase> StarLoader;
typedef CatalogLoader<DSODatabase> DeepSkyLoader;

bool CelestiaCore::initSimulation(const string& configFileName,
                                  const vector<string>& extrasDirs,
                                  const ProgressNotifierPtr& progressNotifier) {
    // Say we're not ready to render yet.
    // bReady = false;
    if (!configFileName.empty()) {
        config = ReadCelestiaConfig(configFileName);
    } else {
        config = ReadCelestiaConfig("celestia.cfg");

        string localConfigFile = WordExp("~/.celestia.cfg");
        if (localConfigFile != "")
            ReadCelestiaConfig(localConfigFile.c_str(), config);
    }

    if (config == NULL) {
        fatalError(_("Error reading configuration file."));
        return false;
    }

#ifdef USE_SPICE
    if (!InitializeSpice()) {
        fatalError(_("Initialization of SPICE library failed."));
        return false;
    }
#endif

    // Insert additional extras directories into the configuration. These
    // additional directories typically come from the command line. It may
    // be useful to permit other command line overrides of config file fields.
    if (!extrasDirs.empty()) {
        // Only insert the additional extras directories that aren't also
        // listed in the configuration file. The additional directories are added
        // after the ones from the config file and the order in which they were
        // specified is preserved. This process in O(N*M), but the number of
        // additional extras directories should be small.
        for (const auto& extraDir : extrasDirs) {
            auto& configExtrasDirs = config->extrasDirs;
            const auto begin = configExtrasDirs.begin();
            const auto end = configExtrasDirs.end();
            if (end != find(begin, end, extraDir)) {
                configExtrasDirs.push_back(extraDir);
            }
        }
    }

    readFavoritesFile();

    // If we couldn't read the favorites list from a file, allocate
    // an empty list.
    universe = std::make_shared<Universe>();

    /***** Load star catalogs *****/

    if (!readStars(*config, progressNotifier)) {
        fatalError(_("Cannot read star database."));
        return false;
    }

    /***** Load the deep sky catalogs *****/

    auto dsoNameDB = std::make_shared<DSONameDatabase>();
    auto dsoDB = std::make_shared<DSODatabase>();
    dsoDB->setNameDatabase(dsoNameDB);

    // Load first the vector of dsoCatalogFiles in the data directory (deepsky.dsc, globulars.dsc,...):

    for (const auto& file : config->dsoCatalogFiles) {
        if (progressNotifier)
            progressNotifier->update(file);

        ifstream dsoFile(file, ios::in);
        if (!dsoFile.good()) {
            cerr << _("Error opening deepsky catalog file.") << '\n';
            return false;
        } else if (!dsoDB->load(dsoFile, "")) {
            cerr << "Cannot read Deep Sky Objects database." << '\n';
            return false;
        }
    }

    // Next, read all the deep sky files in the extras directories
    {
        for (const auto& dirName : extrasDirs) {
            if (!dirName.empty()) {
                auto dir = OpenDirectory(dirName);
                DeepSkyLoader loader(dsoDB, "deep sky object", Content_CelestiaDeepSkyCatalog, progressNotifier);
                loader.pushDir(dirName);
                dir->enumFiles(loader, true);
            }
        }
    }
    dsoDB->finish();
    universe->setDSOCatalog(dsoDB);

    /***** Load the solar system catalogs *****/
    // First read the solar system files listed individually in the
    // config file.
    {
        auto solarSystemCatalog = std::make_shared<SolarSystemCatalog>();
        universe->setSolarSystemCatalog(solarSystemCatalog);
        for (const auto& file : config->solarSystemFiles) {
            if (progressNotifier)
                progressNotifier->update(file);

            ifstream solarSysFile(file, ios::in);
            if (!solarSysFile.good()) {
                warning(_("Error opening solar system catalog.\n"));
            } else {
                LoadSolarSystemObjects(solarSysFile, *universe, "");
            }
        }
    }

    // Next, read all the solar system files in the extras directories
    {
        for (const auto& dirName : config->extrasDirs) {
            if (!dirName.empty()) {
                auto dir = OpenDirectory(dirName);
                SolarSystemLoader loader(universe, progressNotifier);
                loader.pushDir(dirName);
                dir->enumFiles(loader, true);
            }
        }
    }

    // Load asterisms:
    if (config->asterismsFile != "") {
        ifstream asterismsFile(config->asterismsFile.c_str(), ios::in);
        if (!asterismsFile.good()) {
            warning(_("Error opening asterisms file."));
        } else {
            auto asterisms = ReadAsterismList(asterismsFile, *universe->getStarCatalog());
            universe->setAsterisms(asterisms);
        }
    }

    if (config->boundariesFile != "") {
        ifstream boundariesFile(config->boundariesFile.c_str(), ios::in);
        if (!boundariesFile.good()) {
            warning(_("Error opening constellation boundaries files."));
        } else {
            auto boundaries = ReadBoundaries(boundariesFile);
            universe->setBoundaries(boundaries);
        }
    }

    // Load destinations list
    if (config->destinationsFile != "") {
        string localeDestinationsFile = LocaleFilename(config->destinationsFile);
        ifstream destfile(localeDestinationsFile.c_str());
        if (destfile.good()) {
            ReadDestinationList(destfile, destinations);
        }
    }

    sim = std::make_shared<Simulation>(universe);
    auto view = std::make_shared<View>(View::ViewWindow, sim->getActiveObserver(), 0.0f, 0.0f, 1.0f, 1.0f);
    views.insert(views.end(), view);
    activeView = views.begin();

    return true;
}

static void loadCrossIndex(const StarDatabasePtr& starDB, StarDatabase::Catalog catalog, const string& filename) {
    if (!filename.empty()) {
        ifstream xrefFile(filename.c_str(), ios::in | ios::binary);
        if (xrefFile.good()) {
            if (!starDB->loadCrossIndex(catalog, xrefFile))
                cerr << _("Error reading cross index ") << filename << '\n';
            else
                clog << _("Loaded cross index ") << filename << '\n';
        }
    }
}

bool CelestiaCore::readStars(const CelestiaConfig& cfg, const ProgressNotifierPtr& progressNotifier) {
    StarDetails::SetStarTextures(cfg.starTextures);

    ifstream starNamesFile(cfg.starNamesFile.c_str(), ios::in);
    if (!starNamesFile.good()) {
        cerr << _("Error opening ") << cfg.starNamesFile << '\n';
        return false;
    }

    auto starNameDB = StarNameDatabase::readNames(starNamesFile);
    if (starNameDB == NULL) {
        cerr << _("Error reading star names file\n");
        return false;
    }

    // First load the binary star database file.  The majority of stars
    // will be defined here.
    auto starDB = std::make_shared<StarDatabase>();
    if (!cfg.starDatabaseFile.empty()) {
        if (progressNotifier)
            progressNotifier->update(cfg.starDatabaseFile);

        ifstream starFile(cfg.starDatabaseFile.c_str(), ios::in | ios::binary);
        if (!starFile.good()) {
            cerr << _("Error opening ") << cfg.starDatabaseFile << '\n';
            return false;
        }

        if (!starDB->loadBinary(starFile)) {
            cerr << _("Error reading stars file\n");
            return false;
        }
    }

    starDB->setNameDatabase(starNameDB);

    loadCrossIndex(starDB, StarDatabase::HenryDraper, cfg.HDCrossIndexFile);
    loadCrossIndex(starDB, StarDatabase::SAO, cfg.SAOCrossIndexFile);
    loadCrossIndex(starDB, StarDatabase::Gliese, cfg.GlieseCrossIndexFile);

    // Next, read any ASCII star catalog files specified in the StarCatalogs
    // list.
    if (!cfg.starCatalogFiles.empty()) {
        for (const auto& filename : config->starCatalogFiles) {
            if (!filename.empty()) {
                ifstream starFile(filename, ios::in);
                if (starFile.good()) {
                    starDB->load(starFile, "");
                } else {
                    cerr << _("Error opening star catalog ") << filename << '\n';
                }
            }
        }
    }

    // Now, read supplemental star files from the extras directories
    for (const auto& dirName : config->extrasDirs) {
        if (!dirName.empty()) {
            auto dir = OpenDirectory(dirName);
            StarLoader loader(starDB, "star", Content_CelestiaStarCatalog, progressNotifier);
            loader.pushDir(dirName);
            dir->enumFiles(loader, true);
        }
    }

    starDB->finish();

    universe->setStarCatalog(starDB);

    return true;
}

/// Set the faintest visible star magnitude; adjust the renderer's
/// brightness parameters appropriately.
void CelestiaCore::setFaintest(float magnitude) {
    sim->setFaintestVisible(magnitude);
}

/// Set faintest visible star magnitude and saturation magnitude
/// for a given field of view;
/// adjust the renderer's brightness parameters appropriately.
void CelestiaCore::setFaintestAutoMag() {
    //float faintestMag;
    //renderer->autoMag(faintestMag);
    //sim->setFaintestVisible(faintestMag);
}

void CelestiaCore::fatalError(const string& msg) {
    if (alerter == NULL)
        cout << msg;
    else
        alerter->fatalError(msg);
}

/// Sets the cursor handler object.
/// This must be set before calling initSimulation
/// or the default cursor will not be used.
int CelestiaCore::getTimeZoneBias() const {
    return timeZoneBias;
}

bool CelestiaCore::getLightDelayActive() const {
    return lightTravelFlag;
}

void CelestiaCore::setLightDelayActive(bool lightDelayActive) {
    lightTravelFlag = lightDelayActive;
}

void CelestiaCore::setTextEnterMode(int mode) {
    if (mode != textEnterMode) {
        if ((mode & KbAutoComplete) != (textEnterMode & KbAutoComplete)) {
            typedText = "";
            typedTextCompletion.clear();
            typedTextCompletionIdx = -1;
        }
        textEnterMode = mode;
        notifyWatchers(TextEnterModeChanged);
    }
}

int CelestiaCore::getTextEnterMode() const {
    return textEnterMode;
}

void CelestiaCore::setTimeZoneBias(int bias) {
    timeZoneBias = bias;
    notifyWatchers(TimeZoneChanged);
}

string CelestiaCore::getTimeZoneName() const {
    return timeZoneName;
}

void CelestiaCore::setTimeZoneName(const string& zone) {
    timeZoneName = zone;
}

int CelestiaCore::getHudDetail() {
    return hudDetail;
}

void CelestiaCore::setHudDetail(int newHudDetail) {
    hudDetail = newHudDetail % 3;
    notifyWatchers(VerbosityLevelChanged);
}

Color CelestiaCore::getTextColor() {
    return textColor;
}

void CelestiaCore::setTextColor(Color newTextColor) {
    textColor = newTextColor;
}

astro::Date::Format CelestiaCore::getDateFormat() const {
    return dateFormat;
}

void CelestiaCore::setDateFormat(astro::Date::Format format) {
    dateStrWidth = 0;
    dateFormat = format;
}

void CelestiaCore::addWatcher(CelestiaWatcher* watcher) {
    assert(watcher != NULL);
    watchers.insert(watchers.end(), watcher);
}

void CelestiaCore::removeWatcher(CelestiaWatcher* watcher) {
    auto iter = find(watchers.begin(), watchers.end(), watcher);
    if (iter != watchers.end())
        watchers.erase(iter);
}

void CelestiaCore::notifyWatchers(int property) {
    for (const auto& watcher : watchers) {
        watcher->notifyChange(this, property);
    }
}

void CelestiaCore::goToUrl(const string& urlStr) {
    Url url(urlStr, this);
    url.goTo();
    notifyWatchers(RenderFlagsChanged | LabelFlagsChanged);
}

void CelestiaCore::addToHistory() {
    auto url = std::make_shared<Url>(shared_from_this());
    if (!history.empty() && historyCurrent < history.size() - 1) {
        // truncating history to current position
        while (historyCurrent != history.size() - 1) {
            history.pop_back();
        }
    }
    history.push_back(url);
    historyCurrent = history.size() - 1;
    notifyWatchers(HistoryChanged);
}

void CelestiaCore::back() {
    if (historyCurrent == 0)
        return;

    if (historyCurrent == history.size() - 1) {
        addToHistory();
        historyCurrent = history.size() - 1;
    }
    historyCurrent--;
    history[historyCurrent]->goTo();
    notifyWatchers(HistoryChanged | RenderFlagsChanged | LabelFlagsChanged);
}

void CelestiaCore::forward() {
    if (history.size() == 0)
        return;
    if (historyCurrent == history.size() - 1)
        return;
    historyCurrent++;
    history[historyCurrent]->goTo();
    notifyWatchers(HistoryChanged | RenderFlagsChanged | LabelFlagsChanged);
}

void CelestiaCore::setHistoryCurrent(vector<UrlPtr>::size_type curr) {
    if (curr >= history.size())
        return;
    if (historyCurrent == history.size()) {
        addToHistory();
    }
    historyCurrent = curr;
    history[curr]->goTo();
    notifyWatchers(HistoryChanged | RenderFlagsChanged | LabelFlagsChanged);
}

/*! Toggle the specified reference mark for a selection.
 *  a selection. The default value for the selection argument is
 *  the current simulation selection. This method does nothing
 *  if the selection isn't a solar system body.
 */
void CelestiaCore::toggleReferenceMark(const string& refMark, Selection sel) {
    BodyPtr body;

    if (sel.empty())
        body = getSimulation()->getSelection().body();
    else
        body = sel.body();

    // Reference marks can only be set for solar system bodies.
    if (!body)
        return;

    if (body->findReferenceMark(refMark)) {
        body->removeReferenceMark(refMark);
    } else {
        if (refMark == "body axes") {
            body->addReferenceMark(std::make_shared<BodyAxisArrows>(body));
        } else if (refMark == "frame axes") {
            body->addReferenceMark(std::make_shared<FrameAxisArrows>(body));
        } else if (refMark == "sun direction") {
            body->addReferenceMark(std::make_shared<SunDirectionArrow>(body));
        } else if (refMark == "velocity vector") {
            body->addReferenceMark(std::make_shared<VelocityVectorArrow>(body));
        } else if (refMark == "spin vector") {
            body->addReferenceMark(std::make_shared<SpinVectorArrow>(body));
        } else if (refMark == "frame center direction") {
            double now = getSimulation()->getTime();
            auto arrow = std::make_shared<BodyToBodyDirectionArrow>(body, body->getOrbitFrame(now)->getCenter());
            arrow->setTag(refMark);
            body->addReferenceMark(arrow);
        } else if (refMark == "planetographic grid") {
            body->addReferenceMark(std::make_shared<PlanetographicGrid>(body));
        } else if (refMark == "terminator") {
            double now = getSimulation()->getTime();
            StarPtr sun = NULL;
            auto b = body;
            while (b != NULL) {
                Selection center = b->getOrbitFrame(now)->getCenter();
                if (center.star() != NULL)
                    sun = center.star();
                b = center.body();
            }

            if (sun != NULL) {
                auto visibleRegion = std::make_shared<VisibleRegion>(body, Selection(sun));
                visibleRegion->setTag("terminator");
                body->addReferenceMark(visibleRegion);
            }
        }
    }
}

/*! Return whether the specified reference mark is enabled for a
 *  a selection. The default value for the selection argument is
 *  the current simulation selection.
 */
bool CelestiaCore::referenceMarkEnabled(const string& refMark, Selection sel) const {
    BodyPtr body = NULL;

    if (sel.empty())
        body = getSimulation()->getSelection().body();
    else
        body = sel.body();

    // Reference marks can only be set for solar system bodies.
    if (body == NULL)
        return false;
    else
        return body->findReferenceMark(refMark) != NULL;
}
