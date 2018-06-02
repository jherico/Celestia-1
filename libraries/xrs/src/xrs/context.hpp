#pragma once

#include <algorithm>
#include <functional>
#include <list>
#include <set>
#include <string>
#include <vector>
#include <queue>

#include <glm/glm.hpp>

#define XR_DEFINE_HANDLE(object) typedef struct object##_T* object;

XR_DEFINE_HANDLE(XrInstance);
XR_DEFINE_HANDLE(XrSystem);

typedef uint32_t XrFlags;
typedef uint32_t XrBool32;

typedef enum XrViewportProjectionFlags {
    XR_VIEWPORT_PROJECTION_EYES_TRACKED_BIT = 0x00000001,
} XrViewportProjectionFlags;

enum XrObjectType {
    XR_OBJECT_TYPE_UNKNOWN = 0,
    XR_OBJECT_TYPE_INSTANCE,
    XR_OBJECT_TYPE_SYSTEM,
    XR_OBJECT_TYPE_SESSION,
    XR_OBJECT_TYPE_SPACE,
    XR_OBJECT_TYPE_DEVICE,
    XR_OBJECT_TYPE_INPUT,
    XR_OBJECT_TYPE_VIEWPORT,
    XR_OBJECT_TYPE_EVENT,
};


namespace xr {
}

namespace xrs {

using StringList = std::list<std::string>;
using CStringVector = std::vector<const char*>;

struct Context {
private:
    static CStringVector toCStrings(const StringList& values) {
        CStringVector result;
        result.reserve(values.size());
        for (const auto& string : values) {
            result.push_back(string.c_str());
        }
        return result;
    }

public:
    void createInstance() {
    }

    void destroy() {
    }

    void recycle() {
    }
};

}  // namespace xrs
