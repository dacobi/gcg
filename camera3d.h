#pragma once

#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/matrix4f.h>

class Camera3D {
public:
    Camera3D();
    
    void setPosition(const pxr::GfVec3f& pos);
    void setTarget(const pxr::GfVec3f& target);
    void setUpVector(const pxr::GfVec3f& up);
    void setPerspective(float fovDegrees, float nearZ, float farZ);

    pxr::GfVec3f getPosition() const { return position; }

    // Returns a 16-float array for the view matrix
    pxr::GfMatrix4f getViewMatrix() const;
    
    // Returns a 16-float array for the projection matrix
    pxr::GfMatrix4f getProjectionMatrix(float aspect) const;

private:
    pxr::GfVec3f position;
    pxr::GfVec3f target;
    pxr::GfVec3f upVector;
    
    float fov;
    float nearClip;
    float farClip;
};