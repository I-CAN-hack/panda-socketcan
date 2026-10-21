/*
 * Copyright (c) 2024 Henrik Brix Andersen <henrik@brixandersen.dk>
 * Copyright (c) 2026 Willem Melching
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Adapted from CANnectivity app/src/main.c for panda harness support.
 */

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/usb/usbd.h>
#include <cannectivity/usb/class/gs_usb.h>

#include "cannectivity.h"
#include "harness.h"

#define CHANNEL_CONTROLLER(node) DEVICE_DT_GET(DT_PHANDLE(node, can_controller))

#ifdef CONFIG_CAN_STM32_BXCAN
#include <soc.h>

#define CHANNEL_BXCAN(node) ((CAN_TypeDef *)DT_REG_ADDR(DT_PHANDLE(node, can_controller)))

static int panda_can_tx_order_init(void)
{
	const struct device *const devices[] = {
		CANNECTIVITY_DT_FOREACH_CHANNEL_SEP(CHANNEL_CONTROLLER, (,))
	};
	CAN_TypeDef *const controllers[] = {
		CANNECTIVITY_DT_FOREACH_CHANNEL_SEP(CHANNEL_BXCAN, (,))
	};

	/* Zephyr leaves stopped bxCAN controllers in initialization mode. Its
	 * driver disables TXFP and reuses the lowest free mailbox, allowing newer
	 * frames with the same CAN ID to overtake older ones under load. Select
	 * request-order priority before gs_usb can start any of the channels.
	 */
	for (size_t i = 0; i < ARRAY_SIZE(controllers); i++) {
		if (!device_is_ready(devices[i])) {
			return -ENODEV;
		}
		if ((controllers[i]->MSR & CAN_MSR_INAK) == 0U) {
			return -EBUSY;
		}
		controllers[i]->MCR |= CAN_MCR_TXFP;
	}
	return 0;
}
#endif

static const struct gs_usb_ops usb_ops = {
	.timestamp = cannectivity_timestamp_get,
	.event = cannectivity_led_event,
};

int main(void)
{
	const struct device *usb = DEVICE_DT_GET(DT_NODELABEL(gs_usb0));
	const struct device *channels[] = {
		CANNECTIVITY_DT_FOREACH_CHANNEL_SEP(CHANNEL_CONTROLLER, (,))
	};
	int err;

	if (!device_is_ready(usb)) {
		return -ENODEV;
	}

#if DT_HAS_COMPAT_STATUS_OKAY(comma_panda_harness)
	err = panda_harness_init();
	if (err != 0) {
		return err;
	}
	if (panda_harness_flipped()) {
		const struct device *tmp = channels[0];

		channels[0] = channels[2];
		channels[2] = tmp;
	}
#endif

	err = cannectivity_led_init();
	if (err != 0) {
		return err;
	}
	err = cannectivity_timestamp_init();
	if (err != 0) {
		return err;
	}
#ifdef CONFIG_CAN_STM32_BXCAN
	err = panda_can_tx_order_init();
	if (err != 0) {
		return err;
	}
#endif

	err = gs_usb_register(usb, channels, ARRAY_SIZE(channels), &usb_ops, NULL);
	if (err != 0) {
		return err;
	}
	err = cannectivity_usb_init();
	if (err != 0) {
		return err;
	}

#if DT_HAS_COMPAT_STATUS_OKAY(comma_panda_harness)
	while (true) {
		k_msleep(100);
		if (panda_harness_disconnected()) {
			/* Disabling USB also stops all CAN channels and transceivers. */
			STRUCT_SECTION_FOREACH(usbd_context, context) {
				(void)usbd_disable(context);
			}
			/* Start the next connection with fresh CAN filters, bit timings
			 * and channel registration. Boot waits for a harness again.
			 */
			sys_reboot(SYS_REBOOT_COLD);
		}
	}
#endif
	return 0;
}
