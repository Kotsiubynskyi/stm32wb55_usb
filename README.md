STM32WB55 USB Example

Simple USB program for WeAct Studio board based on STM32WB55CGU6

LED is on pin PE4 and user button is on PH3

## How to build and flash (for Ubuntu)

1. Install needed ARM development toolchain and other utilities:

    ```bash
    apt update; apt install -y cmake git usbutils ninja-build gcc-arm-none-eabi stlink-tools
    ```

1. Clone the project:

    ```bash
    git clone --recursive https://github.com/Kotsiubynskyi/stm32wb55_usb.git
    cd stm32wb55_usb
    ```

1. Generate build files:

    ```bash
    cmake -B build -G Ninja
    ```

1. Build project from ***build*** folder:

    ```bash
    cmake --build build
    ```

1. Insert ST-LINK programmer USB stick and make sure it's visible by OS:

    ```bash
    lsusb|grep STMicroelectronics
    ```

1. Upload built firmware to MCU:

    ```bash
    st-flash erase && st-flash --reset write build/stm32wb55_usb.bin 0x8000000
    ```
