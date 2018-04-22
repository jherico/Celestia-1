// timeline.h
//
// Object timelines.
//
// Copyright (C) 2008, the Celestia Development Team
// Initial version by Chris Laurel, claurel@gmail.com
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#ifndef _CELENGINE_TIMELINE_H_
#define _CELENGINE_TIMELINE_H_

#include <vector>
#include "forward.h"

class Timeline {
public:
    Timeline();
    ~Timeline();

    const TimelinePhasePtr& findPhase(double t) const;
    bool appendPhase(const TimelinePhasePtr&);
    const TimelinePhasePtr& getPhase(size_t n) const;
    size_t phaseCount() const;

    double startTime() const;
    double endTime() const;
    bool includes(double t) const;

    void markChanged();

private:
    std::vector<TimelinePhasePtr> phases;
};

#endif  // _CELENGINE_TIMELINE_H_
