# Requirements

- pico-sdk >= 1.5.0 (for bluetooth support)
- arm-none-eabi-gcc toolchain

# Build instructions

```
mkdir build
cd build
cmake -DPICO_SDK_PATH=/path/to/pico-sdk -DPICO_BOARD=pico_w ..
make -j8
cp pico-ntroller64.uf2 /path/to/RPI-RP2
```

# Limitations

Bluetooth connection is still a bit rough:
- only supports DualShock 4 controller
- must put the controller in sync mode every time (no bonding)
- controller address is hard-coded
- won't retry connection in case of failure
