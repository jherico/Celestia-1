// asterism.cpp
//
// Copyright (C) 2001, Chris Laurel <claurel@shatters.net>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#include "asterism.h"

#include <algorithm>

#include <celutil/util.h>
#include <celutil/debug.h>

#include "star.h"
#include "stardb.h"
#include "parser.h"

using namespace std;

Asterism::Asterism(const string& _name) : name(_name) {
    i18nName = dgettext("celestia_constellations", _name.c_str());
}

Asterism::~Asterism() {
}

string Asterism::getName(bool i18n) const {
    return i18n ? i18nName : name;
}

uint32_t Asterism::getChainCount() const {
    return static_cast<uint32_t>(chains.size());
}

const Asterism::Chain& Asterism::getChain(uint32_t index) const {
    return *chains[index];
}

void Asterism::addChain(Asterism::Chain& chain) {
    chains.insert(chains.end(), std::make_shared<Chain>(chain));
}

/*! Return whether the constellation is visible.
 */
bool Asterism::getActive() const {
    return active;
}

/*! Set whether or not the constellation is visible.
 */
void Asterism::setActive(bool _active) {
    active = _active;
}

/*! Get the override color for this constellation.
 */
Color Asterism::getOverrideColor() const {
    return color;
}

/*! Set an override color for the constellation. If this method isn't
 *  called, the constellation is drawn in the render's default color
 *  for contellations. Calling unsetOverrideColor will remove the
 *  override color.
 */
void Asterism::setOverrideColor(const Color& c) {
    color = c;
    useOverrideColor = true;
}

/*! Make this constellation appear in the default color (undoing any
 *  calls to setOverrideColor.
 */
void Asterism::unsetOverrideColor() {
    useOverrideColor = false;
}

/*! Return true if this constellation has a custom color, or false
 *  if it should be drawn in the default color.
 */
bool Asterism::isColorOverridden() const {
    return useOverrideColor;
}

AsterismListPtr ReadAsterismList(istream& in, const StarDatabase& stardb) {
    auto asterisms = std::make_shared<AsterismList>();
    Tokenizer tokenizer(&in);
    Parser parser(&tokenizer);

    while (tokenizer.nextToken() != Tokenizer::TokenEnd) {
        if (tokenizer.getTokenType() != Tokenizer::TokenString) {
            DPRINTF(0, "Error parsing asterism file.\n");
            return nullptr;
        }

        string name = tokenizer.getStringValue();
        Asterism::Pointer ast = std::make_shared<Asterism>(name);

        auto chainsValue = parser.readValue();
        if (chainsValue == NULL || chainsValue->getType() != Value::ArrayType) {
            DPRINTF(0, "Error parsing asterism %s\n", name.c_str());
            return nullptr;
        }

        auto chains = chainsValue->getArray();

        for (const auto& chain : *chains) {
            if (chain->getType() == Value::ArrayType) {
                Asterism::Chain newChain;
                for (const auto& iter : *(chain->getArray())) {
                    if (iter->getType() == Value::StringType) {
                        auto star = stardb.find(iter->getString());
                        if (star) {
                            newChain.push_back(star->getPosition());
                        }
                    }
                }
                ast->addChain(newChain);
            }
        }
        asterisms->insert(asterisms->end(), ast);
    }

    return asterisms;
}
