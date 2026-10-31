// Copyright (c) 2026 Dimitris Savvaris
// SPDX-License-Identifier: MIT

#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "Rotation.h"

// A right-handed world frame named by its three axis directions, e.g. NED.
class ReferenceFrame
{
public:
    // Case-insensitive. One axis from each of N/S, E/W, U/D, in any order.
    // Returns nullopt for malformed or left-handed codes.
    static std::optional<ReferenceFrame> parse(const std::string& code);

    // Frames offered to clients.
    static const std::vector<ReferenceFrame>& supported();

    // Rotation taking coordinates expressed in `from` to coordinates in `to`.
    static Rotation conversion(const ReferenceFrame& from, const ReferenceFrame& to);

    const std::string& code() const
    {
        return code_;
    }

    std::array<std::string, 3> axisLabels() const;

    // Maps this frame's coordinates into the Three.js display basis (Y up).
    Rotation toDisplay() const;

    bool operator==(const ReferenceFrame& other) const
    {
        return code_ == other.code_;
    }

private:
    using Axes = std::array<std::array<int, 3>, 3>;

    ReferenceFrame(std::string code, const Axes& axes) : code_(std::move(code)), axes_(axes)
    {
    }

    Rotation toENU() const;

    std::string code_;
    // axes_[i] is this frame's i-th axis expressed in ENU coordinates.
    Axes axes_;
};
