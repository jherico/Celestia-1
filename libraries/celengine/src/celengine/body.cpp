// body.cpp
//
// Copyright (C) 2001-2006 Chris Laurel <claurel@shatters.net>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#include "body.h"

#include <cstdlib>
#include <cassert>
#include <algorithm>

#include <celutil/util.h>
#include <celutil/utf8.h>
#include <celmath/mathlib.h>

#include "atmosphere.h"
#include "frame.h"
#include "timeline.h"
#include "timelinephase.h"
#include "frametree.h"
#include "referencemark.h"

using namespace Eigen;
using namespace std;

Body::Body(const PlanetarySystemPtr& _system, const string& _name) :
    system(_system), classification(Unknown), visible(1), clickable(1), visibleAsPoint(1), overrideOrbitColor(0),
    orbitVisibility(UseClassVisibility), secondaryIlluminator(true) {
    setName(_name);
    recomputeCullingRadius();
}

Body::~Body() {
    if (system) {
        system->removeBody(shared_from_this());
    }
}

/*! Reset body attributes to their default values. The object hierarchy is left untouched,
 *  i.e. child objects are not removed. Alternate surfaces and locations are not removed
 *  either.
 */
void Body::setDefaultProperties() {
    radius = 1.0f;
    semiAxes = Vector3f::Ones();
    mass = 0.0f;
    albedo = 0.5f;
    geometryOrientation = Quaternionf::Identity();
    geometry = InvalidResource;
    surface = Surface(Color::White);
    atmosphere.reset();
    rings.reset();
    classification = Unknown;
    visible = 1;
    clickable = 1;
    visibleAsPoint = 1;
    overrideOrbitColor = 0;
    orbitVisibility = UseClassVisibility;
    recomputeCullingRadius();
}

/*! Return the list of all names (non-localized) by which this
 *  body is known.
 */
const vector<string>& Body::getNames() const {
    return names;
}

/*! Return the primary name for the body; if i18n, return the
 *  localized name of the body.
 */
string Body::getName(bool i18n) const {
    if (!i18n)
        return names[0];
    else
        return names[localizedNameIndex];
}

/*! Get the localized name for the body. If no localized name
 *  has been set, the primary name is returned.
 */
string Body::getLocalizedName() const {
    return names[localizedNameIndex];
}

bool Body::hasLocalizedName() const {
    return localizedNameIndex != 0;
}

/*! Set the primary name of the body. The localized name is updated
 *  automatically as well.
 *  Note: setName() is private, and only called from the Body constructor.
 *  It shouldn't be called elsewhere.
 */
void Body::setName(const string& name) {
    names[0] = name;
    string localizedName = _(name.c_str());
    if (name == localizedName) {
        // No localized name; set the localized name index to zero to
        // indicate this.
        localizedNameIndex = 0;
    } else {
        names.push_back(localizedName);
        localizedNameIndex = names.size() - 1;
    }
}

/*! Add a new name for this body. Aliases are non localized.
 */
void Body::addAlias(const string& alias) {
    names.push_back(alias);
    system->addAlias(shared_from_this(), alias);
}

const FrameTreePtr& Body::getOrCreateFrameTree() {
    if (!frameTree)
        frameTree = std::make_shared<FrameTree>(shared_from_this());
    return frameTree;
}

void Body::setTimeline(const TimelinePtr& newTimeline) {
    if (timeline != newTimeline) {
        timeline = newTimeline;
        markChanged();
    }
}

void Body::markChanged() {
    if (timeline)
        timeline->markChanged();
}

void Body::markUpdated() {
    if (frameTree)
        frameTree->markUpdated();
}

const ReferenceFramePtr& Body::getOrbitFrame(double tdb) const {
    return timeline->findPhase(tdb)->orbitFrame();
}

const OrbitPtr& Body::getOrbit(double tdb) const {
    return timeline->findPhase(tdb)->orbit();
}

const ReferenceFramePtr& Body::getBodyFrame(double tdb) const {
    return timeline->findPhase(tdb)->bodyFrame();
}

const RotationModelPtr& Body::getRotationModel(double tdb) const {
    return timeline->findPhase(tdb)->rotationModel();
}

/*! Get the radius of a sphere large enough to contain the primary
 *  geometry of the object: either a mesh or an ellipsoid.
 *  For an irregular (mesh) object, the radius is defined to be
 *  the largest semi-axis of the axis-aligned bounding box.  The
 *  radius of the smallest sphere containing the object is potentially
 *  larger by a factor of sqrt(3).
 *
 *  This method does not consider additional object features
 *  such as rings, atmospheres, or reference marks; use
 *  getCullingRadius() for that.
 */
float Body::getBoundingRadius() const {
    if (geometry.empty())
        return radius;
    else
        return radius * 1.7320508f;  // sqrt(3)
}

/*! Set the semiaxes of a body.
 */
void Body::setSemiAxes(const Vector3f& _semiAxes) {
    semiAxes = _semiAxes;

    // Radius will always be the largest of the three semi axes
    radius = semiAxes.maxCoeff();
    recomputeCullingRadius();
}

/*! Return true if the body is a perfect sphere.
*/
bool Body::isSphere() const {
    return (geometry.empty()) && (semiAxes.x() == semiAxes.y()) && (semiAxes.x() == semiAxes.z());
}

/*! Return true if the body is ellipsoidal, with geometry determined
 *  completely by its semiaxes rather than a triangle based model.
 */
bool Body::isEllipsoid() const {
    return geometry.empty();
}

void Body::setRings(const RingSystem& _rings) {
    rings = std::make_shared<RingSystem>(_rings);
    recomputeCullingRadius();
}

void Body::setAtmosphere(const Atmosphere& _atmosphere) {
    atmosphere = std::make_shared<Atmosphere>(_atmosphere);
    recomputeCullingRadius();
}

// The following four functions are used to get the state of the body
// in universal coordinates:
//    * getPosition
//    * getOrientation
//    * getVelocity
//    * getAngularVelocity

/*! Get the position of the body in the universal coordinate system.
 *  This method uses high-precision coordinates and is thus
 *  slower relative to getAstrocentricPosition(), which works strictly
 *  with standard double precision. For most purposes,
 *  getAstrocentricPosition() should be used instead of the more
 *  general getPosition().
 */
UniversalCoord Body::getPosition(double tdb) const {
    Vector3d position = Vector3d::Zero();

    auto phase = timeline->findPhase(tdb);
    Vector3d p = phase->orbit()->positionAtTime(tdb);
    auto frame = phase->orbitFrame();

    while (frame->getCenter().getType() == Selection::Type_Body) {
        phase = frame->getCenter().body()->timeline->findPhase(tdb);
        position += frame->getOrientation(tdb).conjugate() * p;
        p = phase->orbit()->positionAtTime(tdb);
        frame = phase->orbitFrame();
    }

    position += frame->getOrientation(tdb).conjugate() * p;

    if (frame->getCenter().star())
        return frame->getCenter().star()->getPosition(tdb).offsetKm(position);
    else
        return frame->getCenter().getPosition(tdb).offsetKm(position);
}

/*! Get the orientation of the body in the universal coordinate system.
 */
Quaterniond Body::getOrientation(double tdb) const {
    auto phase = timeline->findPhase(tdb);
    return phase->rotationModel()->orientationAtTime(tdb) * phase->bodyFrame()->getOrientation(tdb);
}

/*! Get the velocity of the body in the universal frame.
 */
Vector3d Body::getVelocity(double tdb) const {
    auto phase = timeline->findPhase(tdb);

    auto orbitFrame = phase->orbitFrame();

    Vector3d v = phase->orbit()->velocityAtTime(tdb);
    v = orbitFrame->getOrientation(tdb).conjugate() * v + orbitFrame->getCenter().getVelocity(tdb);

    if (!orbitFrame->isInertial()) {
        auto self = const_cast<Body*>(this)->shared_from_this();
        Vector3d r = Selection(self).getPosition(tdb).offsetFromKm(orbitFrame->getCenter().getPosition(tdb));
        v += orbitFrame->getAngularVelocity(tdb).cross(r);
    }

    return v;
}

/*! Get the angular velocity of the body in the universal frame.
 */
Vector3d Body::getAngularVelocity(double tdb) const {
    auto phase = timeline->findPhase(tdb);
    Vector3d v = phase->rotationModel()->angularVelocityAtTime(tdb);
    auto bodyFrame = phase->bodyFrame();
    v = bodyFrame->getOrientation(tdb).conjugate() * v;
    if (!bodyFrame->isInertial()) {
        v += bodyFrame->getAngularVelocity(tdb);
    }

    return v;
}

/*! Get the transformation which converts body coordinates into
 *  astrocentric coordinates. Some clarification on the meaning
 *  of 'astrocentric': the position of every solar system body
 *  is ultimately defined with respect to some star or star
 *  system barycenter.
 */
Matrix4d Body::getLocalToAstrocentric(double tdb) const {
    auto phase = timeline->findPhase(tdb);
    Vector3d p = phase->orbitFrame()->convertToAstrocentric(phase->orbit()->positionAtTime(tdb), tdb);
    return Eigen::Transform<double, 3, Affine>(Translation3d(p)).matrix();
}

/*! Get the position of the center of the body in astrocentric ecliptic coordinates.
 */
Vector3d Body::getAstrocentricPosition(double tdb) const {
    // TODO: Switch the iterative method used in getPosition
    auto phase = timeline->findPhase(tdb);
    return phase->orbitFrame()->convertToAstrocentric(phase->orbit()->positionAtTime(tdb), tdb);
}

/*! Get a rotation that converts from the ecliptic frame to the body frame.
 */
Quaterniond Body::getEclipticToFrame(double tdb) const {
    auto phase = timeline->findPhase(tdb);
    return phase->bodyFrame()->getOrientation(tdb);
}

/*! Get a rotation that converts from the ecliptic frame to the body's 
 *  mean equatorial frame.
 */
Quaterniond Body::getEclipticToEquatorial(double tdb) const {
    auto phase = timeline->findPhase(tdb);
    return phase->rotationModel()->equatorOrientationAtTime(tdb) * phase->bodyFrame()->getOrientation(tdb);
}

/*! Get a rotation that converts from the ecliptic frame to this
 *  objects's body fixed frame.
 */
Quaterniond Body::getEclipticToBodyFixed(double tdb) const {
    auto phase = timeline->findPhase(tdb);
    return phase->rotationModel()->orientationAtTime(tdb) * phase->bodyFrame()->getOrientation(tdb);
}

// The body-fixed coordinate system has an origin at the center of the
// body, y-axis parallel to the rotation axis, x-axis through the prime
// meridian, and z-axis at a right angle the xy plane.
Quaterniond Body::getEquatorialToBodyFixed(double tdb) const {
    auto phase = timeline->findPhase(tdb);
    return phase->rotationModel()->spin(tdb);
}

/*! Get a transformation to convert from the object's body fixed frame
 *  to the astrocentric ecliptic frame.
 */
Matrix4d Body::getBodyFixedToAstrocentric(double tdb) const {
    //return getEquatorialToBodyFixed(tdb).toMatrix4() * getLocalToAstrocentric(tdb);
    Matrix4d m = Eigen::Transform<double, 3, Affine>(getEquatorialToBodyFixed(tdb)).matrix();
    return m * getLocalToAstrocentric(tdb);
}

Vector3d Body::planetocentricToCartesian(double lon, double lat, double alt) const {
    double phi = -degToRad(lat) + PI / 2;
    double theta = degToRad(lon) - PI;

    Vector3d pos(cos(theta) * sin(phi), cos(phi), -sin(theta) * sin(phi));

    return pos * (getRadius() + alt);
}

Vector3d Body::planetocentricToCartesian(const Vector3d& lonLatAlt) const {
    return planetocentricToCartesian(lonLatAlt.x(), lonLatAlt.y(), lonLatAlt.z());
}

/*! Convert cartesian body-fixed coordinates to spherical planetocentric
 *  coordinates.
 */
Vector3d Body::cartesianToPlanetocentric(const Vector3d& v) const {
    Vector3d w = v.normalized();

    double lat = PI / 2.0 - acos(w.y());
    double lon = atan2(w.z(), -w.x());

    return Vector3d(lon, lat, v.norm() - getRadius());
}

/*! Convert body-centered ecliptic coordinates to spherical planetocentric
 *  coordinates.
 */
Vector3d Body::eclipticToPlanetocentric(const Vector3d& ecl, double tdb) const {
    Vector3d bf = getEclipticToBodyFixed(tdb) * ecl;
    return cartesianToPlanetocentric(bf);
}

bool Body::extant(double t) const {
    return timeline->includes(t);
}

void Body::getLifespan(double& begin, double& end) const {
    begin = timeline->startTime();
    end = timeline->endTime();
}

float Body::getLuminosity(const Star& sun, float distanceFromSun) const {
    return getLuminosity(sun.getLuminosity(), distanceFromSun);
}

float Body::getLuminosity(float sunLuminosity, float distanceFromSun) const {
    // Compute the total power of the star in Watts
    double power = astro::SOLAR_POWER * sunLuminosity;

    // Compute the irradiance at a distance of 1au from the star in W/m^2
    // double irradiance = power / sphereArea(astro::AUtoKilometers(1.0) * 1000);

    // Compute the irradiance at the body's distance from the star
    double satIrradiance = power / sphereArea(distanceFromSun * 1000);

    // Compute the total energy hitting the planet
    double incidentEnergy = satIrradiance * circleArea(radius * 1000);

    double reflectedEnergy = incidentEnergy * albedo;

    // Compute the luminosity (i.e. power relative to solar power)
    return (float)(reflectedEnergy / astro::SOLAR_POWER);
}

/*! Get the apparent magnitude of the body, neglecting the phase (as if
 *  the body was at opposition.
 */
float Body::getApparentMagnitude(const Star& sun, float distanceFromSun, float distanceFromViewer) const {
    return astro::lumToAppMag(getLuminosity(sun, distanceFromSun), astro::kilometersToLightYears(distanceFromViewer));
}

/*! Get the apparent magnitude of the body, neglecting the phase (as if
 *  the body was at opposition.
 */
float Body::getApparentMagnitude(float sunLuminosity, float distanceFromSun, float distanceFromViewer) const {
    return astro::lumToAppMag(getLuminosity(sunLuminosity, distanceFromSun), astro::kilometersToLightYears(distanceFromViewer));
}

/*! Get the apparent magnitude of the body, corrected for its phase.
 */
float Body::getApparentMagnitude(const Star& sun, const Vector3d& sunPosition, const Vector3d& viewerPosition) const {
    return getApparentMagnitude(sun.getLuminosity(), sunPosition, viewerPosition);
}

/*! Get the apparent magnitude of the body, corrected for its phase.
 */
float Body::getApparentMagnitude(float sunLuminosity, const Vector3d& sunPosition, const Vector3d& viewerPosition) const {
    double distanceToViewer = viewerPosition.norm();
    double distanceToSun = sunPosition.norm();
    float illuminatedFraction = (float)(1.0 + (viewerPosition / distanceToViewer).dot(sunPosition / distanceToSun)) / 2.0f;

    return astro::lumToAppMag(getLuminosity(sunLuminosity, (float)distanceToSun) * illuminatedFraction,
                              (float)astro::kilometersToLightYears(distanceToViewer));
}

void Body::setClassification(int _classification) {
    classification = _classification;
    recomputeCullingRadius();
    markChanged();
}

/*! Return the effective classification of this body used when rendering
 *  orbits. Normally, this is just the classification of the object, but
 *  invisible objects are treated specially: they behave as if they have
 *  the classification of their child objects. This fixes annoyances when
 *  planets are defined with orbits relative to their system barycenters.
 *  For example, Pluto's orbit can seen in a solar system scale view, even
 *  though its orbit is defined relative to the Pluto-Charon barycenter
 *  and is this just a few hundred kilometers in size.
 */
int Body::getOrbitClassification() const {
    if (classification != Invisible || frameTree == NULL) {
        return classification;
    } else {
        int orbitClass = frameTree->childClassMask();
        if (orbitClass & Planet)
            return Planet;
        else if (orbitClass & DwarfPlanet)
            return DwarfPlanet;
        else if (orbitClass & Moon)
            return Moon;
        else if (orbitClass & MinorMoon)
            return MinorMoon;
        else if (orbitClass & Asteroid)
            return Asteroid;
        else if (orbitClass & Spacecraft)
            return Spacecraft;
        else
            return Invisible;
    }
}

string Body::getInfoURL() const {
    return infoURL;
}

void Body::setInfoURL(const string& _infoURL) {
    infoURL = _infoURL;
}

SurfacePtr Body::getAlternateSurface(const string& name) const {
    auto iter = altSurfaces.find(name);
    if (iter == altSurfaces.end())
        return nullptr;

    return iter->second;
}

void Body::addAlternateSurface(const string& name, const SurfacePtr& surface) {
    altSurfaces[name] = surface;
}

vector<string> Body::getAlternateSurfaceNames() const {
    vector<string> names;
    names.reserve(altSurfaces.size());
    for (const auto& entry : altSurfaces) {
        names.emplace_back(entry.first);
    }
    return names;
}

void Body::addLocation(const LocationPtr& loc) {
    assert(loc != NULL);
    locations.push_back(loc);
    loc->setParentBody(shared_from_this());
}

const vector<LocationPtr>& Body::getLocations() const {
    return locations;
}

LocationPtr Body::findLocation(const string& name, bool i18n) const {
    for (const auto& location : locations) {
        if (!UTF8StringCompare(name, location->getName(i18n))) {
            return location;
        }
    }
    return nullptr;
}

// Compute the positions of locations on an irregular object using ray-mesh
// intersections.  This is not automatically done when a location is added
// because it would force the loading of all meshes for objects with
// defined locations; on-demand (i.e. when the object becomes visible to
// a user) loading of meshes is preferred.
void Body::computeLocations() {
    if (locationsComputed)
        return;

    locationsComputed = true;
    return;

#if 0

    // No work to do if there's no mesh, or if the mesh cannot be loaded
    if (geometry == InvalidResource)
        return;
    auto g = GetGeometryManager()->find(geometry);
    if (g == NULL)
        return;

    // TODO: Implement separate radius and bounding radius so that this hack is
    // not necessary.
    double boundingRadius = 2.0;

    for (const auto& location : locations) {
        Vector3f v = location->getPosition();
        float alt = v.norm() - radius;
        if (alt != -radius)
            v.normalize();
        v *= (float)boundingRadius;

        Ray3d ray(v.cast<double>(), -v.cast<double>());
        double t = 0.0;
        if (g->pick(ray, t)) {
            v *= (float)((1.0 - t) * radius + alt);
            location->setPosition(v);
        }
    }
#endif
}

/*! Add a new reference mark.
 */
void Body::addReferenceMark(const ReferenceMarkPtr& refMark) {
    referenceMarks.push_back(refMark);
    recomputeCullingRadius();
}

/*! Remove the first reference mark with the specified tag.
 */
void Body::removeReferenceMark(const string& tag) {
    std::remove_if(referenceMarks.begin(), referenceMarks.end(),
                   [&](const ReferenceMarkPtr& rm) { return rm->getTag() == tag; });
}

/*! Find the first reference mark with the specified tag. If the body has
 *  no reference marks with the specified tag, this method will return
 *  NULL.
 */
ReferenceMarkPtr Body::findReferenceMark(const string& tag) const {
    auto itr = std::find_if(referenceMarks.begin(), referenceMarks.end(),
                            [&](const ReferenceMarkPtr& rm) { return tag == rm->getTag(); });
    if (itr == referenceMarks.end()) {
        return nullptr;
    }
    return *itr;
}

/*! Get the list of reference marks associated with this body. May return
 *  NULL if there are no reference marks.
 */
const vector<ReferenceMarkPtr>& Body::getReferenceMarks() const {
    return referenceMarks;
}

/*! Sets whether or not the object is visible.
 */
void Body::setVisible(bool _visible) {
    visible = _visible ? 1 : 0;
}

/*! Sets whether or not the object can be selected by clicking on
 *  it. If set to false, the object is completely ignored when the
 *  user clicks it, making it possible to select background objects.
 */
void Body::setClickable(bool _clickable) {
    clickable = _clickable ? 1 : 0;
}

/*! Set whether or not the object is visible as a starlike point
 *  when it occupies less than a pixel onscreen. This is appropriate
 *  for planets and moons, but generally not desireable for buildings
 *  or spacecraft components.
 */
void Body::setVisibleAsPoint(bool _visibleAsPoint) {
    visibleAsPoint = _visibleAsPoint ? 1 : 0;
}

/*! The orbitColorOverride flag is set to true if an alternate orbit
 *  color should be used (specified via setOrbitColor) instead of the
 *  default class orbit color.
 */
void Body::setOrbitColorOverridden(bool override) {
    overrideOrbitColor = override ? 1 : 0;
}

/*! Set the visibility policy for the orbit of this object:
 *  - NeverVisible: Never show the orbit of this object.
 *  - UseClassVisibility: (Default) Show the orbit of this object
 *    its class is enabled in the orbit mask.
 *  - AlwaysVisible: Always show the orbit of this object whenever
 *    orbit paths are enabled.
 */
void Body::setOrbitVisibility(VisibilityPolicy _orbitVisibility) {
    orbitVisibility = _orbitVisibility;
}

/*! Set the color used when rendering the orbit. This is only used
 *  when the orbitColorOverride flag is set to true; otherwise, the
 *  standard orbit color for all objects of the class is used.
 */
void Body::setOrbitColor(const Color& c) {
    orbitColor = c;
}

/*! Set whether or not the object should be considered when calculating
 *  secondary illumination (e.g. planetshine.)
 */
void Body::setSecondaryIlluminator(bool enable) {
    if (enable != secondaryIlluminator) {
        markChanged();
        secondaryIlluminator = enable;
    }
}

void Body::recomputeCullingRadius() {
    float r = getBoundingRadius();

    if (rings != NULL)
        r = max(r, rings->outerRadius);

    if (atmosphere != NULL) {
        r = max(r, atmosphere->height);
        r = max(r, atmosphere->cloudHeight);
    }

    for (const auto& rm : referenceMarks) {
        r = max(r, rm->boundingSphereRadius());
    }

    if (classification == Body::Comet)
        r = max(r, astro::AUtoKilometers(1.0f));

    if (r != cullingRadius) {
        cullingRadius = r;
        markChanged();
    }
}

/**** Implementation of PlanetarySystem ****/

/*! Return the equatorial frame for this object. This frame is used as
 *  the default frame for objects in SSC files that orbit non-stellar bodies.
 *  In order to avoid wasting memory, it is created until the first time it
 *  is requested.
 */

PlanetarySystem::PlanetarySystem(const BodyPtr& _primary) :
    star(_primary && _primary->getSystem() ? _primary->getSystem()->getStar() : nullptr), primary(_primary) {
}

PlanetarySystem::PlanetarySystem(const StarPtr& _star) : star(_star) {
}

PlanetarySystem::~PlanetarySystem() {
}

/*! Add a new alias for an object. If an object with the specified
 *  alias already exists in the planetary system, the old entry will
 *  be replaced.
 */
void PlanetarySystem::addAlias(const BodyPtr& body, const string& alias) {
    assert(body->getSystem().get() == this);
    objectIndex.insert({ alias, body });
}

/*! Remove the an alias for an object. This method does nothing
 *  if the alias is not present in the index, or if the alias
 *  refers to a different object.
 */
void PlanetarySystem::removeAlias(const BodyPtr& body, const string& alias) {
    assert(body->getSystem().get() == this);

    ObjectIndex::iterator iter = objectIndex.find(alias);
    if (iter != objectIndex.end()) {
        if (iter->second == body)
            objectIndex.erase(iter);
    }
}

void PlanetarySystem::addBody(const BodyPtr& body) {
    satellites.insert(satellites.end(), body);
    addBodyToNameIndex(body);
}

// Add all aliases for the body to the name index
void PlanetarySystem::addBodyToNameIndex(const BodyPtr& body) {
    const vector<string>& names = body->getNames();
    for (vector<string>::const_iterator iter = names.begin(); iter != names.end(); iter++) {
        objectIndex.insert(make_pair(*iter, body));
    }
}

// Remove all references to the body in the name index.
void PlanetarySystem::removeBodyFromNameIndex(const BodyPtr& body) {
    assert(body->getSystem().get() == this);
    // Erase the object from the object indices
    for (const auto& name : body->getNames()) {
        removeAlias(body, name);
    }
}

void PlanetarySystem::removeBody(const BodyPtr& body) {
    for (auto iter = satellites.begin(); iter != satellites.end(); iter++) {
        if (*iter == body) {
            satellites.erase(iter);
            break;
        }
    }
    removeBodyFromNameIndex(body);
}

void PlanetarySystem::replaceBody(const BodyPtr& oldBody, const BodyPtr& newBody) {
    for (auto iter = satellites.begin(); iter != satellites.end(); iter++) {
        if (*iter == oldBody) {
            *iter = newBody;
            break;
        }
    }

    removeBodyFromNameIndex(oldBody);
    addBodyToNameIndex(newBody);
}

/*! Find a body with the specified name within a planetary system.
 * 
 *  deepSearch: if true, recursively search the systems of child objects
 *  i18n: if true, allow matching of localized body names. When responding
 *    to a user query, this flag should be true. In other cases--such
 *    as resolving an object name in an ssc file--it should be false. Otherwise,
 *    object lookup will behave differently based on the locale.
 */
BodyPtr PlanetarySystem::find(const string& _name, bool deepSearch, bool i18n) const {
    ObjectIndex::const_iterator firstMatch = objectIndex.find(_name);
    if (firstMatch != objectIndex.end()) {
        auto matchedBody = firstMatch->second;

        if (i18n) {
            return matchedBody;
        } else {
            // Ignore localized names
            if (!matchedBody->hasLocalizedName() || _name != matchedBody->getLocalizedName())
                return matchedBody;
        }
    }

    if (deepSearch) {
        for (auto iter = satellites.begin(); iter != satellites.end(); iter++) {
            if (UTF8StringCompare((*iter)->getName(i18n), _name) == 0) {
                return *iter;
            } else if (deepSearch && (*iter)->getSatellites() != NULL) {
                auto body = (*iter)->getSatellites()->find(_name, deepSearch, i18n);
                if (body) {
                    return body;
                }
            }
        }
    }

    return NULL;
}

bool PlanetarySystem::traverse(const TraversalFunc& func, void* info) const {
    for (int i = 0; i < getSystemSize(); i++) {
        auto body = getBody(i);
        // assert(body != NULL);
        if (!func(body, info))
            return false;
        if (body->getSatellites() != NULL) {
            if (!body->getSatellites()->traverse(func, info))
                return false;
        }
    }

    return true;
}

std::vector<std::string> PlanetarySystem::getCompletion(const std::string& _name, bool deepSearch) const {
    std::vector<std::string> completion;
    auto _name_length = UTF8Length(_name);

    // Search through all names in this planetary system.
    for (const auto& entry : objectIndex) {
        const string& alias = entry.first;
        if (UTF8StringCompare(alias, _name, _name_length) == 0) {
            completion.push_back(alias);
        }
    }

    // Scan child objects
    if (deepSearch) {
        for (auto iter = satellites.begin(); iter != satellites.end(); iter++) {
            if ((*iter)->getSatellites() != NULL) {
                vector<string> bodies = (*iter)->getSatellites()->getCompletion(_name);
                completion.insert(completion.end(), bodies.begin(), bodies.end());
            }
        }
    }

    return completion;
}

/*! Get the order of the object in the list of children. Returns -1 if the
 *  specified body is not a child object.
 */
size_t PlanetarySystem::getOrder(const BodyPtr& body) const {
    auto iter = std::find(satellites.begin(), satellites.end(), body);
    if (iter == satellites.end())
        return -1;
    else
        return iter - satellites.begin();
}
