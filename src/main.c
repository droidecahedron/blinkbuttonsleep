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

#include <hal/nrf_gpio.h>
#include <hal/nrf_power.h>
#include <hal/nrf_reset.h>
#include <zephyr/pm/pm.h>

#if IS_ENABLED(CONFIG_GRTC_WAKEUP_ENABLE)
#include <zephyr/drivers/timer/nrf_grtc_timer.h>
#define DEEP_SLEEP_TIME_S 2
#else
#define WAKE_BUTTON_NODE DT_ALIAS(sw1)
#define SLEEP_BUTTON_NODE DT_ALIAS(sw0)
static const struct gpio_dt_spec sleepbutton0 = GPIO_DT_SPEC_GET_OR(SLEEP_BUTTON_NODE, gpios, {0});
static const struct gpio_dt_spec wakebutton1 = GPIO_DT_SPEC_GET_OR(WAKE_BUTTON_NODE, gpios, {0});
static struct gpio_callback button_cb_data;

//5340 button bits
#define dk_sw1_msk 1 << 23
#define dk_sw2_msk 1 << 24
#define dk_sw3_msk 1 << 8
#define dk_sw4_msk 1 << 9

#endif

static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
const struct device *const cons = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

#define WORQ_THREAD_STACK_SIZE 512
#define WQ_PRIO 4

// Define stack area used by workqueue thread
static K_THREAD_STACK_DEFINE(wq_stack_area, WORQ_THREAD_STACK_SIZE);
// Define queue structure
static struct k_work_q sleep_work_q = {0};
struct work_info
{
    struct k_work work;
    uint8_t data[8];
} my_work;

void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    k_work_submit_to_queue(&sleep_work_q, &my_work.work);
}

void poweroff_work_handler(struct k_work *work_item)
{
    printf("configuring sw1 as wakebutton then going to bed\n");
    // Configure pin sense for wakeup
    gpio_pin_configure_dt(&wakebutton1, GPIO_INPUT | GPIO_PULL_UP);
    gpio_add_callback(wakebutton1.port, &button_cb_data);
    gpio_pin_interrupt_configure_dt(&wakebutton1, GPIO_INT_EDGE_TO_ACTIVE);
    nrf_gpio_cfg_sense_set(wakebutton1.pin, NRF_GPIO_PIN_SENSE_LOW);

    int rc = pm_device_action_run(cons, PM_DEVICE_ACTION_SUSPEND);
    if (rc < 0)
    {
        printf("Could not suspend console (%d)\n", rc);
    }

    // Enter system off (deep sleep)
    sys_poweroff();
}

static void sleep_button_init(void)
{
    gpio_pin_configure_dt(&sleepbutton0, GPIO_INPUT | GPIO_PULL_UP);
    gpio_init_callback(&button_cb_data, button_pressed, BIT(sleepbutton0.pin) | BIT(wakebutton1.pin));
    gpio_add_callback(sleepbutton0.port, &button_cb_data);
    gpio_pin_interrupt_configure_dt(&sleepbutton0, GPIO_INT_EDGE_TO_ACTIVE);
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
    // check and reset latch registers. on 5340 all buttons are on p0.
    // since these buttons are across ports, need to logic around both port latch registers
    volatile uint32_t p0_latch = NRF_P0_S->LATCH;
    printf("LATCH REGISTER FOR P0: %d\n", p0_latch);

    // Your logic here will change depending on your device and ports.
    if (p0_latch > 0) // check if p0 was the source
    {
        switch (p0_latch)
        {
        case dk_sw2_msk:
            printk("WAKEUP SRC: SW2\n");
            blink(2);
            break;
        default:
            printk("WAKEUP SRC: UNDEF\n");
            break;
        }
    }
    else
    {
        // unknown wakeup source
    }

    // clear latch (write 1 to clear)
    NRF_P0_S->LATCH = NRF_P0_S->LATCH;
}

int main(void)
{
    if (!device_is_ready(cons))
    {
        printf("%s: device not ready.\n", cons->name);
        return 0;
    }

    printf("\n%s system off demo\n", CONFIG_BOARD);
    wakeup_io_src_get();

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
    /* configure sw3 as input, interrupt as level active to allow wake-up */
    sleep_button_init();

    k_work_init(&my_work.work, poweroff_work_handler);
    strcpy(my_work.data, "sleep");
    k_work_queue_start(&sleep_work_q, wq_stack_area, K_THREAD_STACK_SIZEOF(wq_stack_area), WQ_PRIO, NULL);

    printf("Entering system off; button1 sleeps, button2 to wake up\n");
#endif

    if (IS_ENABLED(CONFIG_APP_USE_RETAINED_MEM))
    {
        /* Update the retained state */
        retained.off_count += 1;
        retained_update();
    }

    while (1)
    {
        k_sleep(K_FOREVER);
    }

    return 0;
}
