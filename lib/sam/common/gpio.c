/*
 * This file is part of the libopencm3 project.
 *
 * Copyright (C) 2012 Gareth McMullin <gareth@blacksphere.co.nz>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <libopencm3/sam/gpio.h>

void gpio_init(uint32_t port, uint32_t pins, enum gpio_flags flags)
{
	switch (flags & 3) {
	case GPIO_FLAG_GPINPUT:
		/* input mode doesn't really exist, so we make a high
		 * output in open-drain mode
		 */
		PIO_SODR(port) = pins;
		flags |= GPIO_FLAG_OPEN_DRAIN;
		/* fall through */
	case GPIO_FLAG_GPOUTPUT:
		PIO_OER(port) = pins;
		PIO_PER(port) = pins;
		break;
	case GPIO_FLAG_PERIPHA:
		PIO_ABSR(port) &= ~pins;
		PIO_PDR(port) = pins;
		break;
	case GPIO_FLAG_PERIPHB:
		PIO_ABSR(port) |= pins;
		PIO_PDR(port) = pins;
	}

	if (flags & GPIO_FLAG_OPEN_DRAIN) {
		PIO_MDER(port) = pins;
	} else {
		PIO_MDDR(port) = pins;
	}

	if (flags & GPIO_FLAG_PULL_UP) {
		PIO_PUER(port) = pins;
	} else {
		PIO_PUDR(port) = pins;
	}
}

void gpio_toggle(uint32_t gpioport, uint32_t gpios)
{
	uint32_t odsr = PIO_ODSR(gpioport);
	PIO_CODR(gpioport) = odsr & gpios;
	PIO_SODR(gpioport) = ~odsr & gpios;
}

struct gpio_isr {
    uint32_t    port;
    uint32_t    pins;
    void        (*isr)(void *arg);
    void        *arg;
};

/* How many GPIO interrupts do we really need? */
static struct gpio_isr gpio_isr_actions[16];

void gpio_init_interrupt(uint32_t port, uint32_t pins, enum gpio_interrupts mode, void (*isr)(void *arg), void *arg)
{
    unsigned int i;
    if (!isr || !pins) return;
    for (i = 0; i < sizeof(gpio_isr_actions)/sizeof(gpio_isr_actions[0]); i++) {
        if (gpio_isr_actions[i].isr) continue;
        /* Add an ISR handler action. */
        gpio_isr_actions[i].port = port;
        gpio_isr_actions[i].pins = pins;
        gpio_isr_actions[i].isr = isr;
        gpio_isr_actions[i].arg = arg;
        /* Configure the PIO interrupt mode registers. */
        switch (mode) {
        case GPIO_INT_EDGE:
            PIO_AIMDR(port) = pins;
            break;
        case GPIO_INT_RISING:
            PIO_AIMER(port) = pins;
            PIO_ESR(port) = pins;
            PIO_REHLSR(port) = pins;
            break;
        case GPIO_INT_FALLING:
            PIO_AIMER(port) = pins;
            PIO_ESR(port) = pins;
            PIO_FELLSR(port) = pins;
            break;
        case GPIO_INT_HIGH:
            PIO_AIMER(port) = pins;
            PIO_LSR(port) = pins;
            PIO_REHLSR(port) = pins;
            break;
        case GPIO_INT_LOW:
            PIO_AIMER(port) = pins;
            PIO_LSR(port) = pins;
            PIO_FELLSR(port) = pins;
            break;
        }
        PIO_IER(port) = pins;
        gpio_init(port, pins, GPIO_FLAG_GPINPUT);
        break;
    }
}

static void
gpio_isr(uint32_t port)
{
    unsigned int i;
    uint32_t sr = PIO_ISR(port);
    for (i = 0; i < sizeof(gpio_isr_actions)/sizeof(gpio_isr_actions[0]); i++) {
        if (!gpio_isr_actions[i].isr) continue;
        if (gpio_isr_actions[i].port != port) continue;
        if (!(gpio_isr_actions[i].pins & sr)) break;
        gpio_isr_actions[i].isr(gpio_isr_actions[i].arg);
    }
}
void pioa_isr(void) { gpio_isr(PIOA); }
void piob_isr(void) { gpio_isr(PIOB); }
void pioc_isr(void) { gpio_isr(PIOC); }

