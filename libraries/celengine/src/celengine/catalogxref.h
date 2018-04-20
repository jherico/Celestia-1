// catalogxref.h
//
// Copyright (C) 2001, Chris Laurel <claurel@shatters.net>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#ifndef _CATALOGXREF_H_
#define _CATALOGXREF_H_

#include <vector>
#include <string>
#include <iostream>
#include <memory>

#include "forward.h"

class CatalogCrossReference {
public:
    using Pointer = std::shared_ptr<CatalogCrossReference>;
    CatalogCrossReference();
    ~CatalogCrossReference();

    std::string getPrefix() const;
    void setPrefix(const std::string&);

    uint32_t parse(const std::string&) const;
    StarPtr lookup(uint32_t) const;
    StarPtr lookup(const std::string&) const;

    void addEntry(uint32_t catalogNumber, const StarPtr& star);
    void sortEntries();
    void reserve(size_t);

    enum
    {
        InvalidCatalogNumber = 0xffffffff,
    };

public:
    class Entry {
    public:
        Entry(uint32_t catalogNumber, const StarPtr& star = nullptr) : catalogNumber(catalogNumber), star(star) {}
        uint32_t catalogNumber;
        StarPtr star;
    };

private:
    std::string prefix;
    std::vector<Entry> entries;
};

class StarDatabase;

extern CatalogCrossReference::Pointer ReadCatalogCrossReference(std::istream&, const StarDatabase&);

#endif  // _CATALOGXREF_H_
