// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022-2099, Nubia ltd. All rights reserved.
 */

#ifndef _NUBIA_DP_PREFERENCE_
#define _NUBIA_DP_PREFERENCE_

#include "../dp/dp_debug.h"
#include "../dp/dp_hpd.h"

#define RES_8K_H 7680 // 8K
#define RES_8K_V 4320
#define UHD_H 3840 // 4K
#define UHD_V 2160
#define QHD_H 2560 // 2K
#define QHD_V 1440
#define FHD_H 1920
#define FHD_V 1080
#define HD_H  1280
#define HD_V  720

// monitors edid
#define GC553          "AVT GC553"
#define GC573          "AVT GC573"
#define TITAN_ARMY     "C35SQPLUS"
#define AOC_MONITOR    "27G2G4"
#define MI_MONITOR     "Mi 245 HF2"
#define MI_TV          "Mi TVPQ"
#define REDMAGIC_GM003 "M27N2S"
#define REDMAGIC_GM001 "27A6V"

// cables vendor ID
#define STIGER_HDMI  "0x20030001"
#define PHILIPS_HDMI "0x00000115"
#define PISEN_HDMI   "0x01030001"
#define UNI_HDMI     "0x01030001"

#define FILTERED_MODE(nm, vdo, h, v, vf, rto) \
	.name = nm, .dp_productvdo = vdo, .hdisplay = h, \
	.vdisplay = v, .vrefresh = vf,.aspect_ratio = rto

struct selected_edid_mode {
	u16 hdisplay;
	u16 vdisplay;
	u16 vrefresh;
	u8 aspect_ratio;
};

struct user_edid_mode {
	u8 name[20];
	u8 vdo[11];
	u16 hdisplay;
	u16 vdisplay;
	u16 vrefresh;
	u8 aspect_ratio;
	struct list_head list;
};

struct edid_control {
	u8 name[20];
	u8 dp_productvdo[11];
	u8 *edid_modes; // all modes
	bool cable_connected;
	bool simulate_hpd;
	struct selected_edid_mode *sel_mode; // selected mode setting
	struct list_head user_modes; // user connected modes
};

struct filtered_mode {
	u8 name[20];
	u8 dp_productvdo[11];
	u16 hdisplay;
	u16 vdisplay;
	u16 vrefresh;
	u8 aspect_ratio;
};

void nubia_edid_modes(struct drm_connector *connector);
void nubia_dp_preference(void);

#define EDID_MODES_SIZE SZ_2K   //+linx 0016005364 20230302 for apk anr because overflow

#endif
