//
// C++ Interface: name
//
// Description:
//
//
// Author: Toti <root@totibox>, (C) 2005
//
// Copyright: See COPYING file that comes with this distribution
//
//

#ifndef _NAME_H_
#define _NAME_H_

#include <string>
#include <iostream>
#include <map>
#include <unordered_map>
#include <vector>
#include <list>
#include <celutil/debug.h>
#include <celutil/util.h>
#include <celutil/utf8.h>

// TODO: this can be "detemplatized" by creating e.g. a global-scope enum InvalidCatalogNumber since there
// lies the one and only need for type genericity.
template <class OBJ>
class NameDatabase {
public:
    using NameVec = std::vector<std::string>;
    using NewNameIndex = std::unordered_map<std::string, uint32_t>;
    using NewNumberIndex = std::unordered_map<uint32_t, NameVec>;
    using CItr = NewNumberIndex::const_iterator;

    //typedef std::map<std::string, uint32_t, CompareIgnoringCasePredicate> NameIndex;
    //typedef std::multimap<uint32_t, std::string> NumberIndex;

public:
    NameDatabase(){};

    uint32_t getNameCount() const { return nameIndex.size(); }

    void add(const uint32_t, const std::string&);

    // delete all names associated with the specified catalog number
    void erase(const uint32_t catalogNumber) { numberIndex.erase(catalogNumber); }

    uint32_t getCatalogNumberByName(const std::string&) const;
    std::string getNameByCatalogNumber(const uint32_t) const;

    const NameVec& getNamesByCatalogNumber(const uint32_t catalogNumber) const {
        static NameVec empty;
        auto iter = numberIndex.find(catalogNumber);
        return iter == numberIndex.end() ? empty : iter->second;
    }

    NameVec getCompletion(const std::string& name) const;

private:
    NewNameIndex nameIndex;
    NewNumberIndex numberIndex;
};

template <class OBJ>
void NameDatabase<OBJ>::add(const uint32_t catalogNumber, const std::string& name) {
    if (name.length() != 0) {
#ifdef DEBUG
        uint32_t tmp;
        if ((tmp = getCatalogNumberByName(name)) != OBJ::InvalidCatalogNumber)
            DPRINTF(2, "Duplicated name '%s' on object with catalog numbers: %d and %d\n", name.c_str(), tmp, catalogNumber);
#endif
        // Add the new name
        //nameIndex.insert(NameIndex::value_type(name, catalogNumber));

        
        nameIndex[toUpperStr(name)] = catalogNumber;
        numberIndex[catalogNumber].emplace_back(name);
    }
}

template <class OBJ>
uint32_t NameDatabase<OBJ>::getCatalogNumberByName(const std::string& name) const {
    auto iter = nameIndex.find(toUpperStr(name));
    if (iter == nameIndex.end())
        return OBJ::InvalidCatalogNumber;
    else
        return iter->second;
}

// Return the first name matching the catalog number or end()
// if there are no matching names.  The first name *should* be the
// proper name of the OBJ, if one exists. This requires the
// OBJ name database file to have the proper names listed before
// other designations.  Also, the STL implementation must
// preserve this order when inserting the names into the multimap
// (not certain whether or not this behavior is in the STL spec.
// but it works on the implementations I've tried so far.)
template <class OBJ>
std::string NameDatabase<OBJ>::getNameByCatalogNumber(const uint32_t catalogNumber) const {
    if (catalogNumber == OBJ::InvalidCatalogNumber)
        return "";

    auto iter = numberIndex.find(catalogNumber);
    if (iter != numberIndex.end() && iter->first == catalogNumber)
        return iter->second;
}

// Return the first name matching the catalog number or end()
// if there are no matching names.  The first name *should* be the
// proper name of the OBJ, if one exists. This requires the
// OBJ name database file to have the proper names listed before
// other designations.  Also, the STL implementation must
// preserve this order when inserting the names into the multimap
// (not certain whether or not this behavior is in the STL spec.
// but it works on the implementations I've tried so far.)

template <class OBJ>
std::vector<std::string> NameDatabase<OBJ>::getCompletion(const std::string& name) const {
    std::vector<std::string> completion;
    auto name_length = UTF8Length(name);

    for (auto iter = nameIndex.begin(); iter != nameIndex.end(); ++iter) {
        if (!UTF8StringCompare(iter->first, name, name_length)) {
            completion.push_back(iter->first);
        }
    }
    return completion;
}

#endif  // _NAME_H_
