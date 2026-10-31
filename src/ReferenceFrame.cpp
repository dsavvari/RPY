// Copyright (c) 2026 Dimitris Savvaris
// SPDX-License-Identifier: MIT

#include "ReferenceFrame.h"

#include <cctype>

namespace
{
struct Direction
{
    char letter;
    const char* label;
    std::array<int, 3> enu; // components along East, North, Up
    int pair;               // 0 = E/W, 1 = N/S, 2 = U/D
};

constexpr Direction DIRECTIONS[] = {
    { 'E', "East (E)",  {  1,  0,  0 }, 0 },
    { 'W', "West (W)",  { -1,  0,  0 }, 0 },
    { 'N', "North (N)", {  0,  1,  0 }, 1 },
    { 'S', "South (S)", {  0, -1,  0 }, 1 },
    { 'U', "Up (U)",    {  0,  0,  1 }, 2 },
    { 'D', "Down (D)",  {  0,  0, -1 }, 2 },
};

const Direction* findDirection(char letter)
{
    const char upper = static_cast<char>(
        std::toupper(static_cast<unsigned char>(letter)));

    for (const Direction& d : DIRECTIONS)
    {
        if (d.letter == upper)
        {
            return &d;
        }
    }

    return nullptr;
}
} // namespace

std::optional<ReferenceFrame> ReferenceFrame::parse(const std::string& code)
{
    if (code.size() != 3)
    {
        return std::nullopt;
    }

    Axes axes{};
    std::string normalized;
    int usedPairs = 0;

    for (int i = 0; i < 3; ++i)
    {
        const Direction* d = findDirection(code[i]);

        if (d == nullptr || (usedPairs & (1 << d->pair)))
        {
            return std::nullopt;
        }

        usedPairs |= 1 << d->pair;
        axes[i] = d->enu;
        normalized += d->letter;
    }

    // Right-handed only: (axis0 x axis1) . axis2 must be +1.
    const auto& a = axes[0];
    const auto& b = axes[1];
    const auto& c = axes[2];
    const int triple =
        (a[1] * b[2] - a[2] * b[1]) * c[0]
        + (a[2] * b[0] - a[0] * b[2]) * c[1]
        + (a[0] * b[1] - a[1] * b[0]) * c[2];

    if (triple != 1)
    {
        return std::nullopt;
    }

    return ReferenceFrame(normalized, axes);
}

const std::vector<ReferenceFrame>& ReferenceFrame::supported()
{
    static const std::vector<ReferenceFrame> frames = []
    {
        std::vector<ReferenceFrame> list;

        for (const char* code : { "NED", "ENU", "NWU", "SEU", "SWD" })
        {
            list.push_back(*parse(code));
        }

        return list;
    }();

    return frames;
}

Rotation ReferenceFrame::conversion(const ReferenceFrame& from,
                                    const ReferenceFrame& to)
{
    return to.toENU().transposed() * from.toENU();
}

std::array<std::string, 3> ReferenceFrame::axisLabels() const
{
    std::array<std::string, 3> labels;

    for (int i = 0; i < 3; ++i)
    {
        labels[i] = findDirection(code_[i])->label;
    }

    return labels;
}

Rotation ReferenceFrame::toDisplay() const
{
    // ENU (East, North, Up) -> Three.js (X right, Y up, Z toward viewer).
    Matrix3 enuToDisplay;
    enuToDisplay.m[0][0] = 1;
    enuToDisplay.m[1][2] = 1;
    enuToDisplay.m[2][1] = -1;

    return Rotation(enuToDisplay) * toENU();
}

Rotation ReferenceFrame::toENU() const
{
    Matrix3 m;

    for (int col = 0; col < 3; ++col)
    {
        for (int row = 0; row < 3; ++row)
        {
            m.m[row][col] = axes_[col][row];
        }
    }

    return Rotation(m);
}
