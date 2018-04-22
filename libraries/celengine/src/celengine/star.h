// star.h
//
// Copyright (C) 2001-2009, the Celestia Development Team
// Original version by Chris Laurel <claurel@gmail.com>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#ifndef _CELENGINE_STAR_H_
#define _CELENGINE_STAR_H_

#include <vector>

#include <Eigen/Core>

#include <celutil/reshandle.h>
#include <celutil/color.h>
#include <celephem/rotation.h>

#include "forward.h"
#include "univcoord.h"
#include "celestia.h"
#include "stellarclass.h"
#include "multitexture.h"

class Orbit;
class Star;

class StarDetails {
    friend class Star;

public:
    using Pointer = std::shared_ptr<StarDetails>;
    StarDetails();
    StarDetails(const StarDetails&);

    ~StarDetails() {}

private:
    // Prohibit assignment of StarDetails objects
    StarDetails& operator=(const StarDetails&);

public:
    inline float getRadius() const { return radius; }
    inline float getTemperature() const { return temperature; }
    inline const std::string& getGeometry() const { return geometry; }
    inline const MultiResTexture& getTexture() const { return texture; }
    inline const OrbitPtr& getOrbit() const { return orbit; }
    inline float getOrbitalRadius() const { return orbitalRadius; }
    inline const char* getSpectralType() const { return spectralType; }
    inline float getBolometricCorrection() const { return bolometricCorrection; }
    inline const StarPtr& getOrbitBarycenter() const { return barycenter; }
    inline bool getVisibility() const { return visible; }
    inline const RotationModelPtr& getRotationModel() const { return rotationModel; }
    inline Eigen::Vector3f getEllipsoidSemiAxes() const { return semiAxes; }
    const std::string& getInfoURL() const;

    void setRadius(float _radius) { radius = _radius; }
    void setTemperature(float _temperature) { temperature = _temperature; }
    void setSpectralType(const std::string&);
    void setBolometricCorrection(float correction) { bolometricCorrection = correction; }
    void setTexture(const MultiResTexture& tex) { texture = tex; }
    void setGeometry(const std::string& rh) { geometry = rh; }
    void setOrbit(const OrbitPtr&);
    void setOrbitBarycenter(const StarPtr&);
    void setOrbitalRadius(float);
    void computeOrbitalRadius();
    void setVisibility(bool b) { visible = b; }
    void setRotationModel(const RotationModelPtr& rm) { rotationModel = rm; }
    void setEllipsoidSemiAxes(const Eigen::Vector3f& v) { semiAxes = v; }
    void setInfoURL(const std::string& _infoURL);

    bool shared() const { return isShared; }

    enum
    {
        KnowRadius = 0x1,
        KnowRotation = 0x2,
        KnowTexture = 0x4,
    };
    inline uint32_t getKnowledge() const { return knowledge; }
    inline bool getKnowledge(uint32_t knowledgeFlags) const { return ((knowledge & knowledgeFlags) == knowledgeFlags); }
    void setKnowledge(uint32_t _knowledge) { knowledge = _knowledge; }
    void addKnowledge(uint32_t _knowledge) { knowledge |= _knowledge; }

private:
    void addOrbitingStar(const StarPtr&);

private:
    float radius;
    float temperature;
    float bolometricCorrection;

    uint32_t knowledge;
    bool visible;
    char spectralType[8];

    MultiResTexture texture;
    std::string geometry;

    OrbitPtr orbit;
    float orbitalRadius;
    StarPtr barycenter;

    RotationModelPtr rotationModel;

    Eigen::Vector3f semiAxes;

    std::shared_ptr<std::string> infoURL;

    std::vector<StarPtr> orbitingStars;
    bool isShared;

public:
    struct StarTextureSet {
        MultiResTexture defaultTex;
        MultiResTexture neutronStarTex;
        MultiResTexture starTex[StellarClass::Spectral_Count];
    };

public:
    static StarDetails::Pointer GetStarDetails(const StellarClass&);
    static StarDetails::Pointer CreateStandardStarType(const std::string& _specType, float _temperature, float _rotationPeriod);

    static StarDetails::Pointer GetNormalStarDetails(StellarClass::SpectralClass specClass,
                                                     uint32_t subclass,
                                                     StellarClass::LuminosityClass lumClass);
    static StarDetails::Pointer GetWhiteDwarfDetails(StellarClass::SpectralClass specClass, uint32_t subclass);
    static StarDetails::Pointer GetNeutronStarDetails();
    static StarDetails::Pointer GetBlackHoleDetails();
    static StarDetails::Pointer GetBarycenterDetails();

    static void SetStarTextures(const StarTextureSet& _starTextures) { starTextures = _starTextures; }

private:
    static StarTextureSet starTextures;
};

class Star : public Object {
public:
    using Pointer = std::shared_ptr<Star>;
    inline Star() {}
    ~Star() {}

    inline uint32_t getCatalogNumber() const { return catalogNumber; }

    /** This getPosition() method returns the approximate star position; that is,
     *  star position without any orbital motion taken into account.  For a
     *  star in an orbit, the position should be set to the 'root' barycenter
     *  of the system.
     */
    Eigen::Vector3f getPosition() const { return position; }

    float getAbsoluteMagnitude() const { return absMag; }

    float getApparentMagnitude(float) const;
    float getLuminosity() const;

    // Return the exact position of the star, accounting for its orbit
    UniversalCoord getPosition(double t) const;
    UniversalCoord getOrbitBarycenterPosition(double t) const;

    Eigen::Vector3d getVelocity(double t) const;

    void setCatalogNumber(uint32_t n) { catalogNumber = n; }
    void setPosition(float x, float y, float z) { position = Eigen::Vector3f(x, y, z); }
    void setPosition(const Eigen::Vector3f& positionLy) { position = positionLy; }
    void setAbsoluteMagnitude(float mag) { absMag = mag; }
    void setLuminosity(float);

    const StarDetails::Pointer& getDetails() const { return details; }
    void setDetails(const StarDetails::Pointer& sd) { details = sd; }
    void setOrbitBarycenter(const StarPtr&);
    void computeOrbitalRadius() { details->computeOrbitalRadius(); }
    void setRotationModel(const RotationModelPtr& rm) { details->setRotationModel(rm); }
    void addOrbitingStar(const StarPtr&);
    inline const std::vector<StarPtr>& getOrbitingStars() const { return details->orbitingStars; }

    // Accessor methods that delegate to StarDetails
    float getRadius() const;
    inline float getTemperature() const { return details->getTemperature(); }
    inline const char* getSpectralType() const { return details->getSpectralType(); }
    inline float getBolometricMagnitude() const { return absMag + details->getBolometricCorrection(); }
    MultiResTexture getTexture() const { return details->getTexture(); }
    const std::string& getGeometry() const { return details->getGeometry(); }
    inline const OrbitPtr& getOrbit() const { return details->getOrbit(); }
    inline float getOrbitalRadius() const { return details->getOrbitalRadius(); }
    inline const StarPtr& getOrbitBarycenter() const { return details->getOrbitBarycenter(); }
    inline bool getVisibility() const { return details->getVisibility(); }
    inline uint32_t getKnowledge() const { return details->getKnowledge(); }
    inline const RotationModelPtr& getRotationModel() const { return details->getRotationModel(); }
    inline Eigen::Vector3f getEllipsoidSemiAxes() const { return details->getEllipsoidSemiAxes(); }
    const std::string& getInfoURL() const { return details->getInfoURL(); }

    enum
    {
        MaxTychoCatalogNumber = 0xf0000000,
        InvalidCatalogNumber = (~(uint32_t)0x0),
    };

private:
    uint32_t catalogNumber{ (uint32_t)InvalidCatalogNumber };
    Eigen::Vector3f position{ 0, 0, 0 };
    float absMag{ 4.83f };
    StarDetails::Pointer details;
};

#endif  // _CELENGINE_STAR_H_
