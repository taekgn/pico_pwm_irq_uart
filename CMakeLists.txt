cmake_minimum_required(VERSION 3.25)

include(pico_sdk_import.cmake)

project(pwm_irq_uart)

pico_sdk_init()
if (TARGET tinyusb_device) 
#Above code will enable serial comms
    add_executable(${PROJECT_NAME}
    ${PROJECT_NAME}.c
    )

    pico_enable_stdio_usb(${PROJECT_NAME} 1)
    #This will enable serial output via USB 1 is enable
    pico_enable_stdio_uart(${PROJECT_NAME} 1)
    #This will enable serial output via UART 0 is disable

    #Everything when you call library make sure add their name here
    #Order :: Filename, library 1, library 2 and so on
    target_link_libraries(${PROJECT_NAME} pico_stdlib hardware_pwm hardware_irq hardware_uart)
    # create map/bin/hex file etc.
    pico_add_extra_outputs(${PROJECT_NAME})
elseif(PICO_ON_DEVICE)
    message(WARNING "not building hello_usb because TinyUSB submodule is not initialized in the SDK")
endif()