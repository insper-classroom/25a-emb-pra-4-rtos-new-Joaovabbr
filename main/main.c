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
volatile uint32_t t0 = 0;
volatile uint32_t tf;
volatile int erro = 0;

SemaphoreHandle_t xSemaphoreTrigger;
QueueHandle_t xQueueTime;
QueueHandle_t xQueueDistance;


void pin_callback(uint gpio, uint32_t events){  
    if(gpio_get(ECHO)){
        t0 = to_us_since_boot(get_absolute_time());
    }else{
        tf = to_us_since_boot(get_absolute_time());
        // faça o print do delta mas dentro deste else
        printf("Delta: %d\n", tf - t0);
        uint32_t delta = tf - t0; 
        xQueueSendFromISR(xQueueTime, &delta, NULL);
    }
    
    
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
    uint32_t time;
    double distancia;
    while(1){
        while (tf == 0 ){
            erro = to_us_since_boot(get_absolute_time()) - t0 > 500000 ? 1 : 0;
        }

        if (erro == 1) {
            distancia = -1;
        }
        
        if(xQueueReceive(xQueueTime,&time,0)){
            distancia = time * (0.0343 / 2);
            
        }
        xQueueSend(xQueueDistance, &distancia, 0);
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
            if (xQueueReceive(xQueueDistance, &distancia, 0)){
                if (distancia != -1){
                    char buffer[50];
                    sprintf(buffer, "Dist: %.2f cm", distancia);
                    gfx_clear_buffer(&disp);
                    gfx_draw_string(&disp, 0, 0, 1, buffer);
                    gfx_draw_line(&disp, 0, 27, (distancia/100) * 100, 27);
                    gfx_show(&disp);
                    vTaskDelay(pdMS_TO_TICKS(150));
                }
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
