// Copyright (c) 2026 Dimitris Savvaris
// SPDX-License-Identifier: MIT

#include "Rotation.h"

#include <cmath>

namespace
{
constexpr double PI = 3.14159265358979323846;

double toRadians(double degrees)
{
    return degrees * PI / 180.0;
}
} // namespace

Rotation::Rotation(const RPY& rpy)
{
    const double roll  = toRadians(rpy.roll);
    const double pitch = toRadians(rpy.pitch);
    const double yaw   = toRadians(rpy.yaw);

    const double cr = std::cos(roll);
    const double sr = std::sin(roll);
    const double cp = std::cos(pitch);
    const double sp = std::sin(pitch);
    const double cy = std::cos(yaw);
    const double sy = std::sin(yaw);

    auto& m = matrix_.m;

    m[0][0] = cy * cp;
    m[0][1] = cy * sp * sr - sy * cr;
    m[0][2] = cy * sp * cr + sy * sr;

    m[1][0] = sy * cp;
    m[1][1] = sy * sp * sr + cy * cr;
    m[1][2] = sy * sp * cr - cy * sr;

    m[2][0] = -sp;
    m[2][1] = cp * sr;
    m[2][2] = cp * cr;
}

Rotation Rotation::transposed() const
{
    Rotation result;

    for (int row = 0; row < 3; ++row)
    {
        for (int col = 0; col < 3; ++col)
        {
            result.matrix_.m[row][col] = matrix_.m[col][row];
        }
    }

    return result;
}

Rotation Rotation::operator*(const Rotation& other) const
{
    Rotation result;

    for (int row = 0; row < 3; ++row)
    {
        for (int col = 0; col < 3; ++col)
        {
            for (int k = 0; k < 3; ++k)
            {
                result.matrix_.m[row][col] +=
                    matrix_.m[row][k] * other.matrix_.m[k][col];
            }
        }
    }

    return result;
}

RPY Rotation::rpyOffsets(const RPY& external, const RPY& vn)
{
    return {
        vn.roll - external.roll,
        vn.pitch - external.pitch,
        vn.yaw - external.yaw
    };
}
