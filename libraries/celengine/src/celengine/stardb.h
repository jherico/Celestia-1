// stardb.h
//
// Copyright (C) 2001-2009, the Celestia Development Team
// Original version by Chris Laurel <claurel@gmail.com>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#ifndef _CELENGINE_STARDB_H_
#define _CELENGINE_STARDB_H_

#include <iostream>
#include <vector>
#include <map>
#include "constellation.h"
#include "starname.h"
#include "star.h"
#include "staroctree.h"
#include "parser.h"

static const uint32_t MAX_STAR_NAMES = 10;

// TODO: Move BlockArray to celutil; consider making it a full STL
// style container with iterator support.

/*! BlockArray is a container class that is similar to an STL vector
 *  except for two very important differences:
 *  - The elements of a BlockArray are not necessarily in one
 *    contiguous block of memory.
 *  - The address of a BlockArray element is guaranteed not to
 *    change over the lifetime of the BlockArray (or until the
 *    BlockArray is cleared.)
 */
template <class T>
class BlockArray {
public:
    BlockArray() : m_blockSize(1000), m_elementCount(0) {}

    ~BlockArray() { clear(); }

    uint32_t size() const { return m_elementCount; }

    /*! Append an item to the BlockArray. */
    void add(T& element) {
        uint32_t blockIndex = m_elementCount / m_blockSize;
        if (blockIndex == m_blocks.size()) {
            T* newBlock = new T[m_blockSize];
            m_blocks.push_back(newBlock);
        }

        uint32_t elementIndex = m_elementCount % m_blockSize;
        m_blocks.back()[elementIndex] = element;

        ++m_elementCount;
    }

    void clear() {
        for (typename std::vector<T*>::const_iterator iter = m_blocks.begin(); iter != m_blocks.end(); ++iter) {
            delete[] * iter;
        }
        m_elementCount = 0;
        m_blocks.clear();
    }

    T& operator[](int index) {
        uint32_t blockNumber = index / m_blockSize;
        uint32_t elementNumber = index % m_blockSize;
        return m_blocks[blockNumber][elementNumber];
    }

    const T& operator[](int index) const {
        uint32_t blockNumber = index / m_blockSize;
        uint32_t elementNumber = index % m_blockSize;
        return m_blocks[blockNumber][elementNumber];
    }

private:
    uint32_t m_blockSize;
    uint32_t m_elementCount;
    std::vector<T*> m_blocks;
};

class StarDatabase {
public:
    StarDatabase();
    ~StarDatabase();

    inline const StarPtr& getStar(const uint32_t) const;
    inline uint32_t size() const;

    StarPtr find(uint32_t catalogNumber) const;
    StarPtr find(const std::string&) const;
    uint32_t findCatalogNumberByName(const std::string&) const;

    std::vector<std::string> getCompletion(const std::string&) const;

    void findVisibleStars(StarHandler& starHandler,
                          const Eigen::Vector3f& obsPosition,
                          const Eigen::Quaternionf& obsOrientation,
                          float fovY,
                          float aspectRatio,
                          float limitingMag) const;

    void findCloseStars(StarHandler& starHandler, const Eigen::Vector3f& obsPosition, float radius) const;

    std::string getStarName(const Star&, bool i18n = false) const;
    void getStarName(const Star& star, char* nameBuffer, uint32_t bufferSize, bool i18n = false) const;
    std::string getStarNameList(const Star&, const uint32_t maxNames = MAX_STAR_NAMES) const;

    const StarNameDatabase::Pointer& getNameDatabase() const;
    void setNameDatabase(const StarNameDatabase::Pointer&);

    bool load(std::istream&, const std::string& resourcePath);
    bool loadBinary(std::istream&);

    enum Catalog
    {
        HenryDraper = 0,
        Gliese = 1,
        SAO = 2,
        MaxCatalog = 3,
    };

    enum StcDisposition
    {
        AddStar,
        ReplaceStar,
        ModifyStar,
    };

    // Not exact, but any star with a catalog number greater than this is assumed to not be
    // a HIPPARCOS stars.
    static const uint32_t MAX_HIPPARCOS_NUMBER = 999999;

    struct CrossIndexEntry {
        uint32_t catalogNumber;
        uint32_t celCatalogNumber;

        bool operator<(const CrossIndexEntry&) const;
    };

    typedef std::vector<CrossIndexEntry> CrossIndex;
    using CrossIndexPtr = std::shared_ptr<CrossIndex>;

    bool loadCrossIndex(const Catalog, std::istream&);
    uint32_t searchCrossIndexForCatalogNumber(const Catalog, const uint32_t number) const;
    StarPtr searchCrossIndex(const Catalog, const uint32_t number) const;
    uint32_t crossIndex(const Catalog, const uint32_t number) const;

    void finish();

    static StarDatabase* read(std::istream&);

    static const char* FILE_HEADER;
    static const char* CROSSINDEX_FILE_HEADER;

private:
    bool createStar(const StarPtr& star,
                    StcDisposition disposition,
                    uint32_t catalogNumber,
                    const HashPtr& starData,
                    const std::string& path,
                    const bool isBarycenter);

    void buildOctree();
    void buildIndexes();
    StarPtr findWhileLoading(uint32_t catalogNumber) const;

    std::vector<StarPtr> stars;
    StarNameDatabase::Pointer namesDB;
    std::vector<StarPtr> catalogNumberIndex;
    StarOctreePtr octreeRoot;
    uint32_t nextAutoCatalogNumber;

    std::vector<CrossIndexPtr> crossIndexes;

    // These values are used by the star database loader; they are
    // not used after loading is complete.
    std::vector<StarPtr> unsortedStars;
    // List of stars loaded from binary file, sorted by catalog number
    std::vector<StarPtr> binFileCatalogNumberIndex;
    // Catalog number -> star mapping for stars loaded from stc files
    std::map<uint32_t, StarPtr> stcFileCatalogNumberIndex;

    struct BarycenterUsage {
        uint32_t catNo;
        uint32_t barycenterCatNo;
    };
    std::vector<BarycenterUsage> barycenters;
};

const StarPtr& StarDatabase::getStar(const uint32_t n) const {
    return stars.at(n);
}

uint32_t StarDatabase::size() const {
    return (uint32_t)stars.size();
}

#endif  // _CELENGINE_STARDB_H_
