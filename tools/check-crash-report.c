#include "ui/modern_crash.inc"

#ifdef _WIN32
__declspec(noinline) static void test_access_violation(void)
{
    volatile LONG* address = (volatile LONG*)(ULONG_PTR)1;
    *address = 42;
}

int main(int argc, char** argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    modern_crash_install();
    if (argc == 1) return 0;
    if (!strcmp(argv[1], "no-dbghelp")) modern_write_dump = NULL;
    test_access_violation();
    return 1;
}
#else
int main(void) { modern_crash_install(); return 0; }
#endif
