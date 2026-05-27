#pragma once

#ifdef USE_USD

#include <string>
#include <vector>

#include <pxr/pxr.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/mesh.h>
#include <pxr/base/gf/vec3f.h>

#include "object3d.h"

PXR_NAMESPACE_USING_DIRECTIVE

struct USDMeshData {
    std::vector<Vertex3D> vertices;
    std::vector<int> indices;
};

class USDManager {
public:
    USDManager();
    ~USDManager();

    bool loadStage(const std::string& filepath);
    std::vector<USDMeshData> extractMeshes();

private:
    UsdStageRefPtr stage;
};

#endif
