/*******************************************************************************
*
* demoLoader.c
*
* Rob Laswick / Paul Quevedo
*
* Copyright (C) 2012 www.laswick.net
*
* This program is free software.  It comes without any warranty, to the extent
* permitted by applicable law.  You can redistribute it and/or modify it under
* the terms of the WTF Public License (WTFPL), Version 2, as published by
* Sam Hocevar.  See http://sam.zoy.org/wtfpl/COPYING for more details.
*
*******************************************************************************/
#include <fcntl.h>
#include "kinetis.h"

#include "hardware.h"
#include "globalDefs.h"

static void delay(void)
{
    volatile uint32_t time = 0x0003ffff;
    while (time)
        --time;
}

int main(void)
{
    gpioConfig(N_LED_ORANGE_PORT, N_LED_ORANGE_PIN, GPIO_OUTPUT | GPIO_LOW);
    gpioConfig(N_LED_YELLOW_PORT, N_LED_YELLOW_PIN, GPIO_OUTPUT | GPIO_LOW);
    gpioConfig(N_LED_GREEN_PORT,  N_LED_GREEN_PIN,  GPIO_OUTPUT | GPIO_LOW);
    gpioConfig(N_LED_BLUE_PORT,   N_LED_BLUE_PIN,   GPIO_OUTPUT | GPIO_LOW);

    gpioConfig(N_SWITCH_1_PORT,   N_SWITCH_1_PIN,   GPIO_INPUT);

    for (;;) {
        delay();
        gpioClear(N_LED_ORANGE_PORT, N_LED_ORANGE_PIN);
        delay();
        gpioClear(N_LED_YELLOW_PORT, N_LED_YELLOW_PIN);
        delay();
        gpioClear(N_LED_GREEN_PORT, N_LED_GREEN_PIN);
        delay();
        gpioClear(N_LED_BLUE_PORT, N_LED_BLUE_PIN);

        delay();
        gpioSet(N_LED_ORANGE_PORT, N_LED_ORANGE_PIN);
        delay();
        gpioSet(N_LED_YELLOW_PORT, N_LED_YELLOW_PIN);
        delay();
        gpioSet(N_LED_GREEN_PORT, N_LED_GREEN_PIN);
        delay();
        gpioSet(N_LED_BLUE_PORT, N_LED_BLUE_PIN);

        if (gpioRead(N_SWITCH_1_PORT, N_SWITCH_1_PIN) == 0) {
            int fd;
            uart_install();
            fd = fdevopen(stdin,  "uart3", 0, 0);
            fd = fdevopen(stdout, "uart3", 0, 0);
            fd = fdevopen(stderr, "uart3", 0, 0);
            ioctl(fd, IO_IOCTL_UART_BAUD_SET, 115200);
            loader();
        }
    }

    return 0;
}
