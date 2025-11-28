#include "axp_prot.h"
#include "button_bsp.h"
#include "driver/rtc_io.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "json_data.h"
#include "led_bsp.h"
#include "sdcard_bsp.h"
#include "user_app.h"
#include "axp_prot.h"
#include <stdio.h>

#include "GUI_BMPfile.h"
#include "GUI_Paint.h"
#include "epaper_port.h"
#include "client_bsp.h"

#define ext_wakeup_pin_1 GPIO_NUM_0 
#define ext_wakeup_pin_2 GPIO_NUM_5 
#define ext_wakeup_pin_3 GPIO_NUM_4 

static uint8_t *epd_blackImage = NULL; // Image buffer
static uint32_t Imagesize;             // Size of image buffer

static uint32_t               sdcard_Basic_bmp   = 0; 
static RTC_DATA_ATTR uint32_t sdcard_Basic_count = 0; 


// User sets the wake-up time in seconds.
static RTC_DATA_ATTR uint64_t basic_rtc_set_time = 3600 * 24;   // Default to 24 hours

static uint8_t           Basic_sleep_arg = 0; // Parameters for low-power tasks
static SemaphoreHandle_t sleep_Semp;          // Binary call low-power task
 
static uint8_t           wakeup_basic_flag = 0;

static void pwr_button_user_Task(void *arg) {
    for (;;) {
        EventBits_t even = xEventGroupWaitBits(pwr_groups, set_bit_all, pdTRUE,
                                               pdFALSE, pdMS_TO_TICKS(2000));
        if (get_bit_button(even, 0)) // Immediately enter low-power mode
        {
            esp_sleep_pd_config(ESP_PD_DOMAIN_MAX, ESP_PD_OPTION_AUTO);
            esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);     
            const uint64_t ext_wakeup_pin_1_mask = 1ULL << ext_wakeup_pin_1;
            const uint64_t ext_wakeup_pin_3_mask = 1ULL << ext_wakeup_pin_3;
            ESP_ERROR_CHECK(esp_sleep_enable_ext1_wakeup_io(ext_wakeup_pin_1_mask | ext_wakeup_pin_3_mask, ESP_EXT1_WAKEUP_ANY_LOW)); 
            ESP_ERROR_CHECK(rtc_gpio_pulldown_dis(ext_wakeup_pin_3));
            ESP_ERROR_CHECK(rtc_gpio_pullup_en(ext_wakeup_pin_3));
            uint64_t sleep_time = basic_rtc_set_time * 1000 * 1000;
            esp_sleep_enable_timer_wakeup(sleep_time);
            //axp_basic_sleep_start();
            ESP_LOGI("SLEEP", "Go to deep sleep now (pwr_button). Wakup time: %llu us.", sleep_time);
            vTaskDelay(pdMS_TO_TICKS(500));
            esp_deep_sleep_start(); 
        }
    }
}

static void refresh_image() {
    if (pdTRUE == xSemaphoreTake(epaper_gui_semapHandle, 2000)) {
        xEventGroupSetBits(Green_led_Mode_queue, set_bit_button(6));
        Green_led_arg                   = 2;
        const char *sp6 = fetch_sp6();
        Green_led_arg                   = 0;
        if (sp6 != NULL) {
            // Process the fetched data.
            ESP_LOGI("SP6", "Use network SP6 image.");
            // PaintSp6(sp6);
            // ESP_LOGI("SP6", "SP6 painted.");
            Green_led_arg                   = 1;
            epaper_port_display((uint8_t*)sp6);
            ESP_LOGI("SP6", "Network SP6 displayed.");
            heap_caps_free((void *)sp6);
        } else {
            // Fallback to SD card image.
            ESP_LOGE("SP6", "Failed to fetch SP6 data. Fallback to SD card image.");
            list_node_t *sdcard_node = list_at(sdcard_scan_listhandle, sdcard_Basic_count); 
            if (sdcard_node == NULL) {
                sdcard_Basic_count = 0;
                sdcard_node        = list_at(sdcard_scan_listhandle, sdcard_Basic_count);
            }
            ESP_LOGI("node", "Image count: %ld", sdcard_Basic_count);
            sdcard_Basic_count++;
            if (sdcard_node != NULL) 
            {
                epd_blackImage = (uint8_t *) heap_caps_malloc(Imagesize * sizeof(uint8_t), MALLOC_CAP_SPIRAM);
                assert(epd_blackImage);
                /*刷图的公共部分*/
                Paint_NewImage(epd_blackImage, EXAMPLE_LCD_WIDTH, EXAMPLE_LCD_HEIGHT, 0, EPD_7IN3E_WHITE);
                Paint_SetScale(6);
                Paint_SelectImage(epd_blackImage); 
                Paint_SetRotate(180);
                xEventGroupSetBits(Green_led_Mode_queue,
                                set_bit_button(6));
                Green_led_arg                   = 1;
                sdcard_node_t *sdcard_Name_node = (sdcard_node_t *) sdcard_node->val;
                GUI_ReadBmp_RGB_6Color(sdcard_Name_node->sdcard_name, 0, 0);
                epaper_port_display(epd_blackImage);
                heap_caps_free((void *)epd_blackImage);
            }
        }
        xSemaphoreGive(epaper_gui_semapHandle); 
        Green_led_arg = 0;
    }
}

static void refresh_image_and_sleep() {
    refresh_image();
    Basic_sleep_arg = 1;
    xSemaphoreGive(sleep_Semp);
}

static void boot_button_user_Task(void *arg) {
    Imagesize      = ((EXAMPLE_LCD_WIDTH % 2 == 0) ? (EXAMPLE_LCD_WIDTH / 2) : (EXAMPLE_LCD_WIDTH / 2 + 1)) * EXAMPLE_LCD_HEIGHT;

    uint8_t *wakeup_arg = (uint8_t *) arg;
    for (;;) {
        EventBits_t even = xEventGroupWaitBits(boot_groups, set_bit_all, pdTRUE, pdFALSE, pdMS_TO_TICKS(2000));
        if (get_bit_button(even, 0)) {
            if (*wakeup_arg == 0) {
                refresh_image_and_sleep();
            }
        }
    }
}

static void default_sleep_user_Task(void *arg) {
    uint8_t *sleep_arg = (uint8_t *) arg;
    for (;;) {
        if (pdTRUE == xSemaphoreTake(sleep_Semp, portMAX_DELAY)) {
            if (*sleep_arg == 1 && !axpIsCharging) {
                uint64_t sleep_time = basic_rtc_set_time * 1000 * 1000;
                ESP_LOGI("SLEEP", "Go to deep sleep now (default). Wakup time: %llu us.", sleep_time);
                esp_sleep_pd_config(
                    ESP_PD_DOMAIN_MAX,
                    ESP_PD_OPTION_AUTO);   
                esp_sleep_disable_wakeup_source(
                    ESP_SLEEP_WAKEUP_ALL); 
                const uint64_t ext_wakeup_pin_1_mask = 1ULL << ext_wakeup_pin_1;
                const uint64_t ext_wakeup_pin_3_mask = 1ULL << ext_wakeup_pin_3;
                ESP_ERROR_CHECK(esp_sleep_enable_ext1_wakeup_io(
                    ext_wakeup_pin_1_mask | ext_wakeup_pin_3_mask,
                    ESP_EXT1_WAKEUP_ANY_LOW)); 
                ESP_ERROR_CHECK(rtc_gpio_pulldown_dis(ext_wakeup_pin_3));
                ESP_ERROR_CHECK(rtc_gpio_pullup_en(ext_wakeup_pin_3));
                //axp_basic_sleep_start(); 
                esp_sleep_enable_timer_wakeup(sleep_time);
                vTaskDelay(pdMS_TO_TICKS(500));
                esp_deep_sleep_start();  
            } else {
                ESP_LOGI("SLEEP", "Skip deep sleep mode.");
                vTaskDelay(pdMS_TO_TICKS(500));
            }
        }
    }
}

static void get_wakeup_gpio(void) {
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    if (ESP_SLEEP_WAKEUP_EXT1 == wakeup_reason) {
        uint64_t wakeup_pins = esp_sleep_get_ext1_wakeup_status();
        ESP_LOGI("WAKEUP", "Wakeup by RTC_CNTL");
        if (wakeup_pins == 0) {
            ESP_LOGI("WAKEUP", "No wakeup pins detected!");
            return;
        }
        if (wakeup_pins & (1ULL << ext_wakeup_pin_1)) {
            // esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
            // //Disable the previous timer first
            // esp_sleep_enable_timer_wakeup(basic_rtc_set_time * 1000 * 1000);
            // //Reset the 10-second timer
            ESP_LOGI("WAKEUP", "Wakeup caused by PIN1");
            xEventGroupSetBits(boot_groups, set_bit_button(0)); 
        } else if (wakeup_pins & (1ULL << ext_wakeup_pin_3)) {
            ESP_LOGI("WAKEUP", "Wakeup caused by PIN3");
            return;
        } else {
            ESP_LOGI("WAKEUP", "Wakeup caused by other pins");
        }
    } else if (ESP_SLEEP_WAKEUP_TIMER == wakeup_reason) {
        ESP_LOGI("WAKEUP", "Wakeup by TIMER");
        xEventGroupSetBits(boot_groups, set_bit_button(0)); 
    }
}

void User_Basic_mode_app_init(void) {
    sleep_Semp  = xSemaphoreCreateBinary();
    xEventGroupSetBits(Red_led_Mode_queue, set_bit_button(0));
    list_scan_dir("/sdcard/06_user_foundation_img");        
    sdcard_Basic_bmp = list_iterator();
    xTaskCreate(boot_button_user_Task, "boot_button_user_Task", 6 * 1024, &wakeup_basic_flag, 3, NULL);
    xTaskCreate(pwr_button_user_Task, "pwr_button_user_Task", 4 * 1024, NULL, 3, NULL);
    xTaskCreate(default_sleep_user_Task, "default_sleep_user_Task", 4 * 1024, &Basic_sleep_arg, 3, NULL); 
    get_wakeup_gpio();
    vTaskDelay(pdMS_TO_TICKS(500));
    refresh_image_and_sleep();
}