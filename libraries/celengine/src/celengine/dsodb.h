//
// C++ Interface: dsodb
//
// Description:
//
//
// Author: Toti <root@totibox>, (C) 2005
//
// Copyright: See COPYING file that comes with this distribution
//
//

#ifndef _DSODB_H_
#define _DSODB_H_

#include <iostream>
#include <vector>
#include "dsoname.h"
#include "deepskyobj.h"
#include "dsooctree.h"
#include "parser.h"

static const uint32_t MAX_DSO_NAMES = 10;

extern const float DSO_OCTREE_ROOT_SIZE;

//NOTE: this one and starDatabase should be derived from a common base class since they share lots of code and functionality.
class DSODatabase {
public:
    using Pointer = std::shared_ptr<DSODatabase>;
    DSODatabase();
    ~DSODatabase();

    inline DeepSkyObject::Pointer getDSO(size_t n) const { return DSOs[n]; }
    inline size_t size() const { return DSOs.size(); }

    DeepSkyObject::Pointer find(const uint32_t catalogNumber) const;
    DeepSkyObject::Pointer find(const std::string&) const;

    std::vector<std::string> getCompletion(const std::string&) const;

    void findVisibleDSOs(DSOHandler& dsoHandler,
                         const Eigen::Vector3d& obsPosition,
                         const Eigen::Quaternionf& obsOrientation,
                         float fovY,
                         float aspectRatio,
                         float limitingMag) const;

    void findCloseDSOs(DSOHandler& dsoHandler, const Eigen::Vector3d& obsPosition, float radius) const;

    std::string getDSOName(const DeepSkyObject::Pointer&, bool i18n = false) const;
    std::string getDSONameList(const DeepSkyObject::Pointer&, const uint32_t maxNames = MAX_DSO_NAMES) const;

    const DSONameDatabase::Pointer& getNameDatabase() const { return namesDB; }
    void setNameDatabase(const DSONameDatabase::Pointer& _namesDB) { namesDB = _namesDB; }

    bool load(std::istream&, const std::string& resourcePath);
    bool loadBinary(std::istream&);
    void finish();

    static DSODatabase* read(std::istream&);

    static const char* FILE_HEADER;

    double getAverageAbsoluteMagnitude() const;

private:
    void buildIndexes();
    void buildOctree();
    void calcAvgAbsMag();

    int capacity;
    std::vector<DeepSkyObject::Pointer> DSOs;
    DSONameDatabase::Pointer namesDB;
    std::vector<DeepSkyObject::Pointer> catalogNumberIndex;
    DSOOctree::Pointer octreeRoot;
    uint32_t nextAutoCatalogNumber;

    double avgAbsMag;
};

#endif  // _DSODB_H_
