// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022-2099, Nubia ltd. All rights reserved.
 */

#ifndef USB_SWITCH_DP_H
#define USB_SWITCH_DP_H

#include <linux/of.h>
#include <linux/notifier.h>

enum switch_function {
	SWITCH_MIC_GND_SWAP,
	SWITCH_USBC_ORIENTATION_CC1,
	SWITCH_USBC_ORIENTATION_CC2,
	SWITCH_USBC_DISPLAYPORT_DISCONNECTED,
	SWITCH_EVENT_MAX,
};

int dp_switch_event(struct device_node *node,
		enum switch_function event);
#endif
