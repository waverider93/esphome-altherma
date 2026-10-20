// Minimal stand-in for the Unity test framework, enough to run ESPAltherma's host tests
// (test_detect, test_layoutfix, test_survey; MIT License, Copyright (c) 2020 Raomin) without PlatformIO.
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>

static int unity_failures = 0;
static const char *unity_test = "";

#define UNITY_FAIL_(msg)                                                         \
  do {                                                                           \
    printf("FAIL %s (%s:%d): %s\n", unity_test, __FILE__, __LINE__, msg);       \
    unity_failures++;                                                            \
    return;                                                                      \
  } while (0)
#define TEST_ASSERT_TRUE(c) \
  if (!(c))                 \
  UNITY_FAIL_(#c)
#define TEST_ASSERT_FALSE(c) TEST_ASSERT_TRUE(!(c))
#define TEST_ASSERT_EQUAL(e, a) TEST_ASSERT_TRUE((e) == (a))
#define TEST_ASSERT_EQUAL_INT(e, a) TEST_ASSERT_TRUE((long long) (e) == (long long) (a))
#define TEST_ASSERT_EQUAL_CHAR TEST_ASSERT_EQUAL_INT
#define TEST_ASSERT_EQUAL_UINT8 TEST_ASSERT_EQUAL_INT
#define TEST_ASSERT_EQUAL_UINT32 TEST_ASSERT_EQUAL_INT
#define TEST_ASSERT_EQUAL_HEX32 TEST_ASSERT_EQUAL_INT
#define TEST_ASSERT_EQUAL_UINT64(e, a) TEST_ASSERT_TRUE((uint64_t) (e) == (uint64_t) (a))
#define TEST_ASSERT_EQUAL_size_t TEST_ASSERT_EQUAL_INT
#define TEST_ASSERT_NOT_EQUAL(e, a) TEST_ASSERT_TRUE((e) != (a))
#define TEST_ASSERT_NULL(p) TEST_ASSERT_TRUE((p) == nullptr)
#define TEST_ASSERT_NOT_NULL(p) TEST_ASSERT_TRUE((p) != nullptr)
#define TEST_ASSERT_EQUAL_STRING(e, a) TEST_ASSERT_TRUE(strcmp((e), (a)) == 0)
#define TEST_ASSERT_EQUAL_STRING_LEN(e, a, n) TEST_ASSERT_TRUE(strncmp((e), (a), (n)) == 0)

#define UNITY_BEGIN() (unity_failures = 0)
#define RUN_TEST(fn)        \
  do {                      \
    unity_test = #fn;       \
    setUp();                \
    fn();                   \
    tearDown();             \
  } while (0)
#define UNITY_END() (printf("%s\n", unity_failures ? "FAILED" : "OK"), unity_failures)
