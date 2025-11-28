#include <stdlib.h>     //exit()
#include <stdio.h>
#include <signal.h>     //signal()
#include <time.h>
#include "epd_paint.h"

int Version;

void  Handler(int signo)
{
    // System Exit
    DEV_ModuleExit();
    exit(0);
}

#define SIZE (EPD_13IN3E_HEIGHT * EPD_13IN3E_WIDTH / 2)

int main(int argc, const char *argv[])
{
    // Exception handling:ctrl + c
    signal(SIGINT, Handler);

    uint8_t image[SIZE];
    size_t n = fread(image, SIZE, 1, stdin);
    if (n != 1) {
        return 1;
    } else {
        EPD_13in3e_update_image(image);
        DEV_ModuleExit();
        return 0;
    }
}
