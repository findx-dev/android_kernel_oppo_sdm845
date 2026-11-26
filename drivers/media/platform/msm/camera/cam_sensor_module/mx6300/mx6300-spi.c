#include <linux/kernel.h>
#include <linux/module.h>

#include "mx6300-spi.h"
#include "mx6300_tac/mx6300_tac.h"

static uint8_t mx6300_output_mode;

#define MX6300_STREAM_MODE_SIZE 6

static const face_ta_mx_driver_stream_data_t stream_mode_config[] = {
	{
		.width = 1280,
		.height = 800,
		.type = MX6300_STREAM_TYPE_IR_IMAGE,
	},
	{
		.width = 640,
		.height = 400,
		.type = MX6300_STREAM_TYPE_IR_IMAGE,
	},
	{
		.width = 800,
		.height = 1280,
		.type = MX6300_STREAM_TYPE_DEPTH_IMAGE,
	},
	{
		.width = 400,
		.height = 640,
		.type = MX6300_STREAM_TYPE_DEPTH_IMAGE,
	},
	{
		.width = 1280,
		.height = 800,
		.type = MX6300_STREAM_TYPE_IR_PATTERN,
	},
	{
		.width = 640,
		.height = 400,
		.type = MX6300_STREAM_TYPE_IR_PATTERN,
	},
};

int cam_sensor_ctl_mxmodule(int cmd, int parm)
{
	int ret = 0;

	pr_debug("%s: cmd %d, parm %d\n", __func__, cmd, parm);

	switch (cmd) {
	case MX_MSG_START:
		if (mx6300_output_mode >= ARRAY_SIZE(stream_mode_config))
			mx6300_output_mode = 0;
		ret = mx6300_tac_start_stream(
			stream_mode_config[mx6300_output_mode].width,
			stream_mode_config[mx6300_output_mode].height,
			stream_mode_config[mx6300_output_mode].type);
		break;
	case MX_MSG_STOP:
		ret = mx6300_tac_stop_stream();
		break;
	case MX_MSG_CONFIG:
		if (parm >= 0 && parm < MX6300_STREAM_MODE_SIZE)
			mx6300_output_mode = parm;
		break;
	case MX_MSG_INIT:
	case MX_MSG_PROBE:
		break;
	default:
		pr_debug("%s: unknown command %d\n", __func__, cmd);
		break;
	}

	return ret;
}

MODULE_AUTHOR("wangdeyong");
MODULE_DESCRIPTION("mx6300 SPI device interface");
MODULE_LICENSE("GPL");
MODULE_ALIAS("spi-mx6300");
