cmake_minimum_required(VERSION 3.15)

set(CMAKE_TOOLCHAIN_FILE "STM32WB55_toolchain.cmake")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(stm32wb55_usb LANGUAGES C CXX ASM)

# Source files
add_executable(${PROJECT_NAME}
  src/stm32wbxx_it.c
  src/stm32wbxx_hal_msp.c
  startup/startup_stm32wb55cgux.s
  src/main.c
  src/usb_descriptors.c
  src/msc_disk.c
  src/hid_media.c
  src/syscall.c
  src/sysmem.c
  src/system_stm32wbxx.c

  src/hal/stm32wbxx_hal.c
  src/hal/stm32wbxx_hal_rcc.c
  src/hal/stm32wbxx_hal_rcc_ex.c
  src/hal/stm32wbxx_hal_pwr.c
  src/hal/stm32wbxx_hal_pwr_ex.c
  src/hal/stm32wbxx_hal_cortex.c
  src/hal/stm32wbxx_hal_gpio.c
  src/hal/stm32wbxx_hal_pcd.c
  src/hal/stm32wbxx_hal_pcd_ex.c
  src/hal/stm32wbxx_ll_usb.c
  src/hal/stm32wbxx_ll_rcc.c

  tinyusb/src/tusb.c
  tinyusb/src/common/tusb_fifo.c
  tinyusb/src/device/usbd.c
  tinyusb/src/class/cdc/cdc_device.c
  tinyusb/src/class/msc/msc_device.c
  tinyusb/src/class/hid/hid_device.c
  tinyusb/src/portable/st/stm32_fsdev/dcd_stm32_fsdev.c
  tinyusb/src/portable/st/stm32_fsdev/fsdev_common.c
)

# Directories with headers
target_include_directories(${PROJECT_NAME} PRIVATE
  include
  include/hal
  include/hal/Legacy
  include/cmsis

  tinyusb/src
)

# Define compile options
target_compile_options(${PROJECT_NAME} PRIVATE
  -mcpu=cortex-m4
  -mthumb
  -mfloat-abi=hard
  -mfpu=fpv4-sp-d16
  -Os
  # -g
  # -O1
  -flto
  -ffunction-sections # Place functions in separate sections
  -fdata-sections # Place data in separate sections
  -fno-exceptions # Disable exceptions for bare-metal code
)

# Linker options
target_link_options(${PROJECT_NAME} PRIVATE
  -mcpu=cortex-m4
  -mfpu=fpv4-sp-d16
  -mfloat-abi=hard
  -mthumb
  -flto
  -Wl,--start-group
  -Wl,--end-group
  -static
)

# Defines here
target_compile_definitions(${PROJECT_NAME} PRIVATE
  STM32WB55xx # Define your WB series MCU
)

# Convert .elf firmware to .bin after build
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
  COMMAND ${CMAKE_OBJCOPY} -O binary ${PROJECT_NAME} ${PROJECT_NAME}.bin
)

# Print firmware Flash and RAM usage
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
  COMMAND ${CMAKE_SIZE} ${PROJECT_NAME}
)

