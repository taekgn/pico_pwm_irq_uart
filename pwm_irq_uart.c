#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "hardware/uart.h"

// Pico GPIO pin pre-define
#define twopin 22
#define fourpin 0
#define feedback 16
#define relaying 17

volatile long int isr_cnt = 0;
// counts the number of how many peak occurred
volatile int usr_def = 7;
// user defined interval value via Serial interface
volatile bool flag = true;
// flag variable for PWM-isr
// volatile int16_t buf = getchar_timeout_us(100);

// UART0
#define UART0_ID uart0
#define UART0_BAUD_RATE 115200
#define UART0_TX_GP 0
#define UART0_RX_GP 1
// UART1
#define UART1_ID uart1
#define UART1_BAUD_RATE 115200
#define UART1_TX_GP 8
#define UART1_RX_GP 9
// Pico LED
#define LED_GP 25

void on_pwm_wrap()
// Interrupt function
{
    // Clear the interrupt flag that brought us here
    pwm_clear_irq(pwm_gpio_to_slice_num(twopin));
    if (isr_cnt >= usr_def)
    {
        isr_cnt = 0;
        flag = true;
        gpio_put(relaying, flag);
    }
    else
    {
        isr_cnt++;
        flag = false;
        gpio_put(relaying, flag);
    }
}

uint32_t pwm_set_freq_duty(uint slice_num,
                           uint channel, uint32_t freq, int dutycycle)
{
    uint32_t clock = 125000000;
    // Default Pico Clock speed variable
    uint32_t divider16 = clock / freq / 4096 +
                         (clock % (freq * 4096) != 0);
    if (divider16 / 16 == 0)
        divider16 = 16;
    uint32_t wrap = clock * 16 / divider16 / freq - 1;
    pwm_set_clkdiv_int_frac(slice_num, divider16 / 16,
                            divider16 & 0xF);
    pwm_set_wrap(slice_num, wrap);
    pwm_set_chan_level(slice_num, channel, wrap * dutycycle / 100);
    return wrap;
}

void pulse()
{
    // Generates two Pulse signal; 2MHz and 4MHz with 50% duty cycle
    gpio_set_function(twopin, GPIO_FUNC_PWM);
    uint slice_num2 = pwm_gpio_to_slice_num(twopin);
    uint chan2 = pwm_gpio_to_channel(twopin);
    pwm_set_freq_duty(slice_num2, chan2, 2000000, 50); // Sllice, 2MHz, 50%

    gpio_set_function(relaying, GPIO_FUNC_PWM);
    uint slice_num4 = pwm_gpio_to_slice_num(relaying);
    uint chan4 = pwm_gpio_to_channel(relaying);
    pwm_set_freq_duty(slice_num4, chan4, 4000000, 50); // Sllice, 4MHz, 50%

    pwm_set_enabled(slice_num2, true); // For concurrency
    pwm_set_enabled(slice_num4, true);
}

void delay(uint32_t ms)
{
    busy_wait_us(ms * 1000);
}

void flash(uint8_t times)
{
    for (uint8_t c = 0; c < times; c++)
    {
        gpio_put(LED_GP, 1);
        delay(25);
        gpio_put(LED_GP, 0);
        delay(100);
    }
}

int main()
{
    stdio_init_all();
    // Initialization for USB (Serial) interface
    ////////////////////////////////////////////////////////

    // Tell the LED pin that the PWM is in charge of its value.74
    gpio_set_function(twopin, GPIO_FUNC_PWM);
    // Figure out which slice we just connected to the LED pin
    uint slice_num = pwm_gpio_to_slice_num(twopin);

    // Mask our slice's IRQ output into the PWM block's single interrupt line,
    // and register our interrupt handler
    pwm_clear_irq(slice_num);
    pwm_set_irq_enabled(slice_num, true);
    irq_set_exclusive_handler(PWM_IRQ_WRAP, on_pwm_wrap);
    irq_set_enabled(PWM_IRQ_WRAP, true);

    uint chan2 = pwm_gpio_to_channel(twopin);
    pwm_set_freq_duty(slice_num, chan2, 1000, 50); // Slice, 2MHz, 50%
    pwm_set_enabled(slice_num, true);              // For concurrency

    gpio_init(relaying);
    gpio_set_dir(relaying, GPIO_OUT);

    ///////////////////////////////////////////////////////////////////////////

    // Setup the USB as as a serial port
    stdio_usb_init();
    // Setup the UART0 port as a seperate item
    uart_init(UART0_ID, UART0_BAUD_RATE);
    gpio_set_function(UART0_TX_GP, GPIO_FUNC_UART);
    gpio_set_function(UART0_RX_GP, GPIO_FUNC_UART);
    // Setup the UART1 port
    uart_init(UART1_ID, UART1_BAUD_RATE);
    gpio_set_function(UART1_TX_GP, GPIO_FUNC_UART);
    gpio_set_function(UART1_RX_GP, GPIO_FUNC_UART);

    delay(1000); // Give time for the USB to be setup
    flash(3);    // Time to connect that USB port
    delay(2000); // Give time for the USB to be connected by the user clicking things

    puts("USB with debug");
    uart_puts(UART0_ID, "UART0 with debug\n");
    // uart_puts(UART1_ID, "UART1\n");

    delay(1000); // Give time for the USB to be setup

    ////////////////////////////////////////////////////////////////////////////////////
    while (true)
    {
        int32_t buf = getchar_timeout_us(100);
        char cbuf; char str[10];
        // USB serial to UART0 + UART1
        int32_t ch = getchar();//getchar_timeout_us(100);
        while (ch != PICO_ERROR_TIMEOUT)
        {
            uart_putc_raw(UART0_ID, ch);  // Send to UART0
            uart_putc_raw(UART1_ID, ch);  // Send to UART1
            printf("Before Limit is: %d\n", usr_def);
            printf("Before Buffer is: %d\n", buf);
            printf("USB to UART0 n UART1: %c\n", ch); // Echo back so you can see what you've done
            //ch = getchar_timeout_us(100);
            ch = getchar();
            usr_def = (int)ch-48;
            isr_cnt = 0;
            printf("After Limit is: %d\n", usr_def);
            printf("After Buffer is: %d\n", buf);
        }
    }
}