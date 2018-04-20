// parseobject.h
//
// Copyright (C) 2004 Chris Laurel <claurel@shatters.net>
//
// Functions for parsing objects common to star, solar system, and
// deep sky catalogs.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#ifndef _CELENGINE_PARSEOBJECT_H_
#define _CELENGINE_PARSEOBJECT_H_

#include <string>
#include <celastro/astro.h>
#include "body.h"
#include "parser.h"

class ReferenceFrame;
class TwoVectorFrame;
class Universe;
class Selection;

bool ParseDate(const HashPtr& hash, const std::string& name, double& jd);

Orbit::Pointer CreateOrbit(const Selection& centralObject,
                           const HashPtr& planetData,
                           const std::string& path,
                           bool usePlanetUnits);

RotationModel::Pointer CreateRotationModel(const HashPtr& rotationData, const std::string& path, double syncRotationPeriod);

RotationModel::Pointer CreateDefaultRotationModel(double syncRotationPeriod);

ReferenceFramePtr CreateReferenceFrame(const Universe& universe,
                                       const ValuePtr& frameValue,
                                       const Selection& defaultCenter,
                                       const BodyPtr& defaultObserver);

ReferenceFramePtr CreateTopocentricFrame(const Selection& center, const Selection& target, const Selection& observer);

#endif  // _CELENGINE_PARSEOBJECT_H_
