#include "usd_manager.h"
#include <pxr/usd/usd/primRange.h>

PXR_NAMESPACE_USING_DIRECTIVE

USDManager::USDManager() {}

USDManager::~USDManager() {}

bool USDManager::loadStage(const std::string& filepath) {
    stage = UsdStage::Open(filepath);
    return stage != nullptr;
}

std::vector<USDMeshData> USDManager::extractMeshes() {
    std::vector<USDMeshData> meshesData;
    
    if (!stage) {
        return meshesData;
    }

    for (UsdPrim prim : stage->Traverse()) {
        if (prim.IsA<UsdGeomMesh>()) {
            UsdGeomMesh mesh(prim);
            
            VtArray<GfVec3f> points;
            mesh.GetPointsAttr().Get(&points);
            
            VtArray<GfVec3f> normals;
            if (mesh.GetNormalsAttr().Get(&normals) && normals.size() == points.size()) {
                // Have per-vertex normals
            } else {
                // Normal generation or just use dummy normals
                normals.assign(points.size(), GfVec3f(0.0f, 1.0f, 0.0f));
            }

            VtArray<int> faceVertexIndices;
            mesh.GetFaceVertexIndicesAttr().Get(&faceVertexIndices);
            
            USDMeshData data;
            data.vertices.reserve(points.size());
            for (size_t i = 0; i < points.size(); ++i) {
                Vertex3D v;
                v.pos[0] = points[i][0];
                v.pos[1] = points[i][1];
                v.pos[2] = points[i][2];
                v.normal[0] = normals[i][0];
                v.normal[1] = normals[i][1];
                v.normal[2] = normals[i][2];
                data.vertices.push_back(v);
            }
            
            data.indices.reserve(faceVertexIndices.size());
            for (size_t i = 0; i < faceVertexIndices.size(); ++i) {
                data.indices.push_back(faceVertexIndices[i]);
            }
            
            meshesData.push_back(data);
        }
    }
    
    return meshesData;
}
