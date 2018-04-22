// particlesystem.h
//
// Particle system file loader.
//
// Copyright (C) 2008, Chris Laurel <claurel@gmail.com>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#ifndef _CELENGINE_PARTICLESYSTEMFILE_H_
#define _CELENGINE_PARTICLESYSTEMFILE_H_

#include <iostream>
#include <string>
#include "tokenizer.h"
#include "parser.h"

class ParticleSystem;
using ParticleSystemPtr = std::shared_ptr<ParticleSystem>;
class VectorGenerator;
using VectorGeneratorPtr = std::shared_ptr<VectorGenerator>;
class ParticleEmitter;
using ParticleEmitterPtr = std::shared_ptr<ParticleEmitter>;

class ParticleSystemLoader {
public:
    using Pointer = std::shared_ptr<ParticleSystemLoader>;

    ParticleSystemLoader(std::istream&);
    ~ParticleSystemLoader();

    ParticleSystemPtr load();
    VectorGeneratorPtr parseGenerator(const HashPtr& params);
    ParticleEmitterPtr parseEmitter(const HashPtr& params);

    const std::string& getErrorMessage() const;
    void setTexturePath(const std::string&);
    const std::string& getTexturePath() const;

    static ParticleSystemLoader::Pointer OpenParticleSystemFile(std::istream& in);

private:
    virtual void raiseError(const std::string&);

private:
    Tokenizer m_tokenizer;
    Parser m_parser;
    std::string m_errorMessage;
    std::string m_texPath;
};

ParticleSystemPtr LoadParticleSystem(std::istream& in, const std::string& texPath);

#endif  // _CELENGINE_PARTICLESYSTEMFILE_H_
