// Copyright (c) 2026 Dimitris Savvaris
// SPDX-License-Identifier: MIT

#include "ServiceHandler.h"

#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>

#include "ReferenceFrame.h"
#include "Rotation.h"
#include "httplib.h"

namespace
{
constexpr const char* DEFAULT_FRAME = "NED";

void sendError(httplib::Response& res, const std::string& message)
{
    res.status = 400;
    res.set_content(R"({"error":")" + message + R"("})", "application/json");
}

// Absent parameter -> fallback; present but unknown/left-handed -> nullopt.
std::optional<ReferenceFrame> readFrame(const httplib::Request& req, const std::string& name,
                                        const std::string& fallback)
{
    return ReferenceFrame::parse(req.has_param(name) ? req.get_param_value(name) : fallback);
}

// Rejects missing, invalid, NaN and infinite values.
bool readParameter(const httplib::Request& req, const std::string& name, double& value)
{
    if (!req.has_param(name))
    {
        return false;
    }

    const std::string text = req.get_param_value(name);
    char* end = nullptr;

    value = std::strtod(text.c_str(), &end);

    return end != text.c_str() && *end == '\0' && std::isfinite(value);
}

// Reads <prefix>_roll, <prefix>_pitch and <prefix>_yaw.
bool readRPYParameters(const httplib::Request& req, const std::string& prefix, RPY& rpy)
{
    return readParameter(req, prefix + "_roll", rpy.roll) &&
           readParameter(req, prefix + "_pitch", rpy.pitch) &&
           readParameter(req, prefix + "_yaw", rpy.yaw);
}

void appendRPYJSON(std::ostringstream& out, const RPY& rpy)
{
    out << '{' << R"("roll":)" << rpy.roll << ',' << R"("pitch":)" << rpy.pitch << ','
        << R"("yaw":)" << rpy.yaw << '}';
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
    server.Get("/api/frames", [this](const httplib::Request& req, httplib::Response& res)
               { handleFrames(req, res); });

    server.Get("/api/offsets", [this](const httplib::Request& req, httplib::Response& res)
               { handleOffsets(req, res); });
}

void ServiceHandler::handleFrames(const httplib::Request&, httplib::Response& res) const
{
    std::ostringstream out;
    out << std::setprecision(15) << R"({"default":")" << DEFAULT_FRAME << R"(","frames":[)";

    bool first = true;

    for (const ReferenceFrame& frame : ReferenceFrame::supported())
    {
        const auto labels = frame.axisLabels();

        out << (first ? "" : ",") << R"({"id":")" << frame.code() << R"(","axes":[")" << labels[0]
            << R"(",")" << labels[1] << R"(",")" << labels[2] << R"("],"to_display":)";
        appendMatrixJSON(out, frame.toDisplay().matrix());
        out << '}';
        first = false;
    }

    out << "]}";
    res.set_content(out.str(), "application/json");
}

void ServiceHandler::handleOffsets(const httplib::Request& req, httplib::Response& res) const
{
    RPY external;
    RPY internal;

    if (!readRPYParameters(req, "external", external))
    {
        sendError(res, "Missing or invalid external RPY parameters");
        return;
    }

    if (!readRPYParameters(req, "internal", internal))
    {
        sendError(res, "Missing or invalid Internal RPY parameters");
        return;
    }

    // `frame` is the output frame; each sensor's angles default to it.
    const auto frame = readFrame(req, "frame", DEFAULT_FRAME);

    if (!frame)
    {
        sendError(res, "Unknown or left-handed reference frame");
        return;
    }

    const auto externalFrame = readFrame(req, "external_frame", frame->code());
    const auto internalFrame = readFrame(req, "internal_frame", frame->code());

    if (!externalFrame || !internalFrame)
    {
        sendError(res, "Unknown or left-handed sensor reference frame");
        return;
    }

    // Re-express a sensor's orientation in the output frame.
    const auto toOutputFrame = [&](const RPY& rpy, const ReferenceFrame& from)
    { return ReferenceFrame::conversion(from, *frame) * Rotation(rpy); };

    const Rotation externalRotation = toOutputFrame(external, *externalFrame);
    const Rotation internalRotation = toOutputFrame(internal, *internalFrame);
    const Rotation offsetRotation = externalRotation.transposed() * internalRotation;

    // Untouched angles when no conversion happened, so inputs round-trip exactly.
    const RPY externalOut = *externalFrame == *frame ? external : externalRotation.toRPY();
    const RPY internalOut = *internalFrame == *frame ? internal : internalRotation.toRPY();
    const RPY offsets = Rotation::rpyOffsets(externalOut, internalOut);

    std::ostringstream out;
    out << std::setprecision(15) << '{' << R"("frame":")" << frame->code() << R"(",)"
        << R"("external_frame":")" << externalFrame->code() << R"(",)"
        << R"("internal_frame":")" << internalFrame->code() << R"(",)"
        << R"("external_rpy":)";
    appendRPYJSON(out, external);
    out << ',' << R"("internal_rpy":)";
    appendRPYJSON(out, internal);
    out << ',' << R"("rpy_offsets":)";
    appendRPYJSON(out, offsets);
    out << ',' << R"("R_external":)";
    appendMatrixJSON(out, externalRotation.matrix());
    out << ',' << R"("R_internal":)";
    appendMatrixJSON(out, internalRotation.matrix());
    out << ',' << R"("R_offset":)";
    appendMatrixJSON(out, offsetRotation.matrix());
    out << '}';

    res.set_content(out.str(), "application/json");
}
