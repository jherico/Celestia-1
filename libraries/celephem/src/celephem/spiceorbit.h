// spiceorbit.h
//
// Interface to the SPICE Toolkit
//
// Copyright (C) 2006, Chris Laurel <claurel@shatters.net>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#ifndef _CELENGINE_SPICEORBIT_H_
#define _CELENGINE_SPICEORBIT_H_

#include "orbit.h"
#include <string>
#include <list>

class SpiceOrbit : public CachingOrbit {
public:
    using Pointer = std::shared_ptr<SpiceOrbit>;
    SpiceOrbit(const std::string& _targetBodyName,
               const std::string& _originName,
               double _period,
               double _boundingRadius,
               double _beginning,
               double _ending);
    SpiceOrbit(const std::string& _targetBodyName, const std::string& _originName, double _period, double _boundingRadius);
    virtual ~SpiceOrbit();

    bool init(const std::string& path, const std::list<std::string>& requiredKernels = {});

    bool isPeriodic() const override;
    double getPeriod() const override;

    double getBoundingRadius() const override { return boundingRadius; }

    Eigen::Vector3d computePosition(double jd) const override;
    Eigen::Vector3d computeVelocity(double jd) const override;

    virtual void getValidRange(double& begin, double& end) const;

private:
    const std::string targetBodyName;
    const std::string originName;
    const double period;
    const double boundingRadius;
    bool spiceErr;

    // NAIF ID codes for the target body and origin body
    int targetID;
    int originID;

    double validIntervalBegin;
    double validIntervalEnd;

    bool useDefaultTimeInterval;
};

#endif  // _CELENGINE_SPICEORBIT_H_
