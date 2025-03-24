#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "ssd1306.h"
#include "gfx.h"

#include "pico/stdlib.h"
#include <stdio.h>

const int TRIGGER = 16;
const int ECHO = 17;

SemaphoreHandle_t xSemaphoreTrigger;
QueueHandle_t xQueueTime;
QueueHandle_t xQueueDistance;


void pin_callback(uint gpio, uint32_t events){  
    uint32_t t= 0;
    if(gpio_get(ECHO)){
        t = to_us_since_boot(get_absolute_time());
    }else{
        t = to_us_since_boot(get_absolute_time());
    }
    xQueueSendFromISR(xQueueTime, &t, NULL);

    
    
}
void oled1_btn_led_init(void) {
    gpio_init(TRIGGER);
    gpio_set_dir(TRIGGER, GPIO_OUT);

    gpio_init(ECHO);
    gpio_set_dir(ECHO, GPIO_IN);

    gpio_set_irq_enabled_with_callback(ECHO, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &pin_callback);
}

void trigger_task(void *p){
    while(1){
        vTaskDelay(pdMS_TO_TICKS(0.3));  
        gpio_put(TRIGGER, 1);
        vTaskDelay(pdMS_TO_TICKS(0.01));
        gpio_put(TRIGGER, 0);
        xSemaphoreGive(xSemaphoreTrigger);
    }
}
void echo_task(void *p){
    uint32_t t0 = 0;
    uint32_t tf = 0;
    double distancia;
    while(1){
        if(xQueueReceive(xQueueTime,&t0,0)){
            if(xQueueReceive(xQueueTime,&tf,1000)){
                uint32_t delta = tf - t0; 
                distancia = delta * (0.0343 / 2);
                xQueueSend(xQueueDistance, &distancia, pdMS_TO_TICKS(10));
            }

        }
        
    }
}

void oled_task(void *p) {
    ssd1306_init();

    ssd1306_t disp;
    gfx_init(&disp, 128, 32);


    oled1_btn_led_init();
    double distancia;
    while (1) {
        if (xSemaphoreTake(xSemaphoreTrigger, 0)) {
            if (xQueueReceive(xQueueDistance, &distancia, 500)){
                    char buffer[50];
                    sprintf(buffer, "Dist: %.2f cm", distancia);
                    gfx_clear_buffer(&disp);
                    gfx_draw_string(&disp, 0, 0, 1, buffer);
                    gfx_draw_line(&disp, 0, 27, (distancia/100) * 100, 27);
                    gfx_show(&disp);
                    vTaskDelay(pdMS_TO_TICKS(150));
            } else {
                gfx_clear_buffer(&disp);
                gfx_draw_string(&disp, 0, 0, 1, "Falha");
                gfx_show(&disp);
                vTaskDelay(pdMS_TO_TICKS(150));
            }
        }

    }
}

int main() {
    stdio_init_all();
    xSemaphoreTrigger = xSemaphoreCreateBinary();
    xQueueTime = xQueueCreate(32, sizeof(uint32_t));
    xQueueDistance = xQueueCreate(32, sizeof(double));
    xTaskCreate(echo_task, "Echo", 256, NULL, 1, NULL);
    xTaskCreate(trigger_task, "Trigger", 256, NULL, 1, NULL);
    xTaskCreate(oled_task, "OLED task", 4095, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true)
        ;
}
