// constellation.cpp
//
// Copyright (C) 2001, Chris Laurel <claurel@shatters.net>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#include "constellation.h"

#include <iostream>
#include <vector>

#include <celutil/util.h>

#include "celestia.h"

using namespace std;

std::vector<Constellation::Pointer>& Constellation::getConstellations() {
    static std::vector<Constellation::Pointer> constellationInfo{
        std::make_shared<Constellation>("Aries", "Arietis", "Ari"),
        std::make_shared<Constellation>("Taurus", "Tauri", "Tau"),
        std::make_shared<Constellation>("Gemini", "Geminorum", "Gem"),
        std::make_shared<Constellation>("Cancer", "Cancri", "Cnc"),
        std::make_shared<Constellation>("Leo", "Leonis", "Leo"),
        std::make_shared<Constellation>("Virgo", "Virginis", "Vir"),
        std::make_shared<Constellation>("Libra", "Librae", "Lib"),
        std::make_shared<Constellation>("Scorpius", "Scorpii", "Sco"),
        std::make_shared<Constellation>("Sagittarius", "Sagittarii", "Sgr"),
        std::make_shared<Constellation>("Capricornus", "Capricorni", "Cap"),
        std::make_shared<Constellation>("Aquarius", "Aquarii", "Aqr"),
        std::make_shared<Constellation>("Pisces", "Piscium", "Psc"),
        std::make_shared<Constellation>("Ursa Major", "Ursae Majoris", "UMa"),
        std::make_shared<Constellation>("Ursa Minor", "Ursae Minoris", "UMi"),
        std::make_shared<Constellation>("Bootes", "Bootis", "Boo"),
        std::make_shared<Constellation>("Orion", "Orionis", "Ori"),
        std::make_shared<Constellation>("Canis Major", "Canis Majoris", "CMa"),
        std::make_shared<Constellation>("Canis Minor", "Canis Minoris", "CMi"),
        std::make_shared<Constellation>("Lepus", "Leporis", "Lep"),
        std::make_shared<Constellation>("Perseus", "Persei", "Per"),
        std::make_shared<Constellation>("Andromeda", "Andromedae", "And"),
        std::make_shared<Constellation>("Cassiopeia", "Cassiopeiae", "Cas"),
        std::make_shared<Constellation>("Cepheus", "Cephei", "Cep"),
        std::make_shared<Constellation>("Cetus", "Ceti", "Cet"),
        std::make_shared<Constellation>("Pegasus", "Pegasi", "Peg"),
        std::make_shared<Constellation>("Carina", "Carinae", "Car"),
        std::make_shared<Constellation>("Puppis", "Puppis", "Pup"),
        std::make_shared<Constellation>("Vela", "Velorum", "Vel"),
        std::make_shared<Constellation>("Hercules", "Herculis", "Her"),
        std::make_shared<Constellation>("Hydra", "Hydrae", "Hya"),
        std::make_shared<Constellation>("Centaurus", "Centauri", "Cen"),
        std::make_shared<Constellation>("Lupus", "Lupi", "Lup"),
        std::make_shared<Constellation>("Ara", "Arae", "Ara"),
        std::make_shared<Constellation>("Ophiuchus", "Ophiuchi", "Oph"),
        std::make_shared<Constellation>("Serpens", "Serpentis", "Ser"),
        std::make_shared<Constellation>("Aquila", "Aquilae", "Aql"),
        std::make_shared<Constellation>("Auriga", "Aurigae", "Aur"),
        std::make_shared<Constellation>("Corona Australis", "Coronae Australis", "CrA"),
        std::make_shared<Constellation>("Corona Borealis", "Coronae Borealis", "CrB"),
        std::make_shared<Constellation>("Corvus", "Corvi", "Crv"),
        std::make_shared<Constellation>("Crater", "Crateris", "Crt"),
        std::make_shared<Constellation>("Cygnus", "Cygni", "Cyg"),
        std::make_shared<Constellation>("Delphinus", "Delphini", "Del"),
        std::make_shared<Constellation>("Draco", "Draconis", "Dra"),
        std::make_shared<Constellation>("Equuleus", "Equulei", "Equ"),
        std::make_shared<Constellation>("Eridanus", "Eridani", "Eri"),
        std::make_shared<Constellation>("Lyra", "Lyrae", "Lyr"),
        std::make_shared<Constellation>("Piscis Austrinus", "Piscis Austrini", "PsA"),
        std::make_shared<Constellation>("Sagitta", "Sagittae", "Sge"),
        std::make_shared<Constellation>("Triangulum", "Trianguli", "Tri"),
        std::make_shared<Constellation>("Antlia", "Antliae", "Ant"),
        std::make_shared<Constellation>("Apus", "Apodis", "Aps"),
        std::make_shared<Constellation>("Caelum", "Caeli", "Cae"),
        std::make_shared<Constellation>("Camelopardalis", "Camelopardalis", "Cam"),
        std::make_shared<Constellation>("Canes Venatici", "Canum Venaticorum", "CVn"),
        std::make_shared<Constellation>("Chamaeleon", "Chamaeleontis", "Cha"),
        std::make_shared<Constellation>("Circinus", "Circini", "Cir"),
        std::make_shared<Constellation>("Columba", "Columbae", "Col"),
        std::make_shared<Constellation>("Coma Berenices", "Comae Berenices", "Com"),
        std::make_shared<Constellation>("Crux", "Crucis", "Cru"),
        std::make_shared<Constellation>("Dorado", "Doradus", "Dor"),
        std::make_shared<Constellation>("Fornax", "Fornacis", "For"),
        std::make_shared<Constellation>("Grus", "Gruis", "Gru"),
        std::make_shared<Constellation>("Horologium", "Horologii", "Hor"),
        std::make_shared<Constellation>("Hydrus", "Hydri", "Hyi"),
        std::make_shared<Constellation>("Indus", "Indi", "Ind"),
        std::make_shared<Constellation>("Lacerta", "Lacertae", "Lac"),
        std::make_shared<Constellation>("Leo Minor", "Leonis Minoris", "LMi"),
        std::make_shared<Constellation>("Lynx", "Lyncis", "Lyn"),
        std::make_shared<Constellation>("Microscopium", "Microscopii", "Mic"),
        std::make_shared<Constellation>("Monoceros", "Monocerotis", "Mon"),
        std::make_shared<Constellation>("Mensa", "Mensae", "Men"),
        std::make_shared<Constellation>("Musca", "Muscae", "Mus"),
        std::make_shared<Constellation>("Norma", "Normae", "Nor"),
        std::make_shared<Constellation>("Octans", "Octantis", "Oct"),
        std::make_shared<Constellation>("Pavo", "Pavonis", "Pav"),
        std::make_shared<Constellation>("Phoenix", "Phoenicis", "Phe"),
        std::make_shared<Constellation>("Pictor", "Pictoris", "Pic"),
        std::make_shared<Constellation>("Pyxis", "Pyxidis", "Pyx"),
        std::make_shared<Constellation>("Reticulum", "Reticuli", "Ret"),
        std::make_shared<Constellation>("Sculptor", "Sculptoris", "Scl"),
        std::make_shared<Constellation>("Scutum", "Scuti", "Sct"),
        std::make_shared<Constellation>("Sextans", "Sextantis", "Sex"),
        std::make_shared<Constellation>("Telescopium", "Telescopii", "Tel"),
        std::make_shared<Constellation>("Triangulum Australe", "Trianguli Australis", "TrA"),
        std::make_shared<Constellation>("Tucana", "Tucanae", "Tuc"),
        std::make_shared<Constellation>("Volans", "Volantis", "Vol"),
        std::make_shared<Constellation>("Vulpecula", "Vulpeculae", "Vul"),
    };
    return constellationInfo;
}

Constellation::Constellation(const std::string& _name, const std::string& _genitive, const std::string& _abbrev) :
    name(_name), genitive(_genitive), abbrev(_abbrev) {
}

Constellation::Pointer Constellation::getConstellation(uint32_t n) {
    const auto& constellations = getConstellations();
    if (n >= constellations.size()) {
        return nullptr;
    }
    return getConstellations()[n];
}

Constellation::Pointer Constellation::getConstellation(const string& name) {
    for (const auto& c : getConstellations()) {
        if (compareIgnoringCase(name, c->abbrev) == 0 || compareIgnoringCase(name, c->genitive) == 0 ||
            compareIgnoringCase(name, c->name) == 0) {
            return c;
        }
    }

    return NULL;
}

const string& Constellation::getName() const {
    return name;
}

const string& Constellation::getGenitive() const {
    return genitive;
}

const string& Constellation::getAbbreviation() const {
    return abbrev;
}
