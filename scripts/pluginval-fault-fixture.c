#define _GNU_SOURCE
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

static void replacement(int sig) { (void) sig; _exit(73); }
static int handler_installed(int sig)
{
    struct sigaction action;
    return sigaction(sig, 0, &action) == 0
        && (action.sa_flags & SA_SIGINFO);
}
__attribute__((noinline)) void cs80_diagnostic_named_fault(void) { raise(SIGSEGV); }
__attribute__((noinline)) void cs80_diagnostic_instruction_fault(void *page)
{
    /* A real faulting store into our own PROT_NONE mapping, not raise() and
       not a compiler-optimized null dereference. Never inspect its contents. */
    *(volatile unsigned char *) page = 1;
}
__attribute__((noinline)) void *cs80_diagnostic_worker_fault(void *page)
{
    /* Refuse to fault if handler installation is not observable on this thread. */
    if (!handler_installed(SIGSEGV)) _exit(77);
    cs80_diagnostic_instruction_fault(page);
    return 0;
}
int main(int argc, char **argv)
{
    if (argc < 2) return 2;
    int mode = atoi(argv[1]);
    if (mode == 0 || mode == 7) return mode;
    /* Refuse synthetic crashes if preload failed to install its handler. */
    if (!handler_installed(SIGSEGV)) return 77;
    if (mode == 73) signal(SIGSEGV, replacement);
    if (mode == 134 || mode == 135)
    {
        int sig = mode == 134 ? SIGABRT : SIGBUS;
        if (!handler_installed(sig)) return 77;
        raise(sig);
        return 99;
    }
    if (mode == 140 || mode == 141)
    {
        long size = sysconf(_SC_PAGESIZE);
        if (size <= 0) return 78;
        void *page = mmap(0, (size_t) size, PROT_NONE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (page == MAP_FAILED) return 78;
        if (mode == 140)
            cs80_diagnostic_instruction_fault(page);
        else
        {
            pthread_t thread;
            if (pthread_create(&thread, 0, cs80_diagnostic_worker_fault, page) != 0)
                return 79;
            if (pthread_join(thread, 0) != 0) return 80;
        }
        munmap(page, (size_t) size);
    }
    else
        cs80_diagnostic_named_fault();
    return 99;
}
