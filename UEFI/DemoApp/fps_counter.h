#ifndef FPS_COUNTER
#define FPS_COUNTER

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h>
#include <Library/TimerLib.h> 

static UINT32 gFpsFrameCount = 0;
static UINT64 gFpsTotalTicks = 0;
static UINT64 gFpsLastTick   = 0;
static UINT64 gFpsTimerFreq  = 0;

// Timer properties
static BOOLEAN gFpsTimerCountsUp = TRUE;
static UINT64  gFpsTimerMask     = 0xFFFFFFFFFFFFFFFFULL; 


static VOID FpsInitTimer() {
    UINT64 StartVal = 0;
    UINT64 EndVal   = 0;
    
    gFpsTimerFreq = GetPerformanceCounterProperties(&StartVal, &EndVal);
    
    if (gFpsTimerFreq == 0) gFpsTimerFreq = 1000000;

    if (EndVal > StartVal) {
        gFpsTimerCountsUp = TRUE;
        gFpsTimerMask = EndVal; 
    } else {
        gFpsTimerCountsUp = FALSE;
        gFpsTimerMask = StartVal;
    }
}

VOID FpsCounterStart() {
    if (gFpsTimerFreq == 0) {
        FpsInitTimer();
    }

    gFpsFrameCount = 0;
    gFpsTotalTicks = 0;
    gFpsLastTick   = GetPerformanceCounter();
}

VOID FpsCounterTick() {
    gFpsFrameCount++;

    UINT64 CurrentTick = GetPerformanceCounter();
    UINT64 Delta = 0;

    if (gFpsTimerCountsUp) {
        Delta = (CurrentTick - gFpsLastTick) & gFpsTimerMask;
    } else {
        Delta = (gFpsLastTick - CurrentTick) & gFpsTimerMask;
    }

    gFpsTotalTicks += Delta;
    gFpsLastTick    = CurrentTick;
}

VOID FpsCounterStop() {}

VOID FpsCounterShowStats() {
    UINT64 TotalTimeMs = 0;
    if (gFpsTimerFreq > 0) {
        TotalTimeMs = MultU64x64(gFpsTotalTicks, 1000) / gFpsTimerFreq;
    }

    float AvgFps = 0.0f;
    if (TotalTimeMs > 0) {
        AvgFps = (float)gFpsFrameCount / ((float)TotalTimeMs / 1000.0f);
    }

    Print(L"\n========================================\n");
    Print(L"           BENCHMARK RESULTS            \n");
    Print(L"========================================\n");
    Print(L" Total Frames:  %d\n", gFpsFrameCount);
    Print(L" Total Time:    %ld ms\n", TotalTimeMs);
    // Integer-based float printing for UEFI safe output
    Print(L" Average FPS:   %d.%02d\n", (int)AvgFps, (int)((AvgFps - (int)AvgFps) * 100));
    Print(L"========================================\n");
}

#endif