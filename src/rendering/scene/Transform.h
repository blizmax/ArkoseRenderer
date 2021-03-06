#pragma once

#include <moos/matrix.h>
#include <moos/vector.h>

class Transform {
public:
    explicit Transform(mat4 localMatrix = mat4(1.0f), const Transform* parent = nullptr)
        : m_parent(parent)
        , m_localMatrix(localMatrix)
    {
    }

    void setLocalMatrix(mat4 matrix)
    {
        m_localMatrix = matrix;
    }

    mat4 localMatrix() const
    {
        return m_localMatrix;
    }

    mat4 worldMatrix() const
    {
        if (!m_parent) {
            return m_localMatrix;
        }
        return m_parent->worldMatrix() * m_localMatrix;
    }

    mat3 worldNormalMatrix() const
    {
        mat3 world3x3 = mat3(worldMatrix());
        mat3 normalMatrix = transpose(inverse(world3x3));
        return normalMatrix;
    }

    mat3 localNormalMatrix() const
    {
        mat3 local3x3 = mat3(localMatrix());
        mat3 normalMatrix = transpose(inverse(local3x3));
        return normalMatrix;
    }

    // ..
    //Transform& setScale(float);
    //Transform& rotateBy(float);
private:
    //vec3 m_translation { 0.0 };
    //quat m_orientation {};
    //vec3 m_scale { 1.0 };
    const Transform* m_parent {};
    mutable mat4 m_localMatrix { 1.0f };
};
