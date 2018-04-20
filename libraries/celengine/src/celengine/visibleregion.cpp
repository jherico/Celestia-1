// visibleregion.cpp
//
// Visible region reference mark for ellipsoidal bodies.
//
// Copyright (C) 2008, the Celestia Development Team
// Initial version by Chris Laurel, claurel@gmail.com
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#include "visibleregion.h"

#include <cstdio>
#include <cmath>

#include <Eigen/Core>
#include <Eigen/Geometry>

#include <celmath/intersect.h>

#include "render.h"
#include "body.h"
#include "selection.h"

using namespace Eigen;

/*! Construct a new reference mark that shows the outline of the
 *  region on the surface of a body in which the target object is
 *  visible. The following are assumed:
 *     - target is a point
 *     - the body is an ellipsoid
 *  
 *  This reference mark is useful in a few situations. When the
 *  body is a planet or moon and target is the sun, the outline of
 *  the visible region is the terminator. If target is a satellite,
 *  the outline is its circle of visibility.
 */
VisibleRegion::VisibleRegion(const BodyConstPtr& body, const Selection& target) :
    m_body(body), m_target(target), m_color(1.0f, 1.0f, 0.0f),
#ifdef USE_HDR
    m_opacity(0.0f)
#else
    m_opacity(1.0f)
#endif
{
    setTag("visible region");
}

VisibleRegion::~VisibleRegion() {
}

Color VisibleRegion::color() const {
    return m_color;
}

void VisibleRegion::setColor(Color color) {
    m_color = color;
}

float VisibleRegion::opacity() const {
    return m_opacity;
}

void VisibleRegion::setOpacity(float opacity) {
    m_opacity = opacity;
#ifdef USE_HDR
    m_opacity = 1.0f - opacity;
#endif
}

template <typename T>
static Matrix<T, 3, 1> ellipsoidTangent(const Matrix<T, 3, 1>& recipSemiAxes,
                                        const Matrix<T, 3, 1>& w,
                                        const Matrix<T, 3, 1>& e,
                                        const Matrix<T, 3, 1>& e_,
                                        T ee) {
    // We want to find t such that -E(1-t) + Wt is the direction of a ray
    // tangent to the ellipsoid.  A tangent ray will intersect the ellipsoid
    // at exactly one point.  Finding the intersection between a ray and an
    // ellipsoid ultimately requires using the quadratic formula, which has
    // one solution when the discriminant (b^2 - 4ac) is zero.  The code below
    // computes the value of t that results in a discriminant of zero.
    Matrix<T, 3, 1> w_ =
        w.cwiseProduct(recipSemiAxes);  //(w.x * recipSemiAxes.x, w.y * recipSemiAxes.y, w.z * recipSemiAxes.z);
    T ww = w_.dot(w_);
    T ew = w_.dot(e_);

    // Before elimination of terms:
    // double a =  4 * square(ee + ew) - 4 * (ee + 2 * ew + ww) * (ee - 1.0f);
    // double b = -8 * ee * (ee + ew)  - 4 * (-2 * (ee + ew) * (ee - 1.0f));
    // double c =  4 * ee * ee         - 4 * (ee * (ee - 1.0f));

    // Simplify the below expression and eliminate the ee^2 terms; this
    // prevents precision errors, as ee tends to be a very large value.
    //T a =  4 * square(ee + ew) - 4 * (ee + 2 * ew + ww) * (ee - 1);
    T a = 4 * (square(ew) - ee * ww + ee + 2 * ew + ww);
    T b = -8 * (ee + ew);
    T c = 4 * ee;

    T t = 0;
    T discriminant = b * b - 4 * a * c;

    if (discriminant < 0)
        t = (-b + (T)sqrt(-discriminant)) / (2 * a);  // Bad!
    else
        t = (-b + (T)sqrt(discriminant)) / (2 * a);

    // V is the direction vector.  We now need the point of intersection,
    // which we obtain by solving the quadratic equation for the ray-ellipse
    // intersection.  Since we already know that the discriminant is zero,
    // the solution is just -b/2a
    Matrix<T, 3, 1> v = -e * (1 - t) + w * t;
    Matrix<T, 3, 1> v_ = v.cwiseProduct(recipSemiAxes);
    T a1 = v_.dot(v_);
    T b1 = (T)2 * v_.dot(e_);
    T t1 = -b1 / (2 * a1);

    return e + v * t1;
}

float VisibleRegion::boundingSphereRadius() const {
    return m_body->getRadius();
}
