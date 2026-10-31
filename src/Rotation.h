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

    const Matrix3& matrix() const { return matrix_; }

    // For a rotation matrix, the transpose is the inverse.
    Rotation transposed() const;

    // Not commutative: A * B != B * A.
    Rotation operator*(const Rotation& other) const;

    // Component-wise Euler-angle difference (vn - external), in degrees.
    static RPY rpyOffsets(const RPY& external, const RPY& vn);

private:
    Matrix3 matrix_;
};
