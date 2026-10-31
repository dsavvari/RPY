// Copyright (c) 2026 Dimitris Savvaris
// SPDX-License-Identifier: MIT

#include "ReferenceFrame.h"
#include "Rotation.h"

#include <array>
#include <cmath>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace
{
constexpr double TOLERANCE = 1e-12;

void expectMatrixNear(const Matrix3& actual,
                      const std::array<std::array<double, 3>, 3>& expected)
{
    for (int row = 0; row < 3; ++row)
    {
        for (int col = 0; col < 3; ++col)
        {
            EXPECT_NEAR(actual.m[row][col], expected[row][col], TOLERANCE)
                << "matrix element [" << row << "][" << col << "]";
        }
    }
}

void expectRotationNear(const Rotation& actual, const Rotation& expected)
{
    for (int row = 0; row < 3; ++row)
    {
        for (int col = 0; col < 3; ++col)
        {
            EXPECT_NEAR(actual.matrix().m[row][col],
                        expected.matrix().m[row][col],
                        TOLERANCE)
                << "matrix element [" << row << "][" << col << "]";
        }
    }
}
} // namespace

TEST(RotationTest, ZeroAnglesProduceIdentity)
{
    const Rotation rotation(RPY{});
    expectMatrixNear(rotation.matrix(), {{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}});
}

TEST(RotationTest, PositiveNinetyDegreeYawRotatesAboutZ)
{
    const Rotation rotation(RPY{0, 0, 90});
    expectMatrixNear(rotation.matrix(), {{{0, -1, 0}, {1, 0, 0}, {0, 0, 1}}});
}

TEST(RotationTest, EulerAnglesRoundTripAwayFromGimbalLock)
{
    const RPY input{17.5, -28.0, 63.25};
    const RPY output = Rotation(input).toRPY();

    EXPECT_NEAR(output.roll, input.roll, TOLERANCE);
    EXPECT_NEAR(output.pitch, input.pitch, TOLERANCE);
    EXPECT_NEAR(output.yaw, input.yaw, TOLERANCE);
}

TEST(RotationTest, GimbalLockUsesZeroRollAndPreservesRotation)
{
    for (const double pitch : {90.0, -90.0})
    {
        const Rotation original(RPY{25.0, pitch, -40.0});
        const RPY extracted = original.toRPY();
        const Rotation reconstructed(extracted);

        EXPECT_NEAR(extracted.roll, 0.0, TOLERANCE);
        EXPECT_NEAR(std::abs(extracted.pitch), 90.0, TOLERANCE);
        expectRotationNear(reconstructed, original);
    }
}

TEST(RotationTest, TransposeIsInverse)
{
    const Rotation rotation(RPY{12.0, -34.0, 78.0});
    const Rotation identity = rotation * rotation.transposed();

    expectMatrixNear(identity.matrix(), {{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}});
}

TEST(RotationTest, RpyOffsetsAreInternalMinusExternal)
{
    const RPY offsets = Rotation::rpyOffsets(
        RPY{10.0, -20.0, 30.0}, RPY{15.0, -5.0, -10.0});

    EXPECT_DOUBLE_EQ(offsets.roll, 5.0);
    EXPECT_DOUBLE_EQ(offsets.pitch, 15.0);
    EXPECT_DOUBLE_EQ(offsets.yaw, -40.0);
}

TEST(ReferenceFrameTest, ParsesCaseInsensitivelyAndNormalizesCode)
{
    const auto frame = ReferenceFrame::parse("ned");

    ASSERT_TRUE(frame.has_value());
    EXPECT_EQ(frame->code(), "NED");
    EXPECT_EQ(frame->axisLabels(),
              (std::array<std::string, 3>{"North (N)", "East (E)", "Down (D)"}));
}

TEST(ReferenceFrameTest, RejectsMalformedDuplicateAndLeftHandedCodes)
{
    EXPECT_FALSE(ReferenceFrame::parse("NE").has_value());
    EXPECT_FALSE(ReferenceFrame::parse("NNN").has_value());
    EXPECT_FALSE(ReferenceFrame::parse("NEU").has_value());
    EXPECT_FALSE(ReferenceFrame::parse("XYZ").has_value());
}

TEST(ReferenceFrameTest, SupportedFramesAreValidAndStable)
{
    const std::vector<std::string> expected{"NED", "ENU", "NWU", "SEU", "SWD"};
    const auto& frames = ReferenceFrame::supported();

    ASSERT_EQ(frames.size(), expected.size());

    for (std::size_t i = 0; i < frames.size(); ++i)
    {
        EXPECT_EQ(frames[i].code(), expected[i]);
        EXPECT_TRUE(ReferenceFrame::parse(frames[i].code()).has_value());
    }
}

TEST(ReferenceFrameTest, NEDToENUConversionHasExpectedAxes)
{
    const auto ned = ReferenceFrame::parse("NED");
    const auto enu = ReferenceFrame::parse("ENU");
    ASSERT_TRUE(ned.has_value());
    ASSERT_TRUE(enu.has_value());

    expectMatrixNear(ReferenceFrame::conversion(*ned, *enu).matrix(),
                     {{{0, 1, 0}, {1, 0, 0}, {0, 0, -1}}});
}

TEST(ReferenceFrameTest, ConversionAndReverseConversionCancel)
{
    const auto ned = ReferenceFrame::parse("NED");
    const auto enu = ReferenceFrame::parse("ENU");
    ASSERT_TRUE(ned.has_value());
    ASSERT_TRUE(enu.has_value());

    const Rotation toEnu = ReferenceFrame::conversion(*ned, *enu);
    const Rotation toNed = ReferenceFrame::conversion(*enu, *ned);
    expectRotationNear(toNed * toEnu, Rotation(RPY{}));
}

TEST(ReferenceFrameTest, EnuDisplayBasisMapsUpToThreeJsY)
{
    const auto enu = ReferenceFrame::parse("ENU");
    ASSERT_TRUE(enu.has_value());

    expectMatrixNear(enu->toDisplay().matrix(),
                     {{{1, 0, 0}, {0, 0, 1}, {0, -1, 0}}});
}
