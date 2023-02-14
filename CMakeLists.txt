cmake_minimum_required(VERSION 3.12)

# Pull in SDK (must be before project)
include(pico_sdk_import.cmake)

project(pico-ntroller64 C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.3.0")
    message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.3.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

# Initialize the SDK
pico_sdk_init()

add_compile_options(-Wall
        -Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
        -Wno-unused-function # we have some for the docs that aren't called
        -Wno-maybe-uninitialized
        )


# Add project
add_executable(
  pico-ntroller64
  src/main.cpp
  joybus-pio/src/joybus.c
  joybus-pio/src/N64Console.cpp
)

include_directories(joybus-pio/include)

pico_generate_pio_header(pico-ntroller64 ${CMAKE_CURRENT_LIST_DIR}/joybus-pio/src/joybus.pio)

target_link_libraries(pico-ntroller64 pico_stdlib hardware_pio pico_cyw43_arch_none)

pico_enable_stdio_usb(pico-ntroller64 1)
pico_enable_stdio_uart(pico-ntroller64 0)

pico_add_extra_outputs(pico-ntroller64)
