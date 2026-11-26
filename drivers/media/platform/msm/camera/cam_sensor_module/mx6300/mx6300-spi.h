#ifndef __MX6300_SPI_H
#define __MX6300_SPI_H

enum mx_msg_type {
	MX_MSG_INIT,
	MX_MSG_START,
	MX_MSG_STOP,
	MX_MSG_CONFIG,
	MX_MSG_PROBE,
	MX_MSG_MAX
};

int cam_sensor_ctl_mxmodule(int cmd, int parm);

#endif
