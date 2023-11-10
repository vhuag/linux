// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2020 Synaptics Incorporated
 */

#include <linux/kernel.h>
#include <linux/rmi.h>
#include <linux/input.h>
#include <linux/slab.h>
#include "rmi_driver.h"

#define RMI_F21_FORCE_CLICK_OFFSET	8
#define RMI_f21_FORCE_CLICK		0x01
#define RMI_f21_DATA_REGS_MAX_SIZE	1
#define RMI_f21_FORCEPAD_BUTTON_COUNT	1

struct f21_data {
	/* Query Data */
	u8 data_regs[RMI_f21_DATA_REGS_MAX_SIZE];
	struct input_dev *input;
	u16 key_code;
};

static irqreturn_t rmi_f21_attention(int irq, void *ctx)
{
	struct rmi_function *fn = ctx;
	struct f21_data *f21 = dev_get_drvdata(&fn->dev);
	int error;

	error = rmi_read_block(fn->rmi_dev,
				fn->fd.data_base_addr+RMI_F21_FORCE_CLICK_OFFSET,
				f21->data_regs, 1);
	if (error) {
		dev_err(&fn->dev,
			"%s: Failed to read f21 data registers: %d\n",
			__func__, error);
		return IRQ_RETVAL(error);
	}

	if ((f21->data_regs[0] & RMI_f21_FORCE_CLICK))
		input_report_key(f21->input, f21->key_code, true);
	else
		input_report_key(f21->input, f21->key_code, false);
	return IRQ_HANDLED;
}

static int rmi_f21_config(struct rmi_function *fn)
{
	struct f21_data *f21 = dev_get_drvdata(&fn->dev);
	struct rmi_driver *drv = fn->rmi_dev->driver;

	if (!f21)
		return 0;

	drv->set_irq_bits(fn->rmi_dev, fn->irq_mask);

	return 0;
}

static int rmi_f21_initialize(struct rmi_function *fn, struct f21_data *f21)
{
	struct input_dev *input = f21->input;
	unsigned int button = BTN_LEFT;

	f21->key_code = button;
	input_set_capability(input, EV_KEY, f21->key_code);
	input->keycode = &(f21->key_code);
	input->keycodesize = sizeof(f21->key_code);
	input->keycodemax = RMI_f21_FORCEPAD_BUTTON_COUNT;

	__set_bit(INPUT_PROP_BUTTONPAD, input->propbit);

	return 0;
}

static int rmi_f21_probe(struct rmi_function *fn)
{
	struct rmi_device *rmi_dev = fn->rmi_dev;
	struct rmi_driver_data *drv_data = dev_get_drvdata(&rmi_dev->dev);
	struct f21_data *f21;
	int error;

	if (!drv_data->input) {
		dev_info(&fn->dev, "f21: no input device found, ignoring\n");
		return -ENXIO;
	}

	f21 = devm_kzalloc(&fn->dev, sizeof(*f21), GFP_KERNEL);
	if (!f21)
		return -ENOMEM;

	f21->input = drv_data->input;

	error = rmi_f21_initialize(fn, f21);
	if (error)
		return error;

	dev_set_drvdata(&fn->dev, f21);
	return 0;
}

struct rmi_function_handler rmi_f21_handler = {
	.driver = {
		.name = "rmi4_f21",
	},
	.func = 0x21,
	.probe = rmi_f21_probe,
	.config = rmi_f21_config,
	.attention = rmi_f21_attention,
};
