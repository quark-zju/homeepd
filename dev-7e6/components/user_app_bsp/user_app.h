#ifndef USER_APP_H
#define USER_APP_H
#include "freertos/FreeRTOS.h"


uint8_t User_Mode_init(void);       // main.cc

extern EventGroupHandle_t
    Green_led_Mode_queue; 
extern EventGroupHandle_t
    Red_led_Mode_queue; 
extern SemaphoreHandle_t epaper_gui_semapHandle;
extern uint8_t Green_led_arg;           
extern uint8_t Red_led_arg;             
extern EventGroupHandle_t epaper_groups;


char* Get_TemperatureHumidity(void);
extern int sdcard_bmp_Quantity;
extern int sdcard_doc_count; 
//extern int is_ai_img;        
//extern EventGroupHandle_t ai_IMG_Group;
// extern int is_ai_buff_flag;
//extern int IMG_Score; 
//extern SemaphoreHandle_t
//    ai_img_while_semap; 
//extern EventGroupHandle_t ai_IMG_Score_Group; 


void User_Basic_mode_app_init(void);


#endif
