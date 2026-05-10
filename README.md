STM32WB55 USB Sample

Simple USB program for WeAct Studio board based on STM32WB55CGU6
LED is on pin PE4

## How to build and flash (for Ubuntu)

1. Install needed ARM development toolchain and other utilities:
    ```bash
    apt update; apt install -y cmake git usbutils ninja-build gcc-arm-none-eabi stlink-tools
    ```
1. Clone the project:
    ```bash
    git clone --recursive https://github.com/Kotsiubynskyi/stm32wb55_usb.git
    ```
1. Generate build files:
    ```bash
    cd stm32wb55_usb
    mkdir build
    cd build
    cmake .. -G Ninja
    ```
1. Build project from ***build*** folder:
    ```bash
    ninja
    ```
1. Insert ST-LINK programmer USB stick and make sure it's visible by OS:
    ```bash
	lsusb|grep STMicroelectronics
    ```
1. Upload built firmware to MCU:
    ```bash
    st-flash erase && st-flash --reset write stm32wb55_usb.bin 0x8000000
    ```
