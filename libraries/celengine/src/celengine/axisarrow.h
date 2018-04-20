// axisarrow.h
//
// Copyright (C) 2007-2009, Celestia Development Team
// Original version by Chris Laurel <claurel@gmail.com>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#ifndef _CELENGINE_AXISARROW_H_
#define _CELENGINE_AXISARROW_H_

#include <celutil/color.h>
#include "referencemark.h"
#include "selection.h"
#include <Eigen/Core>
#include <Eigen/Geometry>

class Body;

class ArrowReferenceMark : public ReferenceMark {
public:
    ArrowReferenceMark(const BodyConstPtr& _body);

    void setSize(float _size);
    void setColor(Color _color);

    float boundingSphereRadius() const { return size; }

    bool isOpaque() const { return opacity == 1.0f; }

    virtual Eigen::Vector3d getDirection(double tdb) const = 0;

protected:
    const BodyConstPtr body;

private:
    float size;
    Color color;
    float opacity;
};

class AxesReferenceMark : public ReferenceMark {
public:
    AxesReferenceMark(const BodyConstPtr& _body);

    void setSize(float _size);
    void setOpacity(float _opacity);

    float boundingSphereRadius() const { return size; }

    bool isOpaque() const { return opacity == 1.0f; }

    virtual Eigen::Quaterniond getOrientation(double tdb) const = 0;

protected:
    BodyConstPtr body;

private:
    float size;
    float opacity;
};

class BodyAxisArrows : public AxesReferenceMark {
public:
    BodyAxisArrows(const BodyConstPtr& _body);
    Eigen::Quaterniond getOrientation(double tdb) const;
};

class FrameAxisArrows : public AxesReferenceMark {
public:
    FrameAxisArrows(const BodyConstPtr& _body);
    Eigen::Quaterniond getOrientation(double tdb) const;
};

class SunDirectionArrow : public ArrowReferenceMark {
public:
    SunDirectionArrow(const BodyConstPtr& _body);
    Eigen::Vector3d getDirection(double tdb) const;
};

class VelocityVectorArrow : public ArrowReferenceMark {
public:
    VelocityVectorArrow(const BodyConstPtr& _body);
    Eigen::Vector3d getDirection(double tdb) const;
};

class SpinVectorArrow : public ArrowReferenceMark {
public:
    SpinVectorArrow(const BodyConstPtr& _body);
    Eigen::Vector3d getDirection(double tdb) const;
};

/*! The body-to-body direction arrow points from the center of
 *  the primary body toward a target object.
 */
class BodyToBodyDirectionArrow : public ArrowReferenceMark {
public:
    BodyToBodyDirectionArrow(const BodyConstPtr& _body, const Selection& _target);
    Eigen::Vector3d getDirection(double tdb) const;

private:
    Selection target;
};

#endif  // _CELENGINE_AXISARROW_H_
