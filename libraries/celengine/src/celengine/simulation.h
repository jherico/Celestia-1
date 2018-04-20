// simulation.h
//
// Copyright (C) 2001, Chris Laurel <claurel@shatters.net>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#ifndef _CELENGINE_SIMULATION_H_
#define _CELENGINE_SIMULATION_H_

#include "texture.h"
#include "universe.h"
#include <celastro/astro.h>
#include "galaxy.h"
#include "globular.h"
#include "texmanager.h"
#include "frame.h"
#include "observer.h"
#include <celmath/vecmath.h>
#include <celmath/quaternion.h>
#include <Eigen/Core>
#include <vector>

class Simulation {
public:
    Simulation(const UniversePtr&);
    ~Simulation();

    // Julian date
    double getTime() const { return activeObserver->getTime(); }
    void setTime(double t);

    double getRealTime() const { return realTime; }
    // Get the clock time elapsed since the object was created
    double getArrivalTime() const { return activeObserver->getArrivalTime(); }

    void update(double dt);

    Selection pickObject(const Eigen::Vector3f& pickRay, int renderFlags, float tolerance = 0.0f);

    const UniversePtr& getUniverse() const { return universe; }

    void orbit(const Eigen::Quaternionf& q);
    void rotate(const Eigen::Quaternionf& q);
    void changeOrbitDistance(float d);
    void setTargetSpeed(float s);
    float getTargetSpeed();

    Selection getSelection() const;
    void setSelection(const Selection&);
    Selection getTrackedObject() const;
    void setTrackedObject(const Selection&);

    void selectPlanet(int);
    Selection findObject(std::string s, bool i18n = false);
    Selection findObjectFromPath(std::string s, bool i18n = false);
    std::vector<std::string> getObjectCompletion(std::string s, bool withLocations = false);
    void gotoSelection(double gotoTime, const Eigen::Vector3f& up, ObserverFrame::CoordinateSystem upFrame);
    void gotoSelection(double gotoTime, double distance, const Eigen::Vector3f& up, ObserverFrame::CoordinateSystem upFrame);
    void gotoSelectionLongLat(double gotoTime, double distance, float longitude, float latitude, const Eigen::Vector3f& up);
    void gotoLocation(const UniversalCoord& toPosition, const Eigen::Quaterniond& toOrientation, double duration);
    void getSelectionLongLat(double& distance, double& longitude, double& latitude);
    void gotoSurface(double duration);
    void centerSelection(double centerTime = 0.5);
    void centerSelectionCO(double centerTime = 0.5);
    void follow();
    void geosynchronousFollow();
    void phaseLock();
    void chase();
    void cancelMotion();

    Observer& getObserver() { return *activeObserver; }
    void setObserverPosition(const UniversalCoord&);
    void setObserverOrientation(const Eigen::Quaternionf&);
    void reverseObserverOrientation() { activeObserver->reverseOrientation(); }

    const ObserverPtr& addObserver();
    void removeObserver(const ObserverPtr&);
    const ObserverPtr& getActiveObserver() { return activeObserver; }
    void setActiveObserver(const ObserverPtr&);
    const SolarSystemPtr& getNearestSolarSystem() const { return closestSolarSystem; }

    double getTimeScale() const;
    void setTimeScale(double);
    bool getSyncTime() const;
    void setSyncTime(bool);
    void synchronizeTime();
    bool getPauseState() const;
    void setPauseState(bool);

    float getFaintestVisible() const { return faintestVisible; }
    void setFaintestVisible(float magnitude) { faintestVisible = magnitude; }

    void setObserverMode(Observer::ObserverMode);
    Observer::ObserverMode getObserverMode() const;

    void setFrame(ObserverFrame::CoordinateSystem, const Selection& refObject, const Selection& targetObject);
    void setFrame(ObserverFrame::CoordinateSystem, const Selection& refObject);
    const ObserverFramePtr& getFrame() const;

private:
    SolarSystemPtr getSolarSystem(const StarPtr& star);

private:
    double realTime{ 0 };
    double timeScale{ 1.0 };
    double storedTimeScale{ 1.0 };
    bool syncTime{ true };

    UniversePtr universe;

    SolarSystemPtr closestSolarSystem;
    Selection selection;

    ObserverPtr activeObserver;
    std::vector<ObserverPtr> observers;

    float faintestVisible{ 5.0f };
    bool pauseState{ false };
};

#endif  // _CELENGINE_SIMULATION_H_
