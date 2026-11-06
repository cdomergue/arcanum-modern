/* Local Windows crash reports. No network access or game-state writes. */
#if defined(_WIN32)
#include <windows.h>
#include <dbghelp.h>
#include <stdio.h>
#include <wchar.h>
#include "modern_version.h"

typedef BOOL (WINAPI *ModernWriteDump)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE,
    PMINIDUMP_EXCEPTION_INFORMATION, PMINIDUMP_USER_STREAM_INFORMATION,
    PMINIDUMP_CALLBACK_INFORMATION);
static ModernWriteDump modern_write_dump;
static HANDLE modern_crash_event, modern_crash_done;
static EXCEPTION_POINTERS* modern_crash_exception;
static DWORD modern_crash_thread;
static volatile LONG modern_crash_started;
static wchar_t modern_crash_prefix[32768], modern_crash_manifest[32768];

static DWORD WINAPI modern_crash_writer(void* unused)
{
    (void)unused;
    if (WaitForSingleObject(modern_crash_event, INFINITE) != WAIT_OBJECT_0) return 0;
    wchar_t path[32768];
    char report[1024];
    EXCEPTION_RECORD* record = modern_crash_exception->ExceptionRecord;
    DWORD written, dump_error = ERROR_PROC_NOT_FOUND;
    swprintf(path, 32768, L"%ls.dmp", modern_crash_prefix);
    if (modern_write_dump) {
        HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file != INVALID_HANDLE_VALUE) {
            MINIDUMP_EXCEPTION_INFORMATION info;
            info.ThreadId = modern_crash_thread;
            info.ExceptionPointers = modern_crash_exception;
            info.ClientPointers = FALSE;
            BOOL ok = modern_write_dump(GetCurrentProcess(), GetCurrentProcessId(), file,
                (MINIDUMP_TYPE)(MiniDumpNormal | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules),
                &info, NULL, NULL);
            dump_error = ok ? ERROR_SUCCESS : GetLastError();
            FlushFileBuffers(file);
            CloseHandle(file);
            if (!ok) DeleteFileW(path);
        } else dump_error = GetLastError();
    }
    MEMORY_BASIC_INFORMATION region;
    void* base = NULL;
    if (VirtualQuery(record->ExceptionAddress, &region, sizeof(region))) base = region.AllocationBase;
    int size = snprintf(report, sizeof(report),
        "Arcanum Renaissance %s\r\nPID: %lu\r\nThread: %lu\r\n"
        "Exception: 0x%08lX\r\nAddress: %p\r\nModule base: %p\r\n"
        "Module offset: 0x%llX\r\nDump result (Windows error, 0 = success): %lu\r\n"
        "The dump contains thread contexts, stacks and module identities.\r\n"
        "Reports stay on this computer. Review them before sharing.\r\n",
        ARCANUM_RENAISSANCE_VERSION, GetCurrentProcessId(), modern_crash_thread,
        record->ExceptionCode, record->ExceptionAddress, base,
        (unsigned long long)((ULONG_PTR)record->ExceptionAddress - (ULONG_PTR)base), dump_error);
    swprintf(path, 32768, L"%ls.txt", modern_crash_prefix);
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file != INVALID_HANDLE_VALUE) {
        if (size > 0 && size < (int)sizeof(report)) WriteFile(file, report, (DWORD)size, &written, NULL);
        FlushFileBuffers(file);
        CloseHandle(file);
    }
    swprintf(path, 32768, L"%ls.manifest.json", modern_crash_prefix);
    CopyFileW(modern_crash_manifest, path, TRUE);
    SetEvent(modern_crash_done);
    return 0;
}

static LONG WINAPI modern_crash_filter(EXCEPTION_POINTERS* exception)
{
    if (InterlockedCompareExchange(&modern_crash_started, 1, 0) == 0) {
        modern_crash_exception = exception;
        modern_crash_thread = GetCurrentThreadId();
        SetEvent(modern_crash_event);
        /* A separate stack lets stack-overflow crashes produce a report too.
         * Never hang indefinitely if Windows cannot finish writing the dump. */
        WaitForSingleObject(modern_crash_done, 10000);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static int modern_crash_directory(const wchar_t* directory)
{
    wchar_t probe[32768];
    CreateDirectoryW(directory, NULL);
    SYSTEMTIME now;
    GetSystemTime(&now);
    if (wcslen(directory) > 32000) return 0;
    swprintf(modern_crash_prefix, 32768, L"%ls\\crash-%04u%02u%02u-%02u%02u%02u-%03u-%lu",
        directory, now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond,
        now.wMilliseconds, GetCurrentProcessId());
    swprintf(probe, 32768, L"%ls.probe", modern_crash_prefix);
    HANDLE file = CreateFileW(probe, GENERIC_WRITE, 0, NULL, CREATE_NEW,
        FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, NULL);
    if (file == INVALID_HANDLE_VALUE) return 0;
    CloseHandle(file);
    return 1;
}

static void modern_crash_install(void)
{
    wchar_t directory[32768];
    DWORD length = GetModuleFileNameW(NULL, directory, 32768);
    if (!length || length >= 32000) return;
    wchar_t* slash = wcsrchr(directory, L'\\');
    if (!slash) return;
    *slash = 0;
    wcscat(directory, L"\\ArcanumModern");
    CreateDirectoryW(directory, NULL);
    swprintf(modern_crash_manifest, 32768, L"%ls\\manifest.json", directory);
    wcscat(directory, L"\\CrashReports");
    if (!modern_crash_directory(directory)) {
        length = GetEnvironmentVariableW(L"LOCALAPPDATA", directory, 32768);
        if (!length || length >= 32000) return;
        wcscat(directory, L"\\ArcanumRenaissance");
        CreateDirectoryW(directory, NULL);
        wcscat(directory, L"\\CrashReports");
        if (!modern_crash_directory(directory)) return;
    }
    /* Load the OS library before a crash, never a DLL from the game folder. */
    length = GetSystemDirectoryW(directory, 32768);
    if (length && length < 32000) {
        wcscat(directory, L"\\dbghelp.dll");
        HMODULE library = LoadLibraryW(directory);
        if (library) modern_write_dump = (ModernWriteDump)GetProcAddress(library, "MiniDumpWriteDump");
    }
    modern_crash_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    modern_crash_done = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (!modern_crash_event || !modern_crash_done) return;
    HANDLE worker = CreateThread(NULL, 0, modern_crash_writer, NULL, 0, NULL);
    if (!worker) return;
    CloseHandle(worker);
    ULONG reserve = 65536;
    SetThreadStackGuarantee(&reserve);
    SetUnhandledExceptionFilter(modern_crash_filter);
}
#else
static void modern_crash_install(void) { }
#endif
