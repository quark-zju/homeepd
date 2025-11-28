#ifndef _TEST_H_
#define _TEST_H_

#include <stdlib.h>     //exit()
#include <signal.h>     //signal()
#include "DEV_Config.h"
#include "EPD_13in3e.h"

int EPD_13in3e_update_image(uint8_t image[EPD_13IN3E_WIDTH / 2 * EPD_13IN3E_HEIGHT]);
#endif
