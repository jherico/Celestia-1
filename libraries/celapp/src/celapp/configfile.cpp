// configfile.cpp
//
// Copyright (C) 2001-2009, the Celestia Development Team
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#include "configfile.h"

#include <iostream>
#include <fstream>
#include <cassert>
#include <celutil/debug.h>
#include <celutil/directory.h>
#include <celutil/util.h>
#include <celengine/celestia.h>
#include <celengine/texmanager.h>

using namespace std;

static unsigned int getUint(const HashPtr& params, const string& paramName, unsigned int defaultValue) {
    double value = 0.0;
    if (params->getNumber(paramName, value))
        return (unsigned int)value;
    else
        return defaultValue;
}

CelestiaConfig::Pointer ReadCelestiaConfig(const string& filename) {
    CelestiaConfig::Pointer config;
    return ReadCelestiaConfig(filename, config);
}

CelestiaConfig::Pointer ReadCelestiaConfig(const string& filename, CelestiaConfig::Pointer& config) {
    ifstream configFile(filename.c_str());
    if (!configFile.good()) {
        DPRINTF(0, "Error opening config file '%s'.\n", filename.c_str());
        return config;
    }

    Tokenizer tokenizer(&configFile);
    Parser parser(&tokenizer);

    if (tokenizer.nextToken() != Tokenizer::TokenName) {
        DPRINTF(0, "%s:%d 'Configuration' expected.\n", filename.c_str(), tokenizer.getLineNumber());
        return config;
    }

    if (tokenizer.getStringValue() != "Configuration") {
        DPRINTF(0, "%s:%d 'Configuration' expected.\n", filename.c_str(), tokenizer.getLineNumber());
        return config;
    }

    auto configParamsValue = parser.readValue();
    if (configParamsValue == NULL || configParamsValue->getType() != Value::HashType) {
        DPRINTF(0, "%s: Bad configuration file.\n", filename.c_str());
        return config;
    }

    const auto& configParams = configParamsValue->getHash();

    if (config == NULL)
        config = std::make_shared<CelestiaConfig>();

#ifdef CELX
    config->configParams = configParams;
    configParams->getString("LuaHook", config->luaHook);
    config->luaHook = WordExp(config->luaHook);
#endif

    config->faintestVisible = 6.0f;
    configParams->getNumber("FaintestVisibleMagnitude", config->faintestVisible);
    configParams->getString("FavoritesFile", config->favoritesFile);
    config->favoritesFile = WordExp(config->favoritesFile);
    configParams->getString("DestinationFile", config->destinationsFile);
    config->destinationsFile = WordExp(config->destinationsFile);
    configParams->getString("InitScript", config->initScriptFile);
    config->initScriptFile = WordExp(config->initScriptFile);
    configParams->getString("DemoScript", config->demoScriptFile);
    config->demoScriptFile = WordExp(config->demoScriptFile);
    configParams->getString("AsterismsFile", config->asterismsFile);
    config->asterismsFile = WordExp(config->asterismsFile);
    configParams->getString("BoundariesFile", config->boundariesFile);
    config->boundariesFile = WordExp(config->boundariesFile);
    configParams->getString("StarDatabase", config->starDatabaseFile);
    config->starDatabaseFile = WordExp(config->starDatabaseFile);
    configParams->getString("StarNameDatabase", config->starNamesFile);
    config->starNamesFile = WordExp(config->starNamesFile);
    configParams->getString("HDCrossIndex", config->HDCrossIndexFile);
    config->HDCrossIndexFile = WordExp(config->HDCrossIndexFile);
    configParams->getString("SAOCrossIndex", config->SAOCrossIndexFile);
    config->SAOCrossIndexFile = WordExp(config->SAOCrossIndexFile);
    configParams->getString("GlieseCrossIndex", config->GlieseCrossIndexFile);
    config->GlieseCrossIndexFile = WordExp(config->GlieseCrossIndexFile);
    configParams->getString("Font", config->mainFont);
    configParams->getString("LabelFont", config->labelFont);
    configParams->getString("TitleFont", config->titleFont);
    configParams->getString("LogoTexture", config->logoTextureFile);
    configParams->getString("Cursor", config->cursor);

    double aaSamples = 1;
    configParams->getNumber("AntialiasingSamples", aaSamples);
    config->aaSamples = (unsigned int)aaSamples;

    config->hdr = false;
    configParams->getBoolean("HighDynamicRange", config->hdr);

    config->rotateAcceleration = 120.0f;
    configParams->getNumber("RotateAcceleration", config->rotateAcceleration);
    config->mouseRotationSensitivity = 1.0f;
    configParams->getNumber("MouseRotationSensitivity", config->mouseRotationSensitivity);
    config->reverseMouseWheel = false;
    configParams->getBoolean("ReverseMouseWheel", config->reverseMouseWheel);
    configParams->getString("ScriptScreenshotDirectory", config->scriptScreenshotDirectory);
    config->scriptScreenshotDirectory = WordExp(config->scriptScreenshotDirectory);
    config->scriptSystemAccessPolicy = "ask";
    configParams->getString("ScriptSystemAccessPolicy", config->scriptSystemAccessPolicy);

    config->orbitWindowEnd = 0.5f;
    configParams->getNumber("OrbitWindowEnd", config->orbitWindowEnd);
    config->orbitPeriodsShown = 1.0f;
    configParams->getNumber("OrbitPeriodsShown", config->orbitPeriodsShown);
    config->linearFadeFraction = 0.0f;
    configParams->getNumber("LinearFadeFraction", config->linearFadeFraction);

    config->ringSystemSections = getUint(configParams, "RingSystemSections", 100);
    config->orbitPathSamplePoints = getUint(configParams, "OrbitPathSamplePoints", 100);
    config->shadowTextureSize = getUint(configParams, "ShadowTextureSize", 256);
    config->eclipseTextureSize = getUint(configParams, "EclipseTextureSize", 128);

    config->consoleLogRows = getUint(configParams, "LogSize", 200);

    auto solarSystemsVal = configParams->getValue("SolarSystemCatalogs");
    if (solarSystemsVal != NULL) {
        if (solarSystemsVal->getType() != Value::ArrayType) {
            DPRINTF(0, "%s: SolarSystemCatalogs must be an array.\n", filename.c_str());
        } else {
            const auto& solarSystems = solarSystemsVal->getArray();
            // assert(solarSystems != NULL);

            for (const auto& catalogNameVal : *solarSystems) {
                // assert(catalogNameVal != NULL);
                if (catalogNameVal->getType() == Value::StringType) {
                    config->solarSystemFiles.insert(config->solarSystemFiles.end(), WordExp(catalogNameVal->getString()));
                } else {
                    DPRINTF(0, "%s: Solar system catalog name must be a string.\n", filename.c_str());
                }
            }
        }
    }

    auto starCatalogsVal = configParams->getValue("StarCatalogs");
    if (starCatalogsVal != NULL) {
        if (starCatalogsVal->getType() != Value::ArrayType) {
            DPRINTF(0, "%s: StarCatalogs must be an array.\n", filename.c_str());
        } else {
            const auto& starCatalogs = starCatalogsVal->getArray();
            assert(starCatalogs != NULL);

            for (const auto& catalogNameVal : *starCatalogs) {
                assert(catalogNameVal != NULL);
                if (catalogNameVal->getType() == Value::StringType) {
                    config->starCatalogFiles.push_back(WordExp(catalogNameVal->getString()));
                } else {
                    DPRINTF(0, "%s: Star catalog name must be a string.\n", filename.c_str());
                }
            }
        }
    }

    auto dsoCatalogsVal = configParams->getValue("DeepSkyCatalogs");
    if (dsoCatalogsVal != NULL) {
        if (dsoCatalogsVal->getType() != Value::ArrayType) {
            DPRINTF(0, "%s: DeepSkyCatalogs must be an array.\n", filename.c_str());
        } else {
            const auto& dsoCatalogs = dsoCatalogsVal->getArray();
            assert(dsoCatalogs != NULL);

            for (const auto& catalogNameVal : *dsoCatalogs) {
                assert(catalogNameVal != NULL);
                if (catalogNameVal->getType() == Value::StringType) {
                    config->dsoCatalogFiles.push_back(WordExp(catalogNameVal->getString()));
                } else {
                    DPRINTF(0, "%s: DeepSky catalog name must be a string.\n", filename.c_str());
                }
            }
        }
    }

    auto extrasDirsVal = configParams->getValue("ExtrasDirectories");
    if (extrasDirsVal != NULL) {
        if (extrasDirsVal->getType() == Value::ArrayType) {
            const auto& extrasDirs = extrasDirsVal->getArray();
            assert(extrasDirs != NULL);

            for (const auto& dirNameVal : *extrasDirs) {
                if (dirNameVal->getType() == Value::StringType) {
                    config->extrasDirs.insert(config->extrasDirs.end(), WordExp(dirNameVal->getString()));
                } else {
                    DPRINTF(0, "%s: Extras directory name must be a string.\n", filename.c_str());
                }
            }
        } else if (extrasDirsVal->getType() == Value::StringType) {
            config->extrasDirs.insert(config->extrasDirs.end(), WordExp(extrasDirsVal->getString()));
        } else {
            DPRINTF(0, "%s: ExtrasDirectories must be an array or string.\n", filename.c_str());
        }
    }

    auto ignoreExtVal = configParams->getValue("IgnoreGLExtensions");
    if (ignoreExtVal != NULL) {
        if (ignoreExtVal->getType() != Value::ArrayType) {
            DPRINTF(0, "%s: IgnoreGLExtensions must be an array.\n", filename.c_str());
        } else {
            const auto& ignoreExt = ignoreExtVal->getArray();

            for (const auto& extVal : *ignoreExt) {
                if (extVal->getType() == Value::StringType) {
                    config->ignoreGLExtensions.push_back(extVal->getString());
                } else {
                    DPRINTF(0, "%s: extension name must be a string.\n", filename.c_str());
                }
            }
        }
    }

    auto starTexValue = configParams->getValue("StarTextures");
    if (starTexValue != NULL) {
        if (starTexValue->getType() != Value::HashType) {
            DPRINTF(0, "%s: StarTextures must be a property list.\n", filename.c_str());
        } else {
            const auto& starTexTable = starTexValue->getHash();
            string starTexNames[StellarClass::Spectral_Count];
            starTexTable->getString("O", starTexNames[StellarClass::Spectral_O]);
            starTexTable->getString("B", starTexNames[StellarClass::Spectral_B]);
            starTexTable->getString("A", starTexNames[StellarClass::Spectral_A]);
            starTexTable->getString("F", starTexNames[StellarClass::Spectral_F]);
            starTexTable->getString("G", starTexNames[StellarClass::Spectral_G]);
            starTexTable->getString("K", starTexNames[StellarClass::Spectral_K]);
            starTexTable->getString("M", starTexNames[StellarClass::Spectral_M]);
            starTexTable->getString("R", starTexNames[StellarClass::Spectral_R]);
            starTexTable->getString("S", starTexNames[StellarClass::Spectral_S]);
            starTexTable->getString("N", starTexNames[StellarClass::Spectral_N]);
            starTexTable->getString("WC", starTexNames[StellarClass::Spectral_WC]);
            starTexTable->getString("WN", starTexNames[StellarClass::Spectral_WN]);
            starTexTable->getString("Unknown", starTexNames[StellarClass::Spectral_Unknown]);
            starTexTable->getString("L", starTexNames[StellarClass::Spectral_L]);
            starTexTable->getString("T", starTexNames[StellarClass::Spectral_T]);
            starTexTable->getString("C", starTexNames[StellarClass::Spectral_C]);

            // One texture for all white dwarf types; not sure if this needs to be
            // changed. White dwarfs vary widely in temperature, so texture choice
            // should probably be based on that instead of spectral type.
            starTexTable->getString("WD", starTexNames[StellarClass::Spectral_D]);

            string neutronStarTexName;
            if (starTexTable->getString("NeutronStar", neutronStarTexName)) {
                config->starTextures.neutronStarTex.setTexture(neutronStarTexName, "textures");
            }

            string defaultTexName;
            if (starTexTable->getString("Default", defaultTexName)) {
                config->starTextures.defaultTex.setTexture(defaultTexName, "textures");
            }

            for (unsigned int i = 0; i < (unsigned int)StellarClass::Spectral_Count; i++) {
                if (starTexNames[i] != "") {
                    config->starTextures.starTex[i].setTexture(starTexNames[i], "textures");
                }
            }
        }
    }

    // TODO: not cleaning up properly here--we're just saving the hash, not the instance of Value
    config->params = configParams;
    return config;
}

float CelestiaConfig::getFloatValue(const string& name) {
    assert(params != NULL);

    double x = 0.0;
    params->getNumber(name, x);

    return (float)x;
}

const string& CelestiaConfig::getStringValue(const string& name) {
    assert(params != NULL);
    static const string EMPTY("");

    auto v = params->getValue(name);
    if (v == NULL || v->getType() != Value::StringType)
        return EMPTY;

    return v->getString();
}
