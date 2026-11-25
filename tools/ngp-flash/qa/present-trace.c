/* QA only. Measure intervals between SDL swap returns (not panel scanout).
 * Build: cc -O2 -Wall -Wextra -Werror -fPIC -shared present-trace.c -ldl -o present-trace.so
 * Single-window/single-render-thread player; no GL calls or input interception. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

typedef struct SDL_Window SDL_Window;
void SDL_GL_SwapWindow(SDL_Window *win) {
    static void (*real_swap)(SDL_Window *);
    static FILE *out;
    static unsigned n;
    if (!real_swap) {
        real_swap = (void (*)(SDL_Window *))dlsym(RTLD_NEXT,"SDL_GL_SwapWindow");
        if (!real_swap) { fputs("QA swap hook: missing SDL symbol\n",stderr); _exit(2); }
        const char *p = getenv("FLASH_QA_PRESENT_TRACE");
        if (p && *p && !(out=fopen(p,"wx"))) perror("QA present trace");
        if (out) fputs("mono_ns\n",out);
    }
    real_swap(win);
    if (out) {
        struct timespec ts;
        if (clock_gettime(CLOCK_MONOTONIC,&ts) || fprintf(out,"%llu\n",
              (unsigned long long)ts.tv_sec*1000000000ULL+(unsigned long long)ts.tv_nsec)<0 ||
            (++n % 16 == 0 && fflush(out)) || n >= 100000) { fclose(out); out=NULL; }
    }
}
