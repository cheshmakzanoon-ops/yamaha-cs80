/* Diagnostic-only Linux preload. Never link into the plugin or release builds. */
#define _GNU_SOURCE
#define UNW_LOCAL_ONLY
#include <libunwind.h>
#include <signal.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <unistd.h>

#define TRACE_FD 198
static atomic_flag active = ATOMIC_FLAG_INIT;

static void emit(const char *text, size_t n)
{
    /* One bounded write; failed/partial output must not prevent termination. */
    if (write(TRACE_FD, text, n) < 0) return;
}
static size_t hex(char *out, uintptr_t value)
{
    static const char digits[] = "0123456789abcdef";
    char reverse[2 * sizeof(value)];
    size_t n = 0, j = 0;
    do { reverse[n++] = digits[value & 15]; value >>= 4; } while (value);
    out[j++] = '0'; out[j++] = 'x';
    while (n) out[j++] = reverse[--n];
    return j;
}
static void trace_signal(int sig, siginfo_t *info, void *context)
{
    (void) info;
    /* atomic_flag is lock-free; SA_NODEFER makes nested faults reach this
       guard rather than silently recur while the signal is blocked. */
    if (atomic_flag_test_and_set_explicit(&active, memory_order_relaxed))
        _exit(128 + sig);
    static const char heading[] = "diagnostic fault-thread trace; numeric failure exit follows\n";
    emit(heading, sizeof(heading) - 1);
    unw_cursor_t cursor;
    if (unw_init_local2(&cursor, (unw_context_t *) context, UNW_INIT_SIGNAL_FRAME) >= 0)
    {
        for (unsigned frame = 0; frame < 32; ++frame)
        {
            unw_word_t ip = 0, offset = 0;
            char name[160] = {0}, line[256];
            if (unw_get_reg(&cursor, UNW_REG_IP, &ip) < 0) break;
            size_t n = 0;
            line[n++] = '#';
            n += hex(line + n, frame);
            line[n++] = ' ';
            n += hex(line + n, (uintptr_t) ip);
            if (unw_get_proc_name(&cursor, name, sizeof(name), &offset) == 0)
            {
                line[n++] = ' ';
                /* Raw mangled symbols only; never demangle or print values.
                   Restrict output characters and length even for corrupt names. */
                for (size_t j = 0; j < sizeof(name) - 1 && name[j]; ++j)
                {
                    char c = name[j];
                    line[n++] = ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                                 || (c >= '0' && c <= '9') || c == '_' || c == '.'
                                 || c == '$' || c == '@') ? c : '?';
                }
                line[n++] = '+';
                n += hex(line + n, (uintptr_t) offset);
            }
            line[n++] = '\n';
            emit(line, n);
            if (unw_step(&cursor) <= 0) break;
        }
    }
    /* Diagnostic numeric status, NOT WIFSIGNALED. Do not resume damaged code
       or request a core. Existing/replacement handlers are outside this claim. */
    _exit(128 + sig);
}
__attribute__((constructor)) static void install_trace(void)
{
    /* Resolve unwinder dependencies outside the handler; -z now handles PLT. */
    unw_context_t context;
    unw_cursor_t cursor;
    unw_getcontext(&context);
    if (unw_init_local(&cursor, &context) >= 0) (void) unw_step(&cursor);
    struct sigaction action = {0};
    action.sa_sigaction = trace_signal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_SIGINFO | SA_NODEFER;
    if (sigaction(SIGSEGV, &action, 0) || sigaction(SIGBUS, &action, 0)
        || sigaction(SIGABRT, &action, 0))
    {
        static const char msg[] = "diagnostic handler installation incomplete\n";
        emit(msg, sizeof(msg) - 1);
        _exit(125);
    }
    static const char ready[] = "diagnostic handler installed; later replacement is possible\n";
    emit(ready, sizeof(ready) - 1);
}
