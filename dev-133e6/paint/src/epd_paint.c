#include "epd_paint.h"
#include <stdio.h>

int EPD_13in3e_update_image(uint8_t image[EPD_13IN3E_WIDTH / 2 * EPD_13IN3E_HEIGHT])
{
    puts("DEV_ModuleInit");
    DEV_ModuleInit();
    DEV_Delay_ms(1000);
    puts("EPD_13IN3E_Init");
    EPD_13IN3E_Init();
    DEV_Delay_ms(1000);
    // EPD_13IN3E_Clear(EPD_13IN3E_WHITE);
    puts("EPD_13IN3E_Display");
    EPD_13IN3E_Display(image);
    DEV_Delay_ms(1000);
    return 0;
}

