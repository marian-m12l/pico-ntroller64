#include <hardware/pio.h>
#include <stdio.h>
#include <pico/stdlib.h>
#include <pico/cyw43_arch.h>
#include <pico/multicore.h>

#include "N64Console.hpp"
#include "n64_definitions.h"

#define JOYBUS_PIN 0

n64_report_t report = default_n64_report;
uint32_t ms_since_boot = 0;
uint32_t ms_last_switch = 0;

void core1_entry() {
    printf("[core1] Initializing joybus on core 1\n");
    N64Console *console = new N64Console(JOYBUS_PIN, pio0);
    // Handle poll commands
    while(true) {
        //printf("[core1] Waiting for POLL command from console...\n");
        console->WaitForPoll();
        // TODO Handle concurrent access to report!
        // TODO use a fifo to transfer report between cores?
        //printf("[core1] sending report\n");
        console->SendReport(&report);
    }
}

int main(void) {
    set_sys_clock_khz(130'000, true);

    stdio_init_all();
    printf("[core0] Starting pico-ntroller64\n");
    
    if (cyw43_arch_init()) {
        printf("[core0] cyw43 init failed\n");
        return -1;
    }

    multicore_launch_core1(core1_entry);

    printf("[core0] Starting loop\n");
    while(1) {
        // Change state of start/A buttons every 500ms
        ms_since_boot = to_ms_since_boot(get_absolute_time());
        if ((ms_since_boot - ms_last_switch) >= 500) {
            printf("[core0] Switching buttons\n");
            ms_last_switch = ms_since_boot;
            report.start = !report.start;
            report.a = !report.a;
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, report.start);
        }
    }
}
