#ifndef __COMMON_FIXTURE_H__
#define __COMMON_FIXTURE_H__

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Protocol/Gop3D.h>

// ============================================================================
// Base Fixture Definitions
// ============================================================================

typedef struct {
    GOP_3D_PROTOCOL *Gop3D;
    BOOLEAN         Initialized;
    UINT32          AllocatedPagesCheckpoint;
} GPU_FIXTURE_BASE;

// ============================================================================
// Sentinel / Checkpoint Stubs (for future expansion)
// ============================================================================

STATIC inline VOID HarnessFixtureCheckpoint(IN GPU_FIXTURE_BASE *Base) {
    if (Base != NULL) {
        Base->AllocatedPagesCheckpoint = 0; 
    }
}

STATIC inline BOOLEAN HarnessFixtureVerify(IN GPU_FIXTURE_BASE *Base) {
    if (Base == NULL) {
        return FALSE;
    }
    return TRUE;
}

#endif // __COMMON_FIXTURE_H__
