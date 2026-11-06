/* Bounded, persistent save diagnostics. Never include descriptions or save data. */
static const char* save_diagnostic_phase = "idle";
void gamelib_save_log(const char* event, const char* detail)
{
    int saved_errno = errno;
    time_t now = time(NULL);
    struct tm* local = localtime(&now);
    char stamp[32] = "time-unavailable";
    if (local != NULL) strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%S%z", local);
    FILE* log = fopen("ArcanumModern/save-events.log", "ab+");
    if (log != NULL) {
        fseek(log, 0, SEEK_END);
        long length = ftell(log);
        if (length >= 1024 * 1024) {
            fclose(log);
            remove("ArcanumModern/save-events.previous.log");
            if (rename("ArcanumModern/save-events.log", "ArcanumModern/save-events.previous.log") != 0) {
                errno = saved_errno;
                return;
            }
            log = fopen("ArcanumModern/save-events.log", "ab");
        }
        if (log != NULL) {
            fprintf(log, "%s event=%s detail=%.512s phase=%s sector_failed=%d\n",
                stamp, event, detail != NULL ? detail : "", save_diagnostic_phase, sector_save_failed());
            fclose(log);
        }
    }
    errno = saved_errno;
}
