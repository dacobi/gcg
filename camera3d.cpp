#include "camera3d.h"
#include <cmath>
#include <pxr/base/gf/rotation.h>

Camera3D::Camera3D() 
    : position(0, 500, 1000), 
      target(0, 0, 0), 
      upVector(0, 1, 0),
      fov(45.0f), 
      nearClip(0.1f), 
      farClip(10000.0f) {
}

void Camera3D::setPosition(const pxr::GfVec3f& pos) {
    position = pos;
}

void Camera3D::setTarget(const pxr::GfVec3f& tgt) {
    target = tgt;
}

void Camera3D::setUpVector(const pxr::GfVec3f& up) {
    upVector = up;
}

void Camera3D::setPerspective(float fovDegrees, float nearZ, float farZ) {
    fov = fovDegrees;
    nearClip = nearZ;
    farClip = farZ;
}

pxr::GfMatrix4f Camera3D::getViewMatrix() const {
    pxr::GfMatrix4f view;
    // OpenUSD provides LookAt which returns a matrix that transforms FROM view TO world.
    // We want world TO view. So we take the inverse.
    view.SetLookAt(position, target, upVector);
    return view.GetInverse();
}

pxr::GfMatrix4f Camera3D::getProjectionMatrix(float aspect) const {
    pxr::GfMatrix4f proj(0.0f);
    float f = 1.0f / std::tan((fov * M_PI / 180.0f) / 2.0f);
    
    proj[0][0] = f / aspect;
    proj[1][1] = f;
    proj[2][2] = farClip / (nearClip - farClip);
    proj[2][3] = -1.0f;
    proj[3][2] = (nearClip * farClip) / (nearClip - farClip);
    
    return proj;
}