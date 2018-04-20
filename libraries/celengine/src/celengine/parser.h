// parser.h
//
// Copyright (C) 2001-2009, the Celestia Development Team
// Original version by Chris Laurel <claurel@gmail.com>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#ifndef _PARSER_H_
#define _PARSER_H_

#include <vector>
#include <map>
#include <celmath/vecmath.h>
#include <celmath/quaternion.h>
#include <celutil/color.h>
#include <Eigen/Core>
#include <Eigen/Geometry>
#include "tokenizer.h"

class Value;
using ValuePtr = std::shared_ptr<Value>;

typedef std::map<std::string, ValuePtr>::const_iterator HashIterator;

class AssociativeArray {
public:
    using Pointer = std::shared_ptr<AssociativeArray>;

    ValuePtr getValue(const std::string&) const;
    void addValue(const std::string&, const ValuePtr&);

    bool getNumber(const std::string&, double&) const;
    bool getNumber(const std::string&, float&) const;
    bool getNumber(const std::string&, int&) const;
    bool getNumber(const std::string&, uint32_t&) const;
    bool getString(const std::string&, std::string&) const;
    bool getBoolean(const std::string&, bool&) const;
    bool getVector(const std::string&, Eigen::Vector3d&) const;
    bool getVector(const std::string&, Eigen::Vector3f&) const;
    bool getVector(const std::string&, Vec3d&) const;
    bool getVector(const std::string&, Vec3f&) const;
    bool getRotation(const std::string&, Quatf&) const;
    bool getRotation(const std::string&, Eigen::Quaternionf&) const;
    bool getColor(const std::string&, Color&) const;
    bool getAngle(const std::string&, double&, double = 1.0, double = 0.0) const;
    bool getAngle(const std::string&, float&, double = 1.0, double = 0.0) const;
    bool getLength(const std::string&, double&, double = 1.0, double = 0.0) const;
    bool getLength(const std::string&, float&, double = 1.0, double = 0.0) const;
    bool getTime(const std::string&, double&, double = 1.0, double = 0.0) const;
    bool getTime(const std::string&, float&, double = 1.0, double = 0.0) const;
    bool getLengthVector(const std::string&, Eigen::Vector3d&, double = 1.0, double = 0.0) const;
    bool getLengthVector(const std::string&, Eigen::Vector3f&, double = 1.0, double = 0.0) const;
    bool getSphericalTuple(const std::string&, Eigen::Vector3d&) const;
    bool getSphericalTuple(const std::string&, Eigen::Vector3f&) const;
    bool getAngleScale(const std::string&, double&) const;
    bool getAngleScale(const std::string&, float&) const;
    bool getLengthScale(const std::string&, double&) const;
    bool getLengthScale(const std::string&, float&) const;
    bool getTimeScale(const std::string&, double&) const;
    bool getTimeScale(const std::string&, float&) const;

    HashIterator begin();
    HashIterator end();

private:
    std::map<std::string, ValuePtr> assoc;
};

using Array = std::vector<ValuePtr>;
using ValueArray = std::vector<ValuePtr>;
using ValueArrayPtr = std::shared_ptr<ValueArray>;
using StringPtr = std::shared_ptr<std::string>;
using Hash = AssociativeArray;
using HashPtr = std::shared_ptr<Hash>;

class Value {
public:
    enum ValueType
    {
        NumberType = 0,
        StringType = 1,
        ArrayType = 2,
        HashType = 3,
        BooleanType = 4
    };

    Value(double);
    Value(const std::string&);
    Value(const ValueArrayPtr&);
    Value(const HashPtr&);
    Value(bool);
    ~Value();

    ValueType getType() const;

    const double& getNumber() const;
    const std::string& getString() const;
    const ValueArrayPtr& getArray() const;
    const HashPtr& getHash() const;
    bool getBoolean() const;

private:
    const ValueType type;

    const struct Data {
        Data(double d) : d(d) {}
        Data(const std::string& s) : s(std::make_shared<std::string>(s)), d(0) {}
        Data(const ValueArrayPtr& a) : a(a), d(0) {}
        Data(const HashPtr& h) : h(h), d(0) {}
        const StringPtr s;
        const double d;
        const ValueArrayPtr a;
        const HashPtr h;
    } data;
};

class Parser {
public:
    Parser(Tokenizer*);

    ValuePtr readValue();

private:
    Tokenizer* tokenizer;

    bool readUnits(const std::string&, const HashPtr&);
    ValueArrayPtr readArray();
    HashPtr readHash();
};

#endif  // _PARSER_H_
