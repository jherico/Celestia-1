// universe.h
//
// Copyright (C) 2001-2009, the Celestia Development Team
// Original version by Chris Laurel <claurel@gmail.com>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#ifndef _CELENGINE_UNIVERSE_H_
#define _CELENGINE_UNIVERSE_H_

#include "univcoord.h"
#include "stardb.h"
#include "dsodb.h"
#include "solarsys.h"
#include "deepskyobj.h"
#include "marker.h"
#include "selection.h"
#include "asterism.h"
#include <vector>

class ConstellationBoundaries;
class Asterism;

class Universe : public std::enable_shared_from_this<Universe> {
public:
    Universe();
    ~Universe();

    const StarDatabasePtr& getStarCatalog() const { return starCatalog; }
    void setStarCatalog(const StarDatabasePtr& _starCatalog) { starCatalog = _starCatalog; }
    const SolarSystemCatalogPtr& getSolarSystemCatalog() const { return solarSystemCatalog; }
    void setSolarSystemCatalog(const SolarSystemCatalogPtr& catalog) { solarSystemCatalog = catalog; }
    const DSODatabasePtr& getDSOCatalog() const { return dsoCatalog; }
    void setDSOCatalog(const DSODatabasePtr& catalog) { dsoCatalog = catalog; }
    const AsterismListPtr& getAsterisms() const { return asterisms; }
    void setAsterisms(const AsterismListPtr& _asterisms) { asterisms = _asterisms; }
    const ConstellationBoundariesPtr& getBoundaries() const { return boundaries; }
    void setBoundaries(const ConstellationBoundariesPtr& _boundaries) { boundaries = _boundaries; }

    Selection pick(const UniversalCoord& origin,
                   const Eigen::Vector3f& direction,
                   double when,
                   int renderFlags,
                   float faintestMag,
                   float tolerance = 0.0f);

    Selection find(const std::string& s, Selection* contexts = NULL, int nContexts = 0, bool i18n = false) const;
    Selection findPath(const std::string& s, Selection* contexts = NULL, int nContexts = 0, bool i18n = false) const;
    Selection findChildObject(const Selection& sel, const std::string& name, bool i18n = false) const;
    Selection findObjectInContext(const Selection& sel, const std::string& name, bool i18n = false) const;

    std::vector<std::string> getCompletion(const std::string& s,
                                           Selection* contexts = NULL,
                                           int nContexts = 0,
                                           bool withLocations = false);
    std::vector<std::string> getCompletionPath(const std::string& s,
                                               Selection* contexts = NULL,
                                               int nContexts = 0,
                                               bool withLocations = false);

    SolarSystemPtr getNearestSolarSystem(const UniversalCoord& position) const;
    SolarSystemPtr getSolarSystem(const StarPtr& star) const;
    SolarSystemPtr getSolarSystem(const Selection&) const;
    SolarSystemPtr createSolarSystem(const StarPtr& star) const;

    void getNearStars(const UniversalCoord& position, float maxDistance, std::vector<StarPtr>& stars) const;

    void markObject(const Selection&,
                    const MarkerRepresentation& rep,
                    int priority,
                    bool occludable = true,
                    MarkerSizing sizing = ConstantSize);
    void unmarkObject(const Selection&, int priority);
    void unmarkAll();
    bool isMarked(const Selection&, int priority) const;
    const MarkerList& getMarkers() const { return markers; }

private:
    Selection pickPlanet(SolarSystem& solarSystem,
                         const UniversalCoord& origin,
                         const Eigen::Vector3f& direction,
                         double when,
                         float faintestMag,
                         float tolerance);

    Selection pickStar(const UniversalCoord& origin,
                       const Eigen::Vector3f& direction,
                       double when,
                       float faintest,
                       float tolerance = 0.0f);

    Selection pickDeepSkyObject(const UniversalCoord& origin,
                                const Eigen::Vector3f& direction,
                                int renderFlags,
                                float faintest,
                                float tolerance = 0.0f);

private:
    StarDatabasePtr starCatalog;
    DSODatabasePtr dsoCatalog;
    SolarSystemCatalogPtr solarSystemCatalog;
    AsterismListPtr asterisms;
    ConstellationBoundariesPtr boundaries;
    MarkerList markers;
    std::vector<StarPtr> closeStars;
};

#endif  // _CELENGINE_UNIVERSE_H_
