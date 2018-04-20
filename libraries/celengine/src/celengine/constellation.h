// constellation.h
//
// Copyright (C) 2001, Chris Laurel <claurel@shatters.net>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#ifndef _CONSTELLATION_H_
#define _CONSTELLATION_H_

#include <string>
#include <memory>
#include <vector>

class Constellation {
public:
    using Pointer = std::shared_ptr<Constellation>;
    static Pointer getConstellation(uint32_t);
    static Pointer getConstellation(const std::string&);

    const std::string& getName() const;
    const std::string& getGenitive() const;
    const std::string& getAbbreviation() const;

    Constellation(const std::string& _name, const std::string& _genitive, const std::string& _abbrev);
private:
    static std::vector<Pointer>& getConstellations();
    const std::string name;
    const std::string genitive;
    const std::string abbrev;
};

#endif  // _CONSTELLATION_H_
