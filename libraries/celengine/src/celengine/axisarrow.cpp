// axisarrow.cpp
//
// Copyright (C) 2007-2009, Celestia Development Team
// Original version by Chris Laurel <claurel@gmail.com>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#include <algorithm>
#include <celmath/mathlib.h>
#include "axisarrow.h"
#include "selection.h"
#include "frame.h"
#include "body.h"
#include "timelinephase.h"

using namespace Eigen;
using namespace std;

static const unsigned int MaxArrowSections = 100;

/****** ArrowReferenceMark base class ******/

ArrowReferenceMark::ArrowReferenceMark(const BodyConstPtr& _body) :
    body(_body), size(1.0), color(1.0f, 1.0f, 1.0f),
#ifdef USE_HDR
    opacity(0.0f)
#else
    opacity(1.0f)
#endif
{
}

void ArrowReferenceMark::setSize(float _size) {
    size = _size;
}

void ArrowReferenceMark::setColor(Color _color) {
    color = _color;
}

/****** AxesReferenceMark base class ******/

AxesReferenceMark::AxesReferenceMark(const BodyConstPtr& _body) :
    body(_body), size(),
#ifdef USE_HDR
    opacity(0.0f)
#else
    opacity(1.0f)
#endif
{
}

void AxesReferenceMark::setSize(float _size) {
    size = _size;
}

void AxesReferenceMark::setOpacity(float _opacity) {
    opacity = _opacity;
#ifdef USE_HDR
    opacity = 1.0f - opacity;
#endif
}

/****** VelocityVectorArrow implementation ******/

VelocityVectorArrow::VelocityVectorArrow(const BodyConstPtr& _body) : ArrowReferenceMark(_body) {
    setTag("velocity vector");
    setColor(Color(0.6f, 0.6f, 0.9f));
    setSize(body->getRadius() * 2.0f);
}

Vector3d VelocityVectorArrow::getDirection(double tdb) const {
    auto phase = body->getTimeline()->findPhase(tdb);
    return phase->orbitFrame()->getOrientation(tdb).conjugate() * phase->orbit()->velocityAtTime(tdb);
}

/****** SunDirectionArrow implementation ******/

SunDirectionArrow::SunDirectionArrow(const BodyConstPtr& _body) : ArrowReferenceMark(_body) {
    setTag("sun direction");
    setColor(Color(1.0f, 1.0f, 0.4f));
    setSize(body->getRadius() * 2.0f);
}

Vector3d SunDirectionArrow::getDirection(double tdb) const {
    auto b = body;
    StarPtr sun = NULL;
    while (b != NULL) {
        Selection center = b->getOrbitFrame(tdb)->getCenter();
        if (center.star() != NULL)
            sun = center.star();
        b = center.body();
    }

    if (sun != NULL) {
        return Selection(sun).getPosition(tdb).offsetFromKm(body->getPosition(tdb));
    } else {
        return Vector3d::Zero();
    }
}

/****** SpinVectorArrow implementation ******/

SpinVectorArrow::SpinVectorArrow(const BodyConstPtr& _body) : ArrowReferenceMark(_body) {
    setTag("spin vector");
    setColor(Color(0.6f, 0.6f, 0.6f));
    setSize(body->getRadius() * 2.0f);
}

Vector3d SpinVectorArrow::getDirection(double tdb) const {
    auto phase = body->getTimeline()->findPhase(tdb);
    return phase->bodyFrame()->getOrientation(tdb).conjugate() * phase->rotationModel()->angularVelocityAtTime(tdb);
}

/****** BodyToBodyDirectionArrow implementation ******/

/*! Create a new body-to-body direction arrow pointing from the origin body toward
 *  the specified target object.
 */
BodyToBodyDirectionArrow::BodyToBodyDirectionArrow(const BodyConstPtr& _body, const Selection& _target) :
    ArrowReferenceMark(_body), target(_target) {
    setTag("body to body");
    setColor(Color(0.0f, 0.5f, 0.0f));
    setSize(body->getRadius() * 2.0f);
}

Vector3d BodyToBodyDirectionArrow::getDirection(double tdb) const {
    return target.getPosition(tdb).offsetFromKm(body->getPosition(tdb));
}

/****** BodyAxisArrows implementation ******/

BodyAxisArrows::BodyAxisArrows(const BodyConstPtr& _body) : AxesReferenceMark(_body) {
    setTag("body axes");
    setOpacity(1.0);
    setSize(body->getRadius() * 2.0f);
}

Quaterniond BodyAxisArrows::getOrientation(double tdb) const {
    return (Quaterniond(AngleAxis<double>(PI, Vector3d::UnitY())) * body->getEclipticToBodyFixed(tdb)).conjugate();
}

/****** FrameAxisArrows implementation ******/

FrameAxisArrows::FrameAxisArrows(const BodyConstPtr& _body) : AxesReferenceMark(_body) {
    setTag("frame axes");
    setOpacity(0.5);
    setSize(body->getRadius() * 2.0f);
}

Quaterniond FrameAxisArrows::getOrientation(double tdb) const {
    return body->getEclipticToFrame(tdb).conjugate();
}
