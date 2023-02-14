#include <stdio.h>
#include <pico/stdlib.h>

int main(void) {
    stdio_init_all();
    printf("Starting loop\n");
    while(1) {
        printf("Hello!\n");
        sleep_ms(1000);
    }
}
