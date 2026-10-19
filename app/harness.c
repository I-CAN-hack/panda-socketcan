/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * SBU detection and routing follow commaai/panda f849893b,
 * board/drivers/harness.h and board/boards/{black,red}.h.
 * The original panda copyright and MIT permission notice are in NOTICE.
 */

#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/can/transceiver.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/kernel.h>

#include "harness.h"

#define HARNESS DT_NODELABEL(panda_harness)
#define CAN2_NODE DT_PHANDLE(HARNESS, can_controller)
#define CAN2_PHY_NODE DT_PHANDLE(CAN2_NODE, phys)

PINCTRL_DT_DEFINE(HARNESS);
PINCTRL_DT_DEV_CONFIG_DECLARE(CAN2_NODE);

static const struct device *const adc = DEVICE_DT_GET(DT_IO_CHANNELS_CTLR_BY_IDX(HARNESS, 0));
static const uint8_t adc_channels[] = {
	DT_IO_CHANNELS_INPUT_BY_IDX(HARNESS, 0),
	DT_IO_CHANNELS_INPUT_BY_IDX(HARNESS, 1),
};
static const struct gpio_dt_spec sbu[] = {
	GPIO_DT_SPEC_GET_BY_IDX(HARNESS, sbu_gpios, 0),
	GPIO_DT_SPEC_GET_BY_IDX(HARNESS, sbu_gpios, 1),
};
static const struct gpio_dt_spec relays[] = {
	GPIO_DT_SPEC_GET_BY_IDX(HARNESS, relay_gpios, 0),
	GPIO_DT_SPEC_GET_BY_IDX(HARNESS, relay_gpios, 1),
};
static const struct gpio_dt_spec can2_standby[] = {
	GPIO_DT_SPEC_GET(CAN2_PHY_NODE, normal_standby_gpios),
	GPIO_DT_SPEC_GET(CAN2_PHY_NODE, flipped_standby_gpios),
};

static bool flipped;

static int read_orientation(bool *result)
{
	uint16_t samples[2];
	struct adc_sequence sequence = {
		.channels = BIT(adc_channels[0]) | BIT(adc_channels[1]),
		.buffer = samples,
		.buffer_size = sizeof(samples),
		.resolution = 12,
	};
	int err = 0;

	/* These pins lose their 5V tolerance in analog mode. Restore inputs after
	 * each reading, including on ADC failure, just like the factory firmware.
	 */
	for (size_t i = 0; i < ARRAY_SIZE(sbu); i++) {
		err = gpio_pin_configure_dt(&sbu[i], GPIO_DISCONNECTED);
		if (err != 0) {
			break;
		}
	}
	if (err == 0) {
		err = adc_read(adc, &sequence);
	}
	for (size_t i = 0; i < ARRAY_SIZE(sbu); i++) {
		int restore_err = gpio_pin_configure_dt(&sbu[i], GPIO_INPUT);

		if (err == 0) {
			err = restore_err;
		}
	}
	if (err == 0) {
		/* ADC results are in ascending channel order. The factory detection
		 * threshold is AVDD / 2, or 2048 for our 12-bit conversions.
		 */
		if (samples[0] >= 2048 && samples[1] >= 2048) {
			return -ENOTCONN;
		}
		*result = samples[0] < samples[1];
	}
	return err;
}

bool panda_harness_flipped(void)
{
	return flipped;
}

bool panda_harness_disconnected(void)
{
	static unsigned int absent;
	bool detected;

	/* Ignore orientation changes during a connection; only detect unplugging. */
	if (read_orientation(&detected) != -ENOTCONN) {
		absent = 0;
		return false;
	}
	return ++absent >= 3;
}

int panda_harness_init(void)
{
	BUILD_ASSERT(DT_SAME_NODE(DT_IO_CHANNELS_CTLR_BY_IDX(HARNESS, 0),
				  DT_IO_CHANNELS_CTLR_BY_IDX(HARNESS, 1)));
	BUILD_ASSERT(DT_IO_CHANNELS_INPUT_BY_IDX(HARNESS, 0) <
		     DT_IO_CHANNELS_INPUT_BY_IDX(HARNESS, 1));
	int err;

	/* Release both open-drain relay controls: leave the harness in passthrough. */
	for (size_t i = 0; i < ARRAY_SIZE(relays); i++) {
		if (!gpio_is_ready_dt(&relays[i]) || !gpio_is_ready_dt(&sbu[i])) {
			return -ENODEV;
		}
		err = gpio_pin_configure_dt(&relays[i], GPIO_OUTPUT_HIGH | GPIO_OPEN_DRAIN);
		if (err != 0) {
			return err;
		}
	}
	if (!device_is_ready(adc)) {
		return -ENODEV;
	}
	for (size_t i = 0; i < ARRAY_SIZE(adc_channels); i++) {
		struct adc_channel_cfg cfg = {
			.gain = ADC_GAIN_1,
			.reference = ADC_REF_INTERNAL,
			.acquisition_time = ADC_ACQ_TIME_MAX,
			.channel_id = adc_channels[i],
		};

		err = adc_channel_setup(adc, &cfg);
		if (err != 0) {
			return err;
		}
	}
	/* Wait for a harness before fixing the channel map. Require three matching
	 * orientations so partially inserted contacts don't select the routing.
	 */
	unsigned int stable = 0;

	while (stable < 3) {
		bool detected;

		k_msleep(100);
		err = read_orientation(&detected);
		if (err == -ENOTCONN) {
			stable = 0;
			continue;
		}
		if (err != 0) {
			return err;
		}
		stable = stable != 0 && detected == flipped ? stable + 1 : 1;
		flipped = detected;
	}

	/* Disconnect the unused RX/TX pair before selecting the other pair. */
	const struct device *port = DEVICE_DT_GET(DT_NODELABEL(gpiob));
	const uint8_t unused_pins[] = { flipped ? 5 : 12, flipped ? 6 : 13 };

	for (size_t i = 0; i < ARRAY_SIZE(unused_pins); i++) {
		err = gpio_pin_configure(port, unused_pins[i], GPIO_DISCONNECTED);
		if (err != 0) {
			return err;
		}
	}
	if (flipped) {
		struct pinctrl_dev_config *can2_pins = PINCTRL_DT_DEV_CONFIG_GET(CAN2_NODE);
		const struct pinctrl_dev_config *alt = PINCTRL_DT_DEV_CONFIG_GET(HARNESS);

		err = pinctrl_update_states(can2_pins, alt->states, alt->state_cnt);
		if (err != 0) {
			return err;
		}
		return pinctrl_apply_state(can2_pins, PINCTRL_STATE_DEFAULT);
	}
	return 0;
}

/* CAN2 uses one of two physical transceivers. Let the CAN driver control the
 * selected transceiver so it also enters standby on USB reset/disconnect.
 */
static int can2_enable(const struct device *dev, can_mode_t mode)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(mode);
	return gpio_pin_set_dt(&can2_standby[flipped], 0);
}

static int can2_disable(const struct device *dev)
{
	ARG_UNUSED(dev);
	int err = gpio_pin_set_dt(&can2_standby[0], 1);
	int other_err = gpio_pin_set_dt(&can2_standby[1], 1);

	return err != 0 ? err : other_err;
}

static int can2_phy_init(const struct device *dev)
{
	ARG_UNUSED(dev);
	for (size_t i = 0; i < ARRAY_SIZE(can2_standby); i++) {
		if (!gpio_is_ready_dt(&can2_standby[i])) {
			return -ENODEV;
		}
		int err = gpio_pin_configure_dt(&can2_standby[i], GPIO_OUTPUT_ACTIVE);

		if (err != 0) {
			return err;
		}
	}
	return 0;
}

static DEVICE_API(can_transceiver, can2_api) = {
	.enable = can2_enable,
	.disable = can2_disable,
};

DEVICE_DT_DEFINE(CAN2_PHY_NODE, can2_phy_init, NULL, NULL, NULL,
		 POST_KERNEL, CONFIG_CAN_TRANSCEIVER_INIT_PRIORITY, &can2_api);
