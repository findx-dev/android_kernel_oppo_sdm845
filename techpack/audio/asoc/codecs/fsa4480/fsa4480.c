/*
 * FSA4480 USB Type-C analog audio switch
 *
 * Copyright (C) 2018-2020 OPPO Mobile Comm Corp., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>

#define FSA4480_DEVID		0x00
#define FSA4480_OVPMSK		0x01
#define FSA4480_SWEN		0x04
#define FSA4480_SWSEL		0x05
#define FSA4480_FUN_EN		0x12
#define FSA4480_RES_SET		0x13
#define FSA4480_RES_VAL		0x14
#define FSA4480_JACTYP		0x17
#define FSA4480_DECINT		0x18
#define FSA4480_AUDDATA0	0x1c
#define FSA4480_AUDDATA1	0x1d
#define FSA4480_CURSET		0x1f

#define FSA4480_DEV_VERSION	0x09
#define FSA4480_I2C_RETRIES	5
#define FSA4480_I2C_RETRY_MS	5

enum fsa4480_jack_type {
	FSA4480_JACK_INVALID = -1,
	FSA4480_JACK_NO_ACCESSORY,
	FSA4480_JACK_HEADSET,
	FSA4480_JACK_HEADPHONES,
	FSA4480_JACK_HEADSET_SBU1 = 4,
	FSA4480_JACK_HEADSET_SBU2 = 8,
	FSA4480_JACK_SELFIE_STICK = 6,
	FSA4480_JACK_HIGH_RES = 7,
};

struct fsa4480 {
	struct i2c_client *client;
	struct mutex io_lock;
	u8 jack_status;
};

static struct fsa4480 *fsa4480;

static int fsa4480_i2c_read(struct fsa4480 *priv, u8 reg, u8 *value)
{
	struct i2c_msg msgs[2];
	int ret;
	int retries;

	msgs[0].addr = priv->client->addr;
	msgs[0].flags = 0;
	msgs[0].len = 1;
	msgs[0].buf = &reg;
	msgs[1].addr = priv->client->addr;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len = 1;
	msgs[1].buf = value;

	for (retries = 0; retries < FSA4480_I2C_RETRIES; retries++) {
		ret = i2c_transfer(priv->client->adapter, msgs, 2);
		if (ret == 2)
			return 0;
		usleep_range(FSA4480_I2C_RETRY_MS * 1000,
			     FSA4480_I2C_RETRY_MS * 1000 + 100);
	}

	dev_err(&priv->client->dev, "failed to read register 0x%02x: %d\n",
		reg, ret);
	return ret < 0 ? ret : -EIO;
}

static int fsa4480_i2c_write(struct fsa4480 *priv, u8 reg, u8 value)
{
	u8 buf[2] = { reg, value };
	struct i2c_msg msg;
	int ret;
	int retries;

	msg.addr = priv->client->addr;
	msg.flags = 0;
	msg.len = sizeof(buf);
	msg.buf = buf;

	for (retries = 0; retries < FSA4480_I2C_RETRIES; retries++) {
		ret = i2c_transfer(priv->client->adapter, &msg, 1);
		if (ret == 1)
			return 0;
		usleep_range(FSA4480_I2C_RETRY_MS * 1000,
			     FSA4480_I2C_RETRY_MS * 1000 + 100);
	}

	dev_err(&priv->client->dev, "failed to write register 0x%02x: %d\n",
		reg, ret);
	return ret < 0 ? ret : -EIO;
}

static int fsa4480_park_usb(struct fsa4480 *priv)
{
	int ret;

	ret = fsa4480_i2c_write(priv, FSA4480_SWEN, 0x98);
	if (ret)
		return ret;

	ret = fsa4480_i2c_write(priv, FSA4480_SWSEL, 0x18);
	priv->jack_status = FSA4480_JACK_NO_ACCESSORY;
	return ret;
}

static int fsa4480_switch_mic(struct fsa4480 *priv, int path)
{
	int ret;

	ret = fsa4480_i2c_write(priv, FSA4480_SWEN, 0x9b);
	if (ret)
		return ret;

	if (path == 1)
		return fsa4480_i2c_write(priv, FSA4480_SWSEL, 0x07);

	return fsa4480_i2c_write(priv, FSA4480_SWSEL, 0x00);
}

static int fsa4480_prepare_detection(struct fsa4480 *priv)
{
	u8 dec_int;
	int ret;

	ret = fsa4480_i2c_write(priv, FSA4480_SWEN, 0x98);
	if (ret)
		return ret;
	ret = fsa4480_i2c_write(priv, FSA4480_SWSEL, 0x00);
	if (ret)
		return ret;

	mdelay(10);
	ret = fsa4480_i2c_write(priv, FSA4480_SWEN, 0x9b);
	if (ret)
		return ret;
	mdelay(10);

	ret = fsa4480_i2c_write(priv, FSA4480_FUN_EN, 0x45);
	if (ret)
		return ret;
	mdelay(8);

	ret = fsa4480_i2c_read(priv, FSA4480_DECINT, &dec_int);
	if (ret)
		return ret;

	return (dec_int & 0x04) ? 0 : -ENODEV;
}

static int fsa4480_reset_audio_path(struct fsa4480 *priv)
{
	int ret;

	ret = fsa4480_i2c_write(priv, FSA4480_SWEN, 0x00);
	if (ret)
		return ret;
	mdelay(20);

	ret = fsa4480_i2c_write(priv, FSA4480_SWEN, 0x98);
	if (ret)
		return ret;
	ret = fsa4480_i2c_write(priv, FSA4480_SWSEL, 0x00);
	if (ret)
		return ret;
	mdelay(10);

	return fsa4480_i2c_write(priv, FSA4480_SWEN, 0x9f);
}

static int fsa4480_detect_accessory(struct fsa4480 *priv, u8 jack_status)
{
	u8 resistance[5];
	int ret;
	int i;

	for (i = 0; i < ARRAY_SIZE(resistance); i++) {
		ret = fsa4480_i2c_write(priv, FSA4480_RES_SET, i);
		if (ret)
			return ret;
		ret = fsa4480_i2c_write(priv, FSA4480_FUN_EN, 0x46);
		if (ret)
			return ret;
		mdelay(10);
		ret = fsa4480_i2c_read(priv, FSA4480_RES_VAL,
				       &resistance[i]);
		if (ret)
			return ret;
	}

	switch (jack_status) {
	case FSA4480_JACK_HEADSET_SBU1:
		if (resistance[1] == 0xff && resistance[2] == 0xff)
			return resistance[3] == 0xff ?
				FSA4480_JACK_HIGH_RES :
				FSA4480_JACK_SELFIE_STICK;
		return FSA4480_JACK_HEADSET;
	case FSA4480_JACK_HEADSET_SBU2:
		if (resistance[1] == 0xff && resistance[2] == 0xff)
			return resistance[4] == 0xff ?
				FSA4480_JACK_HIGH_RES :
				FSA4480_JACK_SELFIE_STICK;
		return FSA4480_JACK_HEADSET;
	case FSA4480_JACK_HEADPHONES:
		if (resistance[3] != 0xff && resistance[4] != 0xff)
			return FSA4480_JACK_HEADPHONES;
		return FSA4480_JACK_INVALID;
	default:
		return FSA4480_JACK_INVALID;
	}
}

int fsa4480_enable_audio_switch(void)
{
	struct fsa4480 *priv = fsa4480;
	u8 jack_status = 0;
	int jack_type = FSA4480_JACK_INVALID;
	int ret;
	int i;

	if (!priv)
		return -EPROBE_DEFER;

	mutex_lock(&priv->io_lock);

	for (i = 0; i < 3; i++) {
		ret = fsa4480_prepare_detection(priv);
		if (!ret)
			break;
	}
	if (ret) {
		fsa4480_park_usb(priv);
		goto unlock;
	}

	ret = fsa4480_i2c_read(priv, FSA4480_JACTYP, &jack_status);
	if (ret)
		goto park;
	jack_status &= 0x0f;
	priv->jack_status = jack_status;

	ret = fsa4480_reset_audio_path(priv);
	if (ret)
		goto park;

	ret = fsa4480_switch_mic(priv,
			jack_status == FSA4480_JACK_HEADSET_SBU1 ? 1 : 2);
	if (ret)
		goto park;
	mdelay(10);

	jack_type = fsa4480_detect_accessory(priv, jack_status);
	if (jack_type < 0)
		goto park;

	/*
	 * Resistance detection temporarily repurposes the switches. Restore
	 * the audio path before returning control to MBHC.
	 */
	ret = fsa4480_reset_audio_path(priv);
	if (ret)
		goto park;
	ret = fsa4480_switch_mic(priv,
			jack_status == FSA4480_JACK_HEADSET_SBU1 ? 1 : 2);
	if (ret)
		goto park;
	mdelay(10);

	goto unlock;

park:
	fsa4480_park_usb(priv);
	if (ret)
		jack_type = ret;
unlock:
	mutex_unlock(&priv->io_lock);
	return jack_type;
}
EXPORT_SYMBOL_GPL(fsa4480_enable_audio_switch);

int fsa4480_disable_audio_switch(void)
{
	struct fsa4480 *priv = fsa4480;
	int ret;

	if (!priv)
		return -EPROBE_DEFER;

	mutex_lock(&priv->io_lock);
	ret = fsa4480_park_usb(priv);
	mutex_unlock(&priv->io_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(fsa4480_disable_audio_switch);

bool fsa4480_switch_check_plug_in(void)
{
	struct fsa4480 *priv = fsa4480;
	int jack_type;

	if (!priv)
		return false;

	mutex_lock(&priv->io_lock);
	jack_type = fsa4480_detect_accessory(priv, priv->jack_status);
	mutex_unlock(&priv->io_lock);

	return jack_type >= 0 &&
	       jack_type != FSA4480_JACK_HIGH_RES &&
	       jack_type != FSA4480_JACK_SELFIE_STICK;
}
EXPORT_SYMBOL_GPL(fsa4480_switch_check_plug_in);

static int fsa4480_init_registers(struct fsa4480 *priv)
{
	u8 device_id;
	int ret;

	ret = fsa4480_i2c_read(priv, FSA4480_DEVID, &device_id);
	if (ret)
		return ret;
	if (device_id != FSA4480_DEV_VERSION) {
		dev_err(&priv->client->dev,
			"unexpected device ID 0x%02x\n", device_id);
		return -ENODEV;
	}

	ret = fsa4480_i2c_write(priv, FSA4480_OVPMSK, 0x00);
	if (ret)
		return ret;
	ret = fsa4480_i2c_write(priv, FSA4480_AUDDATA0, 0x08);
	if (ret)
		return ret;
	ret = fsa4480_i2c_write(priv, FSA4480_AUDDATA1, 0xff);
	if (ret)
		return ret;
	ret = fsa4480_i2c_write(priv, FSA4480_FUN_EN, 0x44);
	if (ret)
		return ret;
	ret = fsa4480_i2c_write(priv, FSA4480_SWEN, 0x80);
	if (ret)
		return ret;
	ret = fsa4480_i2c_write(priv, FSA4480_CURSET, 0x02);
	if (ret)
		return ret;

	return fsa4480_park_usb(priv);
}

static int fsa4480_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct fsa4480 *priv;
	int ret;

	priv = devm_kzalloc(&client->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->client = client;
	mutex_init(&priv->io_lock);
	i2c_set_clientdata(client, priv);

	ret = fsa4480_init_registers(priv);
	if (ret)
		return ret;

	fsa4480 = priv;
	return 0;
}

static int fsa4480_remove(struct i2c_client *client)
{
	fsa4480_disable_audio_switch();
	fsa4480 = NULL;
	return 0;
}

static const struct i2c_device_id fsa4480_id[] = {
	{ "fsa4480", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, fsa4480_id);

static const struct of_device_id fsa4480_match[] = {
	{ .compatible = "fsa,fsa4480" },
	{ }
};
MODULE_DEVICE_TABLE(of, fsa4480_match);

static struct i2c_driver fsa4480_driver = {
	.driver = {
		.name = "fsa4480",
		.of_match_table = fsa4480_match,
	},
	.probe = fsa4480_probe,
	.remove = fsa4480_remove,
	.id_table = fsa4480_id,
};
module_i2c_driver(fsa4480_driver);

MODULE_DESCRIPTION("FSA4480 USB Type-C analog audio switch");
MODULE_LICENSE("GPL v2");
