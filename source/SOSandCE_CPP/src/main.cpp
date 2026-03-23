#include "core/app.h"
#include <cstdio>
#include <csignal>
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")

static LONG WINAPI crashHandler(EXCEPTION_POINTERS* ep) {
    FILE* f = fopen("crash.log", "w");
    if (f) {
        fprintf(f, "CRASH: Exception code 0x%08lX at address %p\n",
                ep->ExceptionRecord->ExceptionCode,
                ep->ExceptionRecord->ExceptionAddress);
        fprintf(f, "Registers:\n");
        auto& ctx = *ep->ContextRecord;
        fprintf(f, "  RIP: %p  RSP: %p  RBP: %p\n", (void*)ctx.Rip, (void*)ctx.Rsp, (void*)ctx.Rbp);
        fprintf(f, "  RAX: %p  RBX: %p  RCX: %p\n", (void*)ctx.Rax, (void*)ctx.Rbx, (void*)ctx.Rcx);
        fprintf(f, "  RDX: %p  RSI: %p  RDI: %p\n", (void*)ctx.Rdx, (void*)ctx.Rsi, (void*)ctx.Rdi);
        fclose(f);
    }

    char msg[512];
    snprintf(msg, sizeof(msg), "CRASH: Exception 0x%08lX at %p\nSee crash.log in build/Debug/ for details.",
             ep->ExceptionRecord->ExceptionCode, ep->ExceptionRecord->ExceptionAddress);
    MessageBoxA(nullptr, msg, "SOSandCE Crash", MB_OK | MB_ICONERROR);
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

#ifdef _WIN32
    SetUnhandledExceptionFilter(crashHandler);


    if (AttachConsole(ATTACH_PARENT_PROCESS) || AllocConsole()) {
        freopen("CONOUT$", "w", stdout);
        freopen("CONOUT$", "w", stderr);
    }
#endif


    FILE* logFile = fopen("game.log", "w");
    if (logFile) {
        fprintf(logFile, "SOSandCE starting...\n");
        fflush(logFile);
    }

    printf("SOSandCE C++/SDL2 starting...\n");
    fflush(stdout);

    try {
        App app;
        int result = app.run();
        printf("App exited with code %d\n", result);
        if (logFile) { fprintf(logFile, "App exited with code %d\n", result); fclose(logFile); }
        return result;
    } catch (const std::exception& e) {
        fprintf(stderr, "FATAL EXCEPTION: %s\n", e.what());
        if (logFile) { fprintf(logFile, "FATAL EXCEPTION: %s\n", e.what()); fclose(logFile); }
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal Error", e.what(), nullptr);
        return 1;
    } catch (...) {
        fprintf(stderr, "FATAL: Unknown exception\n");
        if (logFile) { fprintf(logFile, "FATAL: Unknown exception\n"); fclose(logFile); }
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal Error", "Unknown exception", nullptr);
        return 1;
    }
}
