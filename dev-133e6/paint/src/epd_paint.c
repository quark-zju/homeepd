#include "epd_paint.h"

int EPD_13in3e_update_image(uint8_t image[EPD_13IN3E_WIDTH / 2 * EPD_13IN3E_HEIGHT])
{
    DEV_ModuleInit();
    DEV_Delay_ms(500);
    EPD_13IN3E_Init();
    // EPD_13IN3E_Clear(EPD_13IN3E_WHITE);
    EPD_13IN3E_Display(image);
    // DEV_Delay_ms(2000);
    return 0;
}

