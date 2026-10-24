#ifndef __HARNESS_H__
#define __HARNESS_H__

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Protocol/Gop3D.h>

// ============================================================================
// Test Verdict & Status Definitions
// ============================================================================

typedef enum {
    TEST_VERDICT_PASS,
    TEST_VERDICT_FAIL,
    TEST_VERDICT_SKIPPED,
    TEST_VERDICT_NOT_SUPPORTED
} TEST_VERDICT;

#define TEST_MAX_MESSAGE_LEN 128

typedef struct {
    TEST_VERDICT Verdict;
    CHAR8        Message[TEST_MAX_MESSAGE_LEN];
} TEST_STATUS;

// ============================================================================
// Helper Macros for Returning Test Verdicts
// ============================================================================

STATIC inline TEST_STATUS GpuTestPass(VOID) {
    TEST_STATUS S;
    S.Verdict = TEST_VERDICT_PASS;
    S.Message[0] = '\0';
    return S;
}

STATIC inline TEST_STATUS GpuTestFail(IN CONST CHAR8 *Msg) {
    TEST_STATUS S;
    S.Verdict = TEST_VERDICT_FAIL;
    AsciiStrCpyS(S.Message, TEST_MAX_MESSAGE_LEN, (Msg != NULL) ? Msg : "Test Failed");
    return S;
}

STATIC inline TEST_STATUS GpuTestSkip(IN CONST CHAR8 *Reason) {
    TEST_STATUS S;
    S.Verdict = TEST_VERDICT_SKIPPED;
    AsciiStrCpyS(S.Message, TEST_MAX_MESSAGE_LEN, (Reason != NULL) ? Reason : "Test Skipped");
    return S;
}

STATIC inline TEST_STATUS GpuTestNotSupported(IN CONST CHAR8 *Reason) {
    TEST_STATUS S;
    S.Verdict = TEST_VERDICT_NOT_SUPPORTED;
    AsciiStrCpyS(S.Message, TEST_MAX_MESSAGE_LEN, (Reason != NULL) ? Reason : "Not Supported");
    return S;
}

#define GPU_TEST_PASS()               GpuTestPass()
#define GPU_TEST_FAIL(Msg)            GpuTestFail(Msg)
#define GPU_TEST_SKIP(Reason)         GpuTestSkip(Reason)
#define GPU_TEST_NOT_SUPPORTED(Reason) GpuTestNotSupported(Reason)

// ============================================================================
// Test Node & Function Signatures
// ============================================================================

typedef TEST_STATUS (*PFN_TEST_RUN)(IN GOP_3D_PROTOCOL *Gop3D);

typedef struct {
    CONST CHAR8  *Path;         // Full test path, e.g. "test-raster.primitive.triangles"
    CONST CHAR8  *Description;  // Human-readable description
    PFN_TEST_RUN Run;           // Test execution entrypoint
} GPU_TEST_NODE;

// ============================================================================
// Test Declaration & Assertion Macros
// ============================================================================

#define GPU_TEST(FuncName, PathStr, DescStr) \
    TEST_STATUS FuncName(IN GOP_3D_PROTOCOL *Gop3D)

#define GPU_ASSERT(Condition, Msg) \
    do { \
        if (!(Condition)) { \
            return GPU_TEST_FAIL(Msg); \
        } \
    } while (0)

#define GPU_ASSERT_EQ(Actual, Expected) \
    do { \
        if ((Actual) != (Expected)) { \
            return GPU_TEST_FAIL("Assertion failed: " #Actual " == " #Expected); \
        } \
    } while (0)

#endif // __HARNESS_H__
