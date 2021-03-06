//
// main.c
//
#include "kernel.h"
#include <circle/startup.h>

extern "C" void __cxa_pure_virtual() { while (1); }

int main(void) {
    // cannot return here because some destructors used in CKernel are not implemented

    CKernel Kernel;
    if (!Kernel.Initialize()) {
        halt();
        return EXIT_HALT;
    }

    TShutdownMode ShutdownMode = Kernel.Run();

    switch (ShutdownMode) {
        case ShutdownReboot:
            reboot();
            return EXIT_REBOOT;

        case ShutdownHalt:
        case ShutdownNone:
        default:
            halt();
            return EXIT_HALT;
    }
}
