/************************************************************************************
** Copyright (C), 2008-2018, OPPO Mobile Comm Corp., Ltd
** VENDOR_EDIT
** File: oppo_proximity_power.h
**
** Description:
**      Definitions for the OPPO proximity power control.
**************************************************************************************/
#ifndef __OPPO_PROXIMITY_POWER_H__
#define __OPPO_PROXIMITY_POWER_H__

struct oppo_proximity_chip {
	struct device *dev;
	struct pinctrl *pctrl;
	struct pinctrl_state *power_low;
	struct pinctrl_state *power_high;
	unsigned int power_gpio;
};

#endif /* __OPPO_PROXIMITY_POWER_H__ */
