// timelinephase.h
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

#ifndef _CELENGINE_TIMELINEPHASE_H_
#define _CELENGINE_TIMELINEPHASE_H_

#include "forward.h"

class TimelinePhase {
public:
    int addRef() const;
    int release() const;

    const BodyPtr& body() const { return m_body; }

    double startTime() const { return m_startTime; }

    double endTime() const { return m_endTime; }

    const ReferenceFramePtr& orbitFrame() const { return m_orbitFrame; }

    const OrbitPtr& orbit() const { return m_orbit; }

    const ReferenceFramePtr& bodyFrame() const { return m_bodyFrame; }

    const RotationModelPtr& rotationModel() const { return m_rotationModel; }

    /*! Get the frame tree that contains this phase (always the tree associated
     *  with the center of the orbit frame.)
     */
    const FrameTreePtr& getFrameTree() const { return m_owner; }

    /*! Check whether the specified time t lies within this phase. True if
     *  startTime <= t < endTime
     */
    bool includes(double t) const { return m_startTime <= t && t < m_endTime; }

    static TimelinePhasePtr CreateTimelinePhase(Universe& universe,
                                                const BodyPtr& body,
                                                double startTime,
                                                double endTime,
                                                const ReferenceFramePtr& orbitFrame,
                                                const OrbitPtr& orbit,
                                                const ReferenceFramePtr& bodyFrame,
                                                const RotationModelPtr& rotationModel);

public:
    // Private constructor; phases can only created with the
    // createTimelinePhase factory method.
    TimelinePhase(const BodyPtr& _body,
                  double _startTime,
                  double _endTime,
                  const ReferenceFramePtr& _orbitFrame,
                  const OrbitPtr& _orbit,
                  const ReferenceFramePtr& _bodyFrame,
                  const RotationModelPtr& _rotationModel,
                  const FrameTreePtr& _owner);

    // Private copy constructor and assignment operator; should never be used.
    TimelinePhase(const TimelinePhase& phase);
    TimelinePhase& operator=(const TimelinePhase& phase) = delete;

    // TimelinePhases are refCounted; use release() instead.
    ~TimelinePhase();

private:
    const BodyPtr m_body;

    double m_startTime;
    double m_endTime;

    const ReferenceFramePtr m_orbitFrame;
    const OrbitPtr m_orbit;
    const ReferenceFramePtr m_bodyFrame;
    const RotationModelPtr m_rotationModel;
    const FrameTreePtr m_owner;

    mutable int refCount;
};

#endif  // _CELENGINE_TIMELINEPHASE_H_
