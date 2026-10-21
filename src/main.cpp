// Copyright (c) 2026 Dimitris Savvaris
// SPDX-License-Identifier: MIT

#include <array>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "httplib.h"

constexpr double PI = 3.14159265358979323846;
constexpr double DEG_TO_RAD = PI / 180.0;

// ------------------------------------------------------------
// Data structures
// ------------------------------------------------------------

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

// ------------------------------------------------------------
// Angle conversion
// ------------------------------------------------------------

double toRadians(double degrees)
{
    return degrees * DEG_TO_RAD;
}

// ------------------------------------------------------------
// Convert RPY to a rotation matrix.
//
// Convention:
//
//   R = Rz(yaw) * Ry(pitch) * Rx(roll)
//
// The matrix maps coordinates from the local IMU frame
// into the common world frame, under our chosen convention.
// ------------------------------------------------------------

Matrix3 rpyToMatrix(const RPY& rpy)
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

    Matrix3 R;

    R.m[0][0] = cy * cp;
    R.m[0][1] = cy * sp * sr - sy * cr;
    R.m[0][2] = cy * sp * cr + sy * sr;

    R.m[1][0] = sy * cp;
    R.m[1][1] = sy * sp * sr + cy * cr;
    R.m[1][2] = sy * sp * cr - cy * sr;

    R.m[2][0] = -sp;
    R.m[2][1] = cp * sr;
    R.m[2][2] = cp * cr;

    return R;
}

// ------------------------------------------------------------
// Matrix multiplication.
//
// C = A * B
//
// C[i][j] = sum(A[i][k] * B[k][j])
//
// Matrix multiplication is not generally commutative:
// A * B != B * A.
// ------------------------------------------------------------

Matrix3 multiply(const Matrix3& A, const Matrix3& B)
{
    Matrix3 C;

    for (int row = 0; row < 3; ++row)
    {
        for (int col = 0; col < 3; ++col)
        {
            for (int k = 0; k < 3; ++k)
            {
                C.m[row][col] +=
                    A.m[row][k] * B.m[k][col];
            }
        }
    }

    return C;
}

// ------------------------------------------------------------
// Matrix transpose.
//
// T = A^T
//
// For a rotation matrix R:
//
// R^(-1) = R^T
//
// Therefore, transposing R reverses the rotation.
// ------------------------------------------------------------

Matrix3 transpose(const Matrix3& A)
{
    Matrix3 T;

    for (int row = 0; row < 3; ++row)
    {
        for (int col = 0; col < 3; ++col)
        {
            T.m[row][col] = A.m[col][row];
        }
    }

    return T;
}

// ------------------------------------------------------------
// Calculate component-wise RPY offsets.
//
// offsetRPY = RPY_vn - RPY_external
//
// These are Euler-angle differences, not necessarily the
// Euler angles of the true relative rotation.
// ------------------------------------------------------------

RPY calculateRPYOffsets(
    const RPY& external,
    const RPY& vn)
{
    return {
        vn.roll - external.roll,
        vn.pitch - external.pitch,
        vn.yaw - external.yaw
    };
}

// ------------------------------------------------------------
// Serialize a 3x3 matrix as a JSON array of rows.
//
// Example:
//
// [
//   [1, 0, 0],
//   [0, 1, 0],
//   [0, 0, 1]
// ]
// ------------------------------------------------------------

void appendMatrixJSON(
    std::ostringstream& out,
    const Matrix3& matrix)
{
    out << '[';

    for (int row = 0; row < 3; ++row)
    {
        if (row > 0)
        {
            out << ',';
        }

        out << '[';

        for (int col = 0; col < 3; ++col)
        {
            if (col > 0)
            {
                out << ',';
            }

            out << matrix.m[row][col];
        }

        out << ']';
    }

    out << ']';
}

// ------------------------------------------------------------
// Read a finite floating-point parameter from the URL.
//
// Rejects missing, invalid, NaN and infinite values.
// ------------------------------------------------------------

bool readParameter(
    const httplib::Request& req,
    const std::string& name,
    double& value)
{
    if (!req.has_param(name))
    {
        return false;
    }

    const std::string text = req.get_param_value(name);

    char* end = nullptr;

    value = std::strtod(text.c_str(), &end);

    return end != text.c_str()
        && *end == '\0'
        && std::isfinite(value);
}

// ------------------------------------------------------------
// Read an RPY orientation from URL parameters.
//
// For example:
//
// external_roll=30
// external_pitch=10
// external_yaw=45
// ------------------------------------------------------------

bool readRPYParameters(
    const httplib::Request& req,
    const std::string& prefix,
    RPY& rpy)
{
    return
        readParameter(req, prefix + "_roll", rpy.roll)
        &&
        readParameter(req, prefix + "_pitch", rpy.pitch)
        &&
        readParameter(req, prefix + "_yaw", rpy.yaw);
}

// ------------------------------------------------------------
// Main
// ------------------------------------------------------------

int main()
{
    httplib::Server server;

    // --------------------------------------------------------
    // GET /api/offsets
    //
    // Required parameters:
    //
    // external_roll
    // external_pitch
    // external_yaw
    //
    // vn_roll
    // vn_pitch
    // vn_yaw
    //
    // Returns:
    //
    // - Component-wise RPY offsets.
    // - External rotation matrix.
    // - VN rotation matrix.
    // - Relative rotation matrix.
    // --------------------------------------------------------

    server.Get(
        "/api/offsets",
        [](const httplib::Request& req, httplib::Response& res)
        {
            RPY external;
            RPY vn;

            // Read and validate the external IMU orientation.
            if (!readRPYParameters(req, "external", external))
            {
                res.status = 400;

                res.set_content(
                    R"({"error":"Missing or invalid external RPY parameters"})",
                    "application/json");

                return;
            }

            // Read and validate the VN IMU orientation.
            if (!readRPYParameters(req, "vn", vn))
            {
                res.status = 400;

                res.set_content(
                    R"({"error":"Missing or invalid VN RPY parameters"})",
                    "application/json");

                return;
            }

            // ------------------------------------------------
            // STEP 1: Calculate component-wise RPY offsets.
            //
            // offsetRPY = RPY_vn - RPY_external
            // ------------------------------------------------

            const RPY offsets =
                calculateRPYOffsets(external, vn);

            // ------------------------------------------------
            // STEP 2: Convert the orientations into matrices.
            //
            // R_external = RPY_to_matrix(external)
            // R_vn       = RPY_to_matrix(vn)
            // ------------------------------------------------

            const Matrix3 R_external =
                rpyToMatrix(external);

            const Matrix3 R_vn =
                rpyToMatrix(vn);

            // ------------------------------------------------
            // STEP 3: Calculate the relative rotation.
            //
            // R_offset = transpose(R_external) * R_vn
            //
            // Assumption:
            // Both matrices map local IMU coordinates into
            // the same world frame.
            // ------------------------------------------------

            const Matrix3 R_offset =
                multiply(transpose(R_external), R_vn);

            // ------------------------------------------------
            // STEP 4: Return the results as JSON.
            //
            // Matrices are serialized as arrays of rows.
            // ------------------------------------------------

            std::ostringstream out;

            out << std::setprecision(15);

            out << '{';

            out << R"("external_rpy":{)"
                << R"("roll":)" << external.roll << ','
                << R"("pitch":)" << external.pitch << ','
                << R"("yaw":)" << external.yaw
                << "},";

            out << R"("vn_rpy":{)"
                << R"("roll":)" << vn.roll << ','
                << R"("pitch":)" << vn.pitch << ','
                << R"("yaw":)" << vn.yaw
                << "},";

            out << R"("rpy_offsets":{)"
                << R"("roll":)" << offsets.roll << ','
                << R"("pitch":)" << offsets.pitch << ','
                << R"("yaw":)" << offsets.yaw
                << "},";

            out << R"("R_external":)";
            appendMatrixJSON(out, R_external);

            out << ',';

            out << R"("R_vn":)";
            appendMatrixJSON(out, R_vn);

            out << ',';

            out << R"("R_offset":)";
            appendMatrixJSON(out, R_offset);

            out << '}';

            res.set_content(
                out.str(),
                "application/json");
        });

    std::cout
        << "C++ RPY server starting...\n"
        << "Hello endpoint: http://localhost:8080/api/hello\n"
        << "Rotation endpoint: http://localhost:8080/api/offsets\n";

    server.set_mount_point("/", "./www");

    if (!server.listen("127.0.0.1", 8080))
    {
        std::cerr
            << "Error: Could not start server on port 8080.\n";

        return 1;
    }

    return 0;
}