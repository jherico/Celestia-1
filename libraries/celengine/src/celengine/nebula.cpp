// nebula.cpp
//
// Copyright (C) 2003, Chris Laurel <claurel@shatters.net>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#include "nebula.h"

#include <algorithm>
#include <cstdio>

#include <celutil/util.h>
#include <celutil/debug.h>
#include <celmath/mathlib.h>
#include <celastro/astro.h>

#include "render.h"

using namespace Eigen;
using namespace std;

Nebula::Nebula(){
}

const char* Nebula::getType() const {
    return "Nebula";
}

void Nebula::setType(const string& /*typeStr*/) {
}

size_t Nebula::getDescription(char* buf, size_t bufLength) const {
    return snprintf(buf, bufLength, _("Nebula"));
}

const char* Nebula::getObjTypeName() const {
    return "nebula";
}

bool Nebula::pick(const Ray3d& ray, double& distanceToPicker, double& cosAngleToBoundCenter) const {
    // The preconditional sphere-ray intersection test is enough for now:
    return DeepSkyObject::pick(ray, distanceToPicker, cosAngleToBoundCenter);
}

bool Nebula::load(const HashPtr& params, const string& resPath) {
    params->getString("Mesh", geometryFileName);
    return DeepSkyObject::load(params, resPath);
}

//void Nebula::render(const GLContext& glcontext,
//                    const Vector3f&,
//                    const Quaternionf&,
//                    float,
//                    float pixelSize)
//{
//    Geometry* g = NULL;
//    if (geometry != InvalidResource)
//        g = GetGeometryManager()->find(geometry);
//    if (g == NULL)
//        return;
//
//    glDisable(GL_BLEND);
//
//    glScalef(getRadius(), getRadius(), getRadius());
//    glRotate(getOrientation());
//
//    if (glcontext.getRenderPath() == GLContext::GLPath_GLSL)
//    {
//        GLSLUnlit_RenderContext rc(getRadius());
//        rc.setPointScale(2.0f * getRadius() / pixelSize);
//        g->render(rc);
//        glUseProgramObjectARB(0);
//    }
//    else
//    {
//        FixedFunctionRenderContext rc;
//        rc.setLighting(false);
//        g->render(rc);
//
//        // Reset the material
//        float black[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
//        float zero = 0.0f;
//        glColor4fv(black);
//        glMaterialfv(GL_FRONT, GL_EMISSION, black);
//        glMaterialfv(GL_FRONT, GL_SPECULAR, black);
//        glMaterialfv(GL_FRONT, GL_SHININESS, &zero);
//    }
//
//    glEnable(GL_BLEND);
//}

uint32_t Nebula::getRenderMask() const {
    return Renderer::ShowNebulae;
}

uint32_t Nebula::getLabelMask() const {
    return Renderer::NebulaLabels;
}
