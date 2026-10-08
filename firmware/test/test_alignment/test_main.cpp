// Host tests for ui::align.

#include <unity.h>

#include "ui/AlignmentMath.h"

using namespace ui::align;

void setUp() {}
void tearDown() {}

void test_normalize_and_wrap() {
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 345.7f, normalize360(-14.3f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, normalize360(360.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 10.0f, normalize360(730.0f));
    TEST_ASSERT_TRUE(normalize360(-1e-7f) < 360.0f);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, -10.0f, wrap180(350.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 180.0f, wrap180(-180.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 170.0f, wrap180(-190.0f));
}

void test_cardinals() {
    TEST_ASSERT_EQUAL_STRING("N", cardinal16(0));
    TEST_ASSERT_EQUAL_STRING("N", cardinal16(-11.0f));
    TEST_ASSERT_EQUAL_STRING("NNW", cardinal16(-14.3f));
    TEST_ASSERT_EQUAL_STRING("NNE", cardinal16(22.5f));
    TEST_ASSERT_EQUAL_STRING("E", cardinal16(90.0f));
    TEST_ASSERT_EQUAL_STRING("S", cardinal16(180.0f));
    TEST_ASSERT_EQUAL_STRING("W", cardinal16(-90.0f));
    TEST_ASSERT_EQUAL_STRING("N", cardinal16(359.0f));
}

void test_offset_from_real_dish() {
    // Values from the real dish: pointing -14.3 / 72.2, desired -0.04 / 76.0.
    const Offset o = offset(-14.3f, 72.2f, -0.04f, 76.0f);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 14.26f, *o.azimuth);  // turn clockwise
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 3.8f, *o.elevation);  // raise
}

void test_offset_wraps_through_north() {
    const Offset o = offset(350.0f, 70.0f, 10.0f, 70.0f);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 20.0f, *o.azimuth);
    const Offset back = offset(10.0f, 70.0f, 350.0f, 70.0f);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, -20.0f, *back.azimuth);
}

void test_offset_needs_both_values() {
    const Offset o = offset(std::nullopt, 72.0f, 0.0f, std::nullopt);
    TEST_ASSERT_FALSE(o.azimuth.has_value());
    TEST_ASSERT_FALSE(o.elevation.has_value());
}

void test_projection() {
    PlotPoint p = project(0, 90, 30);  // straight up: centre
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0, p.x);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0, p.y);
    p = project(0, 60, 30);  // north, at the edge: top
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0, p.x);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, -1, p.y);
    TEST_ASSERT_FALSE(p.clamped);
    p = project(90, 75, 30);  // east, halfway: right
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.5f, p.x);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0, p.y);
    p = project(180, 20, 30);  // south, far beyond the edge: clamped bottom
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1, p.y);
    TEST_ASSERT_TRUE(p.clamped);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_normalize_and_wrap);
    RUN_TEST(test_cardinals);
    RUN_TEST(test_offset_from_real_dish);
    RUN_TEST(test_offset_wraps_through_north);
    RUN_TEST(test_offset_needs_both_values);
    RUN_TEST(test_projection);
    return UNITY_END();
}
