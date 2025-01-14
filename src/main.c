/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "retained.h"

#include <inttypes.h>
#include <stdio.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/pm/device.h>
#include <zephyr/sys/poweroff.h>
#include <zephyr/sys/util.h>

#include <hal/nrf_reset.h>
#include <hal/nrf_power.h>
#include <hal/nrf_gpio.h>

#if IS_ENABLED(CONFIG_GRTC_WAKEUP_ENABLE)
#include <zephyr/drivers/timer/nrf_grtc_timer.h>
#define DEEP_SLEEP_TIME_S 2
#else
static const struct gpio_dt_spec sw0 = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios); // this is P1.13 on the 54L15DK
static const struct gpio_dt_spec sw1 = GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios); // 		  P1.09 ^
static const struct gpio_dt_spec sw2 = GPIO_DT_SPEC_GET(DT_ALIAS(sw2), gpios); // 		  P1.08 ^
static const struct gpio_dt_spec sw3 = GPIO_DT_SPEC_GET(DT_ALIAS(sw3), gpios); // 		  P0.04 ^
#endif

#define dk_sw0_msk 1 << 13
#define dk_sw1_msk 1 << 9
#define dk_sw2_msk 1 << 8
#define dk_sw3_msk 1 << 4

static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

static int init_io(void)
{
	int rc = 0;
	gpio_pin_configure_dt(&sw0, GPIO_INPUT);
	if (rc < 0)
	{
		printf("Could not configure sw0 GPIO (%d)\n", rc);
		return 0;
	}

	rc = gpio_pin_interrupt_configure_dt(&sw0, GPIO_INT_LEVEL_ACTIVE);
	if (rc < 0)
	{
		printf("Could not configure sw0 GPIO interrupt (%d)\n", rc);
		return 0;
	}

	gpio_pin_configure_dt(&sw1, GPIO_INPUT);
	if (rc < 0)
	{
		printf("Could not configure sw1 GPIO (%d)\n", rc);
		return 0;
	}

	rc = gpio_pin_interrupt_configure_dt(&sw1, GPIO_INT_LEVEL_ACTIVE);
	if (rc < 0)
	{
		printf("Could not configure sw1 GPIO interrupt (%d)\n", rc);
		return 0;
	}

	gpio_pin_configure_dt(&sw2, GPIO_INPUT);
	if (rc < 0)
	{
		printf("Could not configure sw2 GPIO (%d)\n", rc);
		return 0;
	}

	rc = gpio_pin_interrupt_configure_dt(&sw2, GPIO_INT_LEVEL_ACTIVE);
	if (rc < 0)
	{
		printf("Could not configure sw2 GPIO interrupt (%d)\n", rc);
		return 0;
	}

	rc = gpio_pin_configure_dt(&sw3, GPIO_INPUT);
	if (rc < 0)
	{
		printf("Could not configure sw3 GPIO (%d)\n", rc);
		return 0;
	}

	rc = gpio_pin_interrupt_configure_dt(&sw3, GPIO_INT_LEVEL_ACTIVE);
	if (rc < 0)
	{
		printf("Could not configure sw3 GPIO interrupt (%d)\n", rc);
		return 0;
	}

	// configure led
	rc = gpio_pin_configure_dt(&led0, GPIO_OUTPUT_INACTIVE);
	if (rc < 0)
	{
		printf("Could not configure led0 GPIO (%d)\n", rc);
		return 0;
	}
	return rc;
}


static void blink(uint8_t num_blinks)
{
	for (uint8_t blink_cnt = 0; blink_cnt < num_blinks; blink_cnt++)
	{
		gpio_pin_toggle_dt(&led0);
		k_msleep(200);
		gpio_pin_toggle_dt(&led0);
		k_msleep(200);
	}
}

static void wakeup_io_src_get()
{
	// check and reset latch registers, specific to 54l15
	// since these buttons are across ports, need to logic around both port latch registers
	volatile uint32_t p0_latch = NRF_P0_S->LATCH;
	volatile uint32_t p1_latch = NRF_P1_S->LATCH;
	printf("LATCH REGISTER FOR P0: %d\n", p0_latch);
	printf("LATCH REGISTER FOR P1: %d\n", p1_latch);

	// Your logic here will change depending on your device and ports.
	if (p1_latch > 0) // check if p1 was the source
	{
		switch (p1_latch)
		{
		case dk_sw0_msk:
			printk("WAKEUP SRC: SW0\n");
			blink(1);
			break;
		case dk_sw1_msk:
			printk("WAKEUP SRC: SW1\n");
			blink(2);
			break;
		case dk_sw2_msk:
			printk("WAKEUP SRC: SW2\n");
			blink(3);
			break;
		default:
			printk("WAKEUP SRC: ???\n");
			break;
		}
	}
	else if (p0_latch > 0) // check if p0 was the source
	{
		switch (p0_latch)
		{
		case dk_sw3_msk:
			printk("WAKEUP SRC: SW3\n");
			blink(4);
			break;
		default:
			printk("WAKEUP SRC: ???\n");
			break;
		}
	}
	else
	{
		// unknown wakeup source
	}

	// clear latch
	NRF_P0_S->LATCH = NRF_P0_S->LATCH;
	NRF_P1_S->LATCH = NRF_P1_S->LATCH;
}


int main(void)
{
	int rc;
	const struct device *const cons = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

	if (!device_is_ready(cons))
	{
		printf("%s: device not ready.\n", cons->name);
		return 0;
	}

	printf("\n%s system off demo\n", CONFIG_BOARD);
	wakeup_io_src_get(); // fn to check wakeup source

	if (IS_ENABLED(CONFIG_APP_USE_RETAINED_MEM))
	{
		bool retained_ok = retained_validate();

		/* Increment for this boot attempt and update. */
		retained.boots += 1;
		retained_update();

		printf("Retained data: %s\n", retained_ok ? "valid" : "INVALID");
		printf("Boot count: %u\n", retained.boots);
		printf("Off count: %u\n", retained.off_count);
		printf("Active Ticks: %" PRIu64 "\n", retained.uptime_sum);
	}
	else
	{
		printf("Retained data not supported\n");
	}

#if IS_ENABLED(CONFIG_GRTC_WAKEUP_ENABLE)
	int err = z_nrf_grtc_wakeup_prepare(DEEP_SLEEP_TIME_S * USEC_PER_SEC);

	if (err < 0)
	{
		printk("Unable to prepare GRTC as a wake up source (err = %d).\n", err);
	}
	else
	{
		printk("Entering system off; wait %u seconds to restart\n", DEEP_SLEEP_TIME_S);
	}
#else
	/* configure sw0, 3 as input, interrupt as level active to allow wake-up */
	rc = init_io();

	printf("Entering system off; press any switch to restart\n");
#endif

	rc = pm_device_action_run(cons, PM_DEVICE_ACTION_SUSPEND);
	if (rc < 0)
	{
		printf("Could not suspend console (%d)\n", rc);
		return 0;
	}

	

	if (IS_ENABLED(CONFIG_APP_USE_RETAINED_MEM))
	{
		/* Update the retained state */
		retained.off_count += 1;
		retained_update();
	}

	sys_poweroff();

	return 0;
}
