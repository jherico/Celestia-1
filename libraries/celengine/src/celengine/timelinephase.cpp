// timelinephase.cpp
//
// Object timeline phase
//
// Copyright (C) 2008, the Celestia Development Team
// Initial version by Chris Laurel, claurel@gmail.com
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#include "timelinephase.h"

#include <cassert>

#include <celephem/orbit.h>
#include <celephem/rotation.h>

#include "frame.h"
#include "universe.h"
#include "frametree.h"

TimelinePhase::TimelinePhase(const BodyPtr& _body,
                             double _startTime,
                             double _endTime,
                             const ReferenceFramePtr& _orbitFrame,
                             const OrbitPtr& _orbit,
                             const ReferenceFramePtr& _bodyFrame,
                             const RotationModelPtr& _rotationModel,
                             const FrameTreePtr& _owner) :
    m_body(_body),
    m_startTime(_startTime), m_endTime(_endTime), m_orbitFrame(_orbitFrame), m_orbit(_orbit), m_bodyFrame(_bodyFrame),
    m_rotationModel(_rotationModel), m_owner(_owner), refCount(0) {
    // assert(owner == orbitFrame->getCenter()->getFrameTree());
}

TimelinePhase::~TimelinePhase() {
}

// Declared private--should never be used
TimelinePhase::TimelinePhase(const TimelinePhase&) {
    assert(0);
}

int TimelinePhase::addRef() const {
    return ++refCount;
}

int TimelinePhase::release() const {
    --refCount;
    assert(refCount >= 0);
    if (refCount <= 0)
        delete this;

    return refCount;
}

/*! Create a new timeline phase in the specified universe.
 */
TimelinePhasePtr TimelinePhase::CreateTimelinePhase(Universe& universe,
                                                    const BodyPtr& body,
                                                    double startTime,
                                                    double endTime,
                                                    const ReferenceFramePtr& orbitFrame,
                                                    const OrbitPtr& orbit,
                                                    const ReferenceFramePtr& bodyFrame,
                                                    const RotationModelPtr& rotationModel) {
    // Validate the time range.
    if (endTime <= startTime)
        return NULL;

    // Get the frame tree to add the new phase to. Verify that the reference frame
    // center is either a star or solar system body.
    FrameTreePtr frameTree = NULL;
    Selection center = orbitFrame->getCenter();
    if (center.body() != NULL) {
        frameTree = center.body()->getOrCreateFrameTree();
    } else if (center.star() != NULL) {
        SolarSystemPtr solarSystem = universe.getSolarSystem(center.star());
        if (solarSystem == NULL) {
            // No solar system defined for this star yet, so we need
            // to create it.
            solarSystem = universe.createSolarSystem(center.star());
        }
        frameTree = solarSystem->getFrameTree();
    } else {
        // Frame center is not a star or body.
        return NULL;
    }

    TimelinePhasePtr phase =
        std::make_shared<TimelinePhase>(body, startTime, endTime, orbitFrame, orbit, bodyFrame, rotationModel, frameTree);

    frameTree->addChild(phase);

    return phase;
}
