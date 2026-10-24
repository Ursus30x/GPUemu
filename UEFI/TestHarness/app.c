#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>
#include <Protocol/LoadedImage.h>
#include "test_registry.h"

// ============================================================================
// Shell Protocol Definitions (for retrieving CLI arguments without ShellPkg dependency)
// ============================================================================

STATIC EFI_GUID mShellParametersProtocolGuid =
    { 0x752f3136, 0x4e16, 0x4fdc, { 0xa2, 0x2a, 0xe5, 0xf4, 0x68, 0x12, 0xf4, 0xca } };

STATIC EFI_GUID mShellInterfaceGuid =
    { 0x47c7b223, 0xc42a, 0x11d2, { 0x8e, 0x57, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b } };

typedef struct {
    CHAR16 **Argv;
    UINTN  Argc;
    VOID   *StdIn;
    VOID   *StdOut;
    VOID   *StdErr;
} EFI_SHELL_PARAMETERS_PROTOCOL;

typedef struct {
    EFI_HANDLE ImageHandle;
    VOID       *Info;
    CHAR16     **Argv;
    UINTN      Argc;
} EFI_SHELL_INTERFACE;

// ============================================================================
// CLI Configuration Structure
// ============================================================================

#define MAX_CONFIG_STR_LEN 256

typedef struct {
    CHAR16  CaseFilter[MAX_CONFIG_STR_LEN];
    CHAR16  LogFilePath[MAX_CONFIG_STR_LEN];
    BOOLEAN ListOnly;
    BOOLEAN DryRun;
    BOOLEAN ShowHelp;
} HARNESS_CLI_CONFIG;

// ============================================================================
// CLI Help and Usage
// ============================================================================

STATIC
VOID
PrintBanner(VOID)
{
    Print(L"============================================================\n");
    Print(L"                    GPUemu Test Harness v1.0                \n");
    Print(L"============================================================\n");
}

STATIC
VOID
PrintUsage(
    IN CONST CHAR16 *ProgramName
    )
{
    Print(L"Usage: %s [options]\n\n", (ProgramName != NULL) ? ProgramName : L"TestHarness.efi");
    Print(L"Options:\n");
    Print(L"  --test-case=<pattern>  Filter test cases using wildcards (default: *)\n");
    Print(L"  --test-list            List all matching test cases without executing\n");
    Print(L"  --test-log=<file>      Write test execution results to log file\n");
    Print(L"  --test-dryrun          Simulate test execution without GPU submission\n");
    Print(L"  --help, -h, -?         Display this help message and exit\n\n");
    Print(L"Examples:\n");
    Print(L"  %s --test-case=test-raster.*\n", (ProgramName != NULL) ? ProgramName : L"TestHarness.efi");
    Print(L"  %s --test-case=test-compute.ssbo.* --test-log=FS0:\\test.log\n", (ProgramName != NULL) ? ProgramName : L"TestHarness.efi");
    Print(L"  %s --test-list\n\n", (ProgramName != NULL) ? ProgramName : L"TestHarness.efi");
}

// ============================================================================
// Command Line Retrieval
// ============================================================================

STATIC
EFI_STATUS
GetCommandLineArguments(
    IN  EFI_HANDLE ImageHandle,
    OUT UINTN      *Argc,
    OUT CHAR16     ***Argv
    )
{
    EFI_STATUS                     Status;
    EFI_SHELL_PARAMETERS_PROTOCOL *ShellParameters = NULL;
    EFI_SHELL_INTERFACE           *ShellInterface  = NULL;

    if (Argc == NULL || Argv == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    *Argc = 0;
    *Argv = NULL;

    // Try UEFI Shell 2.0 protocol
    Status = gBS->OpenProtocol(
        ImageHandle,
        &mShellParametersProtocolGuid,
        (VOID **)&ShellParameters,
        ImageHandle,
        NULL,
        EFI_OPEN_PROTOCOL_GET_PROTOCOL
        );
    if (!EFI_ERROR(Status) && ShellParameters != NULL && ShellParameters->Argc > 0) {
        *Argc = ShellParameters->Argc;
        *Argv = ShellParameters->Argv;
        return EFI_SUCCESS;
    }

    // Try legacy EFI Shell interface protocol
    Status = gBS->OpenProtocol(
        ImageHandle,
        &mShellInterfaceGuid,
        (VOID **)&ShellInterface,
        ImageHandle,
        NULL,
        EFI_OPEN_PROTOCOL_GET_PROTOCOL
        );
    if (!EFI_ERROR(Status) && ShellInterface != NULL && ShellInterface->Argc > 0) {
        *Argc = ShellInterface->Argc;
        *Argv = ShellInterface->Argv;
        return EFI_SUCCESS;
    }

    return EFI_NOT_FOUND;
}

// ============================================================================
// CLI Parsing Logic
// ============================================================================

STATIC
BOOLEAN
StartsWith(
    IN CONST CHAR16 *String,
    IN CONST CHAR16 *Prefix
    )
{
    UINTN PrefixLen = StrLen(Prefix);
    if (StrLen(String) < PrefixLen) {
        return FALSE;
    }
    return (StrnCmp(String, Prefix, PrefixLen) == 0);
}

STATIC
EFI_STATUS
ParseCommandLine(
    IN  UINTN              Argc,
    IN  CHAR16             **Argv,
    OUT HARNESS_CLI_CONFIG *Config
    )
{
    UINTN Index;

    if (Config == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    // Set default configuration
    ZeroMem(Config, sizeof(HARNESS_CLI_CONFIG));
    StrCpyS(Config->CaseFilter, MAX_CONFIG_STR_LEN, L"*");
    Config->ListOnly  = FALSE;
    Config->DryRun    = FALSE;
    Config->ShowHelp  = FALSE;

    // If no arguments passed, run with defaults
    if (Argc <= 1 || Argv == NULL) {
        return EFI_SUCCESS;
    }

    for (Index = 1; Index < Argc; Index++) {
        CONST CHAR16 *Arg = Argv[Index];

        if (StrCmp(Arg, L"--help") == 0 ||
            StrCmp(Arg, L"-h") == 0 ||
            StrCmp(Arg, L"-?") == 0 ||
            StrCmp(Arg, L"/?") == 0) {
            Config->ShowHelp = TRUE;
            return EFI_SUCCESS;
        }

        if (StrCmp(Arg, L"--test-list") == 0) {
            Config->ListOnly = TRUE;
            continue;
        }

        if (StrCmp(Arg, L"--test-dryrun") == 0) {
            Config->DryRun = TRUE;
            continue;
        }

        // --test-case=<pattern> or --test-case <pattern>
        if (StartsWith(Arg, L"--test-case=")) {
            StrCpyS(Config->CaseFilter, MAX_CONFIG_STR_LEN, Arg + StrLen(L"--test-case="));
            continue;
        }
        if (StrCmp(Arg, L"--test-case") == 0) {
            if (Index + 1 < Argc) {
                Index++;
                StrCpyS(Config->CaseFilter, MAX_CONFIG_STR_LEN, Argv[Index]);
            } else {
                Print(L"Error: Missing value for --test-case\n");
                return EFI_INVALID_PARAMETER;
            }
            continue;
        }

        // --test-log=<file> or --test-log <file>
        if (StartsWith(Arg, L"--test-log=")) {
            StrCpyS(Config->LogFilePath, MAX_CONFIG_STR_LEN, Arg + StrLen(L"--test-log="));
            continue;
        }
        if (StrCmp(Arg, L"--test-log") == 0) {
            if (Index + 1 < Argc) {
                Index++;
                StrCpyS(Config->LogFilePath, MAX_CONFIG_STR_LEN, Argv[Index]);
            } else {
                Print(L"Error: Missing value for --test-log\n");
                return EFI_INVALID_PARAMETER;
            }
            continue;
        }

        // Unrecognized option
        Print(L"Warning: Unrecognized option '%s'\n", Arg);
    }

    return EFI_SUCCESS;
}

STATIC
VOID
DumpActiveConfig(
    IN CONST HARNESS_CLI_CONFIG *Config
    )
{
    Print(L"Configuration:\n");
    Print(L"  - Test Filter : %s\n", Config->CaseFilter);
    Print(L"  - Run Mode    : %s\n", Config->ListOnly ? L"List Test Cases" : (Config->DryRun ? L"Dry-Run" : L"Execute"));
    Print(L"  - Log File    : %s\n\n", (Config->LogFilePath[0] != L'\0') ? Config->LogFilePath : L"None (Console only)");
}

STATIC
BOOLEAN
PatternMatch(
    IN CONST CHAR16 *Pattern,
    IN CONST CHAR8  *String
    )
{
    while (*Pattern != L'\0') {
        if (*Pattern == L'*') {
            Pattern++;
            if (*Pattern == L'\0') {
                return TRUE;
            }
            while (*String != '\0') {
                if (PatternMatch(Pattern, String)) {
                    return TRUE;
                }
                String++;
            }
            return FALSE;
        }

        if (*String == '\0') {
            return FALSE;
        }

        if (*Pattern != L'?' && *Pattern != (CHAR16)*String) {
            return FALSE;
        }

        Pattern++;
        String++;
    }

    return (*String == '\0');
}

// ============================================================================
// Entry Point
// ============================================================================

EFI_STATUS
EFIAPI
TestHarnessEntry(
    IN EFI_HANDLE       ImageHandle,
    IN EFI_SYSTEM_TABLE *SystemTable
    )
{
    EFI_STATUS         Status;
    UINTN              Argc = 0;
    CHAR16             **Argv = NULL;
    HARNESS_CLI_CONFIG Config;
    UINTN              Index;
    UINTN              MatchedCount = 0;

    PrintBanner();

    // Retrieve arguments from UEFI shell
    Status = GetCommandLineArguments(ImageHandle, &Argc, &Argv);
    if (EFI_ERROR(Status)) {
        DEBUG((DEBUG_WARN, "[TestHarness] Could not retrieve Shell parameters (Status: %r). Using defaults.\n", Status));
    }

    // Parse options
    Status = ParseCommandLine(Argc, Argv, &Config);
    if (EFI_ERROR(Status) || Config.ShowHelp) {
        PrintUsage((Argc > 0 && Argv != NULL) ? Argv[0] : L"TestHarness.efi");
        return Status;
    }

    DumpActiveConfig(&Config);

    // Filter matching tests
    if (Config.ListOnly) {
        Print(L"Discovered Matching Test Cases:\n");
        Print(L"------------------------------------------------------------\n");
    }

    for (Index = 0; Index < gTestRegistryCount; Index++) {
        CONST GPU_TEST_NODE *Node = &gTestRegistry[Index];

        if (PatternMatch(Config.CaseFilter, Node->Path)) {
            MatchedCount++;
            if (Config.ListOnly) {
                Print(L"  [%d] %a\n       %a\n", MatchedCount, Node->Path, Node->Description);
            } else {
                Print(L"  - %a\n", Node->Path);
            }
        }
    }

    Print(L"------------------------------------------------------------\n");
    Print(L"Matched %d of %d available test cases.\n\n", MatchedCount, gTestRegistryCount);

    if (!Config.ListOnly) {
        if (Config.DryRun) {
            Print(L"[DRY-RUN] Test execution simulated for %d cases. (GPU workload skipped)\n", MatchedCount);
        } else {
            Print(L"[EXECUTION] Execution pipeline ready for %d cases.\n", MatchedCount);
        }
    }

    return EFI_SUCCESS;
}
