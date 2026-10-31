// Copyright (c) 2026 Dimitris Savvaris
// SPDX-License-Identifier: MIT

#pragma once

struct RPY
{
    double roll{};
    double pitch{};
    double yaw{};
};

struct Matrix3
{
    double m[3][3]{};
};

class Rotation
{
public:
    Rotation() = default;

    // Builds R = Rz(yaw) * Ry(pitch) * Rx(roll) from angles in degrees.
    explicit Rotation(const RPY& rpy);

    // The matrix must already be a proper rotation (orthonormal, det +1).
    explicit Rotation(const Matrix3& matrix) : matrix_(matrix)
    {
    }

    const Matrix3& matrix() const
    {
        return matrix_;
    }

    // Inverse of the RPY constructor, in degrees. Roll is 0 at gimbal lock.
    RPY toRPY() const;

    // For a rotation matrix, the transpose is the inverse.
    Rotation transposed() const;

    // Not commutative: A * B != B * A.
    Rotation operator*(const Rotation& other) const;

    // Component-wise Euler-angle difference (internal - external), in degrees.
    static RPY rpyOffsets(const RPY& external, const RPY& internal);

private:
    Matrix3 matrix_;
};
