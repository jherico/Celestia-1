// selection.h
//
// Copyright (C) 2001, Chris Laurel <claurel@shatters.net>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#ifndef _CELENGINE_SELECTION_H_
#define _CELENGINE_SELECTION_H_

#include <string>
#include "forward.h"
#include "star.h"
#include "body.h"
#include "deepskyobj.h"
#include "location.h"
#include "univcoord.h"
#include <Eigen/Core>

class Selection {
public:
    enum Type
    {
        Type_Nil,
        Type_Star,
        Type_Body,
        Type_DeepSky,
        Type_Location,
    };

public:
    Selection() : type(Type_Nil) {}
    Selection(const StarPtr& star) : type(star ? Type_Star : Type_Nil), obj(star) {}
    Selection(const BodyPtr& body) : type(body ? Type_Body : Type_Nil), obj(body) {}
    Selection(const DeepSkyObjectPtr& deepsky) : type(deepsky ? Type_DeepSky : Type_Nil), obj(deepsky) {}
    Selection(const LocationPtr& location) : type(location ? Type_Location : Type_Nil), obj(location) {}
//    Selection(const StarConstPtr& star) : type(star ? Type_Star : Type_Nil), obj(star) {}
//    Selection(const BodyConstPtr& body) : type(body ? Type_Body : Type_Nil), obj(body) {}
//    Selection(const DeepSkyObjectConstPtr& deepsky) : type(deepsky ? Type_DeepSky : Type_Nil), obj(deepsky) {}
//    Selection(const LocationConstPtr& location) : type(location ? Type_Location : Type_Nil), obj(location) {}
    Selection(const Selection& sel) : type(sel.type), obj(sel.obj) {}
    ~Selection(){};

    Selection& operator=(const Selection& other) {
        type = other.type;
        obj = other.obj;
        return *this;
    }

    bool empty() const { return type == Type_Nil; }
    double radius() const;
    UniversalCoord getPosition(double t) const;
    Eigen::Vector3d getVelocity(double t) const;
    std::string getName(bool i18n = false) const;
    Selection parent() const;

    bool isVisible() const;

    StarPtr star() const {
        return type == Type_Star ? std::static_pointer_cast<Star>(obj) : nullptr;
    }

    BodyPtr body() const {
        return type == Type_Body ? std::static_pointer_cast<Body>(obj) : NULL;
    }

    DeepSkyObjectPtr deepsky() const {
        return type == Type_DeepSky ? std::static_pointer_cast<DeepSkyObject>(obj) : NULL;
    }

    LocationPtr location() const {
        return type == Type_Location ? std::static_pointer_cast<Location>(obj) : NULL;
    }

    Type getType() const {
        return type;
    }

    // private:
    Type type;
    ObjectPtr obj;
};

inline bool operator==(const Selection& s0, const Selection& s1) {
    return s0.type == s1.type && s0.obj == s1.obj;
}

inline bool operator!=(const Selection& s0, const Selection& s1) {
    return s0.type != s1.type || s0.obj != s1.obj;
}

#endif  // _CELENGINE_SELECTION_H_
