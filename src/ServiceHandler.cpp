// Copyright (c) 2026 Dimitris Savvaris
// SPDX-License-Identifier: MIT

#include "ServiceHandler.h"

#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <string>

#include "Rotation.h"
#include "httplib.h"

namespace
{
// Rejects missing, invalid, NaN and infinite values.
bool readParameter(const httplib::Request& req,
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

// Reads <prefix>_roll, <prefix>_pitch and <prefix>_yaw.
bool readRPYParameters(const httplib::Request& req,
                       const std::string& prefix,
                       RPY& rpy)
{
    return readParameter(req, prefix + "_roll", rpy.roll)
        && readParameter(req, prefix + "_pitch", rpy.pitch)
        && readParameter(req, prefix + "_yaw", rpy.yaw);
}

void appendRPYJSON(std::ostringstream& out, const RPY& rpy)
{
    out << '{'
        << R"("roll":)" << rpy.roll << ','
        << R"("pitch":)" << rpy.pitch << ','
        << R"("yaw":)" << rpy.yaw
        << '}';
}

// Serializes as an array of rows: [[a,b,c],[d,e,f],[g,h,i]].
void appendMatrixJSON(std::ostringstream& out, const Matrix3& matrix)
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
} // namespace

void ServiceHandler::registerRoutes(httplib::Server& server) const
{
    server.Get("/api/offsets",
               [this](const httplib::Request& req, httplib::Response& res)
               {
                   handleOffsets(req, res);
               });
}

void ServiceHandler::handleOffsets(const httplib::Request& req,
                                   httplib::Response& res) const
{
    RPY external;
    RPY vn;

    if (!readRPYParameters(req, "external", external))
    {
        res.status = 400;
        res.set_content(
            R"({"error":"Missing or invalid external RPY parameters"})",
            "application/json");
        return;
    }

    if (!readRPYParameters(req, "vn", vn))
    {
        res.status = 400;
        res.set_content(
            R"({"error":"Missing or invalid VN RPY parameters"})",
            "application/json");
        return;
    }

    const RPY offsets = Rotation::rpyOffsets(external, vn);
    const Rotation externalRotation(external);
    const Rotation vnRotation(vn);
    const Rotation offsetRotation = externalRotation.transposed() * vnRotation;

    std::ostringstream out;
    out << std::setprecision(15) << '{' << R"("external_rpy":)";
    appendRPYJSON(out, external);
    out << ',' << R"("vn_rpy":)";
    appendRPYJSON(out, vn);
    out << ',' << R"("rpy_offsets":)";
    appendRPYJSON(out, offsets);
    out << ',' << R"("R_external":)";
    appendMatrixJSON(out, externalRotation.matrix());
    out << ',' << R"("R_vn":)";
    appendMatrixJSON(out, vnRotation.matrix());
    out << ',' << R"("R_offset":)";
    appendMatrixJSON(out, offsetRotation.matrix());
    out << '}';

    res.set_content(out.str(), "application/json");
}
