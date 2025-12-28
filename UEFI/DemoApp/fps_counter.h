#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiRuntimeServicesTableLib.h> // For gRT

#ifndef FPS_COUNTER
#define FPS_COUNTER

// Global State for FPS Counting
static UINT64 gFpsStartTime = 0;
static UINT64 gFpsEndTime   = 0;
static UINT32 gFpsFrameCount = 0;

static UINT64 GetTimeMs() {
    EFI_TIME Time;
    gRT->GetTime(&Time, NULL);
    
    UINT64 ms = (UINT64)Time.Second * 1000 + (UINT64)Time.Nanosecond / 1000000;
    ms += (UINT64)Time.Minute * 60000;
    ms += (UINT64)Time.Hour * 3600000;
    return ms;
}

// Reset and Start the Timer
VOID FpsCounterStart() {
    gFpsFrameCount = 0;
    gFpsStartTime = GetTimeMs();
}

// Call this once per frame
VOID FpsCounterTick() {
    gFpsFrameCount++;
}

// Stop the Timer
VOID FpsCounterStop() {
    gFpsEndTime = GetTimeMs();
}

// Print Stats
VOID FpsCounterShowStats() {
    UINT64 TotalTime = gFpsEndTime - gFpsStartTime;
    float AvgFps = 0.0f;
    
    if (TotalTime > 0) {
        AvgFps = (float)gFpsFrameCount / ((float)TotalTime / 1000.0f);
    }

    Print(L"\n========================================\n");
    Print(L"           BENCHMARK RESULTS            \n");
    Print(L"========================================\n");
    Print(L" Total Frames:  %d\n", gFpsFrameCount);
    Print(L" Total Time:    %ld ms\n", TotalTime);
    // Integer-based float printing for UEFI
    Print(L" Average FPS:   %d.%02d\n", (int)AvgFps, (int)((AvgFps - (int)AvgFps) * 100));
    Print(L"========================================\n");
}

#endif