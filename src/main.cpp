#include <hardware/pio.h>
#include <stdio.h>
#include <pico/stdlib.h>
#include <pico/cyw43_arch.h>

#include "N64Console.hpp"
#include "n64_definitions.h"

N64Console *console;
uint32_t ms_since_boot = 0;
uint32_t ms_last_switch = 0;
bool led = false;

int main(void) {
    stdio_init_all();
    printf("Starting pico-ntroller64\n");
    
    if (cyw43_arch_init()) {
        printf("cyw43 init failed\n");
        return -1;
    }

    printf("Starting loop\n");
    while(1) {
        // TODO Change state of start/A buttons every 500ms
        ms_since_boot = to_ms_since_boot(get_absolute_time());
        if ((ms_since_boot - ms_last_switch) >= 500) {
            printf("Switching buttons\n");
            ms_last_switch = ms_since_boot;
            //report.start = !report.start;
            //report.a = !report.a;
            led = !led;
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led);
        }
    }
}
