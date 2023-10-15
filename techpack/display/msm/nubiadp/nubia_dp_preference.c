// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022-2099, Nubia ltd. All rights reserved.
 */

#include "nubia_dp_preference.h"

struct dp_debug_private *dp_debug_p = NULL;
struct kobject *nubia_global_enhace_kobj = NULL;
struct edid_control *edid_ctl = NULL;

static struct filtered_mode filtered_modes[] = {
	{ FILTERED_MODE(MI_MONITOR, "", 3840, 2160, 60, 0) },
	{ FILTERED_MODE(MI_MONITOR, "", 1920, 1440, 120, 0) },
	{ FILTERED_MODE(MI_MONITOR, PHILIPS_HDMI, 1920, 1080, 165, 0) },
	{ FILTERED_MODE(AOC_MONITOR, "", 1920, 1080, 144, 0) },
	{ FILTERED_MODE(TITAN_ARMY, PISEN_HDMI, 3440, 1440, 100, 0) },
	{ FILTERED_MODE(GC553, "", 1920, 1080, 144, 0) },
	{ FILTERED_MODE(GC573, "", 1920, 1080, 144, 0) },
};

static void trim_monitor_string(char *src, char *dst)
{
	u8 p[128], q[20], i = 0, j = 0;

	strncpy(p, src, 127);
	p[127] = '\0';
	while (p[i] != '\0') {
		if ((p[i] >= '0' && p[i] <= '9') ||
				(p[i] >= 'a' && p[i] <= 'z') ||
				(p[i] >= 'A' && p[i] <= 'Z') ||
				(p[i] == ' ')) {
			q[j] = p[i];
			j++;
		}
		i++;
		if (j >= sizeof(q) - 1) {
			DP_WARN(": monitor_string full\n");
			break;
		}
	}
	q[j] = '\0';
	strcpy(dst, q);
}

static void get_higher_refresh_resolution(u16 hdisplay, u16 vdisplay,
		u16 vrefresh, u8 picture_aspect_ratio, bool ratio_16_9)
{
	if (ratio_16_9) {
		if (hdisplay * 9 != vdisplay * 16)
			goto non_selected;
	}
	if (vrefresh < edid_ctl->sel_mode->vrefresh) {
		goto non_selected;
	} else if (vrefresh == edid_ctl->sel_mode->vrefresh) {
		if (hdisplay < edid_ctl->sel_mode->hdisplay ||
			vdisplay < edid_ctl->sel_mode->vdisplay) {
			goto non_selected;
		}
		else if (hdisplay == edid_ctl->sel_mode->hdisplay &&
			vdisplay == edid_ctl->sel_mode->vdisplay) {
			if (picture_aspect_ratio <=
				edid_ctl->sel_mode->aspect_ratio)
				goto non_selected;
		}
	}
	edid_ctl->sel_mode->hdisplay = hdisplay;
	edid_ctl->sel_mode->vdisplay = vdisplay;
	edid_ctl->sel_mode->vrefresh = vrefresh;
	edid_ctl->sel_mode->aspect_ratio = picture_aspect_ratio;
	DP_INFO(": preselected_edid_mode: %dx%d %d %d\n",
			edid_ctl->sel_mode->hdisplay,
			edid_ctl->sel_mode->vdisplay,
			edid_ctl->sel_mode->vrefresh,
			edid_ctl->sel_mode->aspect_ratio);
non_selected:
	return;
}

static bool filter_edid_modes(u16 hdisplay, u16 vdisplay, u16 vrefresh)
{
	u8 i, len = 0;

	len = sizeof(filtered_modes) / sizeof(filtered_modes[0]);
	for (i = 0; i < len; i++) {
		if (strlen(filtered_modes[i].name)) {
			if (strcmp(edid_ctl->name, filtered_modes[i].name))
				continue;
		}
		if (strlen(filtered_modes[i].dp_productvdo)) {
			if (strcmp(edid_ctl->dp_productvdo,
					filtered_modes[i].dp_productvdo))
				continue;
		}
		if (filtered_modes[i].hdisplay) {
			if (hdisplay != filtered_modes[i].hdisplay)
				continue;
		}
		if (filtered_modes[i].vdisplay) {
			if (vdisplay != filtered_modes[i].vdisplay)
				continue;
		}
		if (filtered_modes[i].vrefresh) {
			if (vrefresh != filtered_modes[i].vrefresh)
				continue;
		}
		DP_INFO(": name = %s, vdo = %s, %dx%d %d %d\n",
			filtered_modes[i].name,
			filtered_modes[i].dp_productvdo,
			filtered_modes[i].hdisplay,
			filtered_modes[i].vdisplay,
			filtered_modes[i].vrefresh,
			filtered_modes[i].aspect_ratio);
		return true;
	}
	return false;
}

static void store_user_edid_modes(u16 hdisplay, u16 vdisplay,
		u16 vrefresh, u8 picture_aspect_ratio)
{
	struct user_edid_mode *mode;

	list_for_each_entry (mode, &edid_ctl->user_modes, list) {
		if (!strcmp(mode->name, edid_ctl->name) &&
			!strcmp(mode->vdo, edid_ctl->dp_productvdo)) {
			mode->hdisplay = hdisplay;
			mode->vdisplay = vdisplay;
			mode->vrefresh = vrefresh;
			mode->aspect_ratio = picture_aspect_ratio;
			DP_INFO(": %s %s %dx%d %d %d replaced\n", mode->name,
				mode->vdo, mode->hdisplay, mode->vdisplay,
				mode->vrefresh, mode->aspect_ratio);
			return;
		}
	}
	mode = kzalloc(sizeof(struct user_edid_mode), GFP_KERNEL);
	strcpy(mode->name, edid_ctl->name);
	strcpy(mode->vdo, edid_ctl->dp_productvdo);
	mode->hdisplay = hdisplay;
	mode->vdisplay = vdisplay;
	mode->vrefresh = vrefresh;
	mode->aspect_ratio = picture_aspect_ratio;
	list_add_tail(&mode->list, &edid_ctl->user_modes);
	DP_INFO(": %s %s %dx%d %d %d stored\n", mode->name,
		mode->vdo, mode->hdisplay, mode->vdisplay,
		mode->vrefresh, mode->aspect_ratio);
}

static bool match_user_edid_modes(void)
{
	struct user_edid_mode *mode;

	list_for_each_entry (mode, &edid_ctl->user_modes, list) {
		if (!strcmp(mode->name, edid_ctl->name) &&
			!strcmp(mode->vdo, edid_ctl->dp_productvdo)) {
			edid_ctl->sel_mode->hdisplay = mode->hdisplay;
			edid_ctl->sel_mode->vdisplay = mode->vdisplay;
			edid_ctl->sel_mode->vrefresh = mode->vrefresh;
			edid_ctl->sel_mode->aspect_ratio = mode->aspect_ratio;
			DP_INFO(": %s %s %dx%d %d %d matched\n", mode->name,
				mode->vdo, mode->hdisplay, mode->vdisplay,
				mode->vrefresh, mode->aspect_ratio);
			return true;
		}
	}
	return false;
}

void nubia_edid_modes(struct drm_connector *connector)
{
	struct drm_display_mode *mode;
	u16 string_len = 0, offset = 0, vrefresh = 0;
	u8 edid_cnt = 0, len = 0;
	u8 *monitor_name = &connector->eld[DRM_ELD_MONITOR_NAME_STRING];
	bool user_stored = false;
	u32 dsc_version_major, dsc_version_minor;
	bool dsc_version_supported = false;
	bool hdmi_cable;

	DP_DEBUG(": +\n");
	if (!dp_debug_p) {
		DP_WARN(": dp_debug_p = NULL\n");
		return;
	}
	if (list_empty(&connector->modes)) {
		DP_INFO(": connector modes is empty\n");
		return;
	}
	if (!edid_ctl || !edid_ctl->sel_mode) {
		DP_WARN(": edid_ctl or edid_ctl->sel_mode = NULL\n");
		return;
	}
	dsc_version_major = dp_debug_p->panel->sink_dsc_caps.version & 0xF;
	dsc_version_minor =
			(dp_debug_p->panel->sink_dsc_caps.version >> 4) & 0xF;
	dsc_version_supported = (dsc_version_major == 0x1 &&
			(dsc_version_minor == 0x1 || dsc_version_minor == 0x2))
			? true : false;
	hdmi_cable = drm_detect_hdmi_monitor(
			dp_debug_p->panel->edid_ctrl->edid);
	DP_INFO(": dp revision = 0x%x, hdmi cable = %d\n",
			dp_debug_p->pll->dp_core_revision, hdmi_cable);
	DP_INFO(": sink dsc version: %d.%d, dsc_version_supported = %d\n",
			dsc_version_major, dsc_version_minor,
			dsc_version_supported);
	string_len = strlen(monitor_name);
	if (string_len > 0) {
		trim_monitor_string(monitor_name, edid_ctl->name);
		DP_INFO("[monitor_name]: original = %s, trimed = %s\n",
				monitor_name, edid_ctl->name);
	} else {
		DP_WARN(": original monitor name length = %d\n", string_len);
	}
	if (match_user_edid_modes())
		user_stored = true;
	list_for_each_entry (mode, &connector->modes, head) {
		vrefresh = drm_mode_vrefresh(mode);
		if (filter_edid_modes(mode->hdisplay,
				mode->vdisplay, vrefresh))
			continue;
		if (mode->hdisplay && mode->vdisplay) {
			len = snprintf(edid_ctl->edid_modes + offset,
				SZ_32, "%dx%d %d %d\n", mode->hdisplay,
				mode->vdisplay, vrefresh,
				mode->picture_aspect_ratio);
			offset += len;
		} else {
			DP_INFO(": hdisplay or vdisplay = 0\n");
		}
		DP_INFO(": %dx%d %d %d\n", mode->hdisplay, mode->vdisplay,
				vrefresh, mode->picture_aspect_ratio);
		if (offset > EDID_MODES_SIZE) {
			DP_ERR(": edid_ctl->edid_modes overflow\n");
			return;
		}
		if (!user_stored)
			get_higher_refresh_resolution(mode->hdisplay,
					mode->vdisplay, vrefresh,
					mode->picture_aspect_ratio, true);
		edid_cnt++;
	}
	if (!edid_ctl->sel_mode->hdisplay || !edid_ctl->sel_mode->vdisplay) {
		DP_WARN(": no 16:9 resolutions, use others\n");
		if (!user_stored) {
			list_for_each_entry (mode, &connector->modes, head) {
				vrefresh = drm_mode_vrefresh(mode);
				get_higher_refresh_resolution(mode->hdisplay,
					mode->vdisplay, vrefresh,
					mode->picture_aspect_ratio, false);
			}
		}
	}
	if (edid_cnt) {
		if (edid_ctl->sel_mode->hdisplay
				&& edid_ctl->sel_mode->vdisplay) {
			dp_debug_p->panel->mode_override = true;
			dp_debug_p->panel->hdisplay =
					edid_ctl->sel_mode->hdisplay;
			dp_debug_p->panel->vdisplay =
					edid_ctl->sel_mode->vdisplay;
			dp_debug_p->panel->vrefresh =
					edid_ctl->sel_mode->vrefresh;
			dp_debug_p->panel->aspect_ratio =
					edid_ctl->sel_mode->aspect_ratio;
			DP_INFO(": selected_edid_mode: %dx%d %d %d,"
					" mode_override = %d\n",
					edid_ctl->sel_mode->hdisplay,
					edid_ctl->sel_mode->vdisplay,
					edid_ctl->sel_mode->vrefresh,
					edid_ctl->sel_mode->aspect_ratio,
					dp_debug_p->panel->mode_override);
		} else {
			DP_ERR(": no best edid mode found\n");
		}
		string_len = strlen(edid_ctl->edid_modes);
		if (string_len > 0)
			edid_ctl->cable_connected = true;
		DP_INFO(": edid_modes length = %d, cable_connected = %d\n",
				string_len, edid_ctl->cable_connected);
	}
	DP_INFO(": edid_modes count = %d\n", edid_cnt);
	DP_DEBUG(": -\n");
}

static ssize_t dp_debug_hpd_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf,
		size_t size)
{
	int const hpd_data_mask = 0x7;
	int hpd = 0;

	if (!dp_debug_p)
		return -ENODEV;
	sscanf(buf, "%d", &hpd);
	DP_DEBUG(": hpd = %d\n", hpd);
	hpd &= hpd_data_mask;
	dp_debug_p->hotplug = !!(hpd & BIT(0));
	dp_debug_p->dp_debug.psm_enabled = !!(hpd & BIT(1));
	if (hpd == 0)
		edid_ctl->simulate_hpd = true;
	dp_debug_p->hpd->simulate_connect(dp_debug_p->hpd,
			dp_debug_p->hotplug);
	DP_INFO(": %s\n", dp_debug_p->hotplug ?
			"dp debug hpd connect" : "dp debug hpd disconnect");
	return size;
}

static ssize_t edid_modes_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	u16 len = 0;

	if (!edid_ctl->sel_mode->hdisplay || !edid_ctl->sel_mode->vdisplay) {
		DP_INFO(": edid mode not been initialized\n");
		return len;
	}
	len = snprintf(buf, SZ_32, "%dx%d %d %d\n",
			edid_ctl->sel_mode->hdisplay,
			edid_ctl->sel_mode->vdisplay,
			edid_ctl->sel_mode->vrefresh,
			edid_ctl->sel_mode->aspect_ratio);
	len += snprintf(buf + len, EDID_MODES_SIZE, "%s", edid_ctl->edid_modes);
	DP_INFO(": selected_edid_mode: %dx%d %d %d\n",
			edid_ctl->sel_mode->hdisplay,
			edid_ctl->sel_mode->vdisplay,
			edid_ctl->sel_mode->vrefresh,
			edid_ctl->sel_mode->aspect_ratio);
	return len;
}

static ssize_t edid_modes_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf,
		size_t size)
{
	u16 hdisplay = 0, vdisplay = 0, vrefresh = 0;
	u8 aspect_ratio = 0;

	if (!dp_debug_p)
		return -ENODEV;
	if (sscanf(buf, "%d %d %d %d", &hdisplay, &vdisplay,
			&vrefresh, &aspect_ratio) != 4) {
		DP_INFO(": clearing debug modes\n");
		dp_debug_p->panel->mode_override = false;
	}
	DP_DEBUG(": hdisplay = %d, vdisplay = %d,"
			" vrefresh = %d, aspect_ratio = %d\n",
			hdisplay, vdisplay, vrefresh, aspect_ratio);
	dp_debug_p->panel->mode_override = true;
	dp_debug_p->panel->hdisplay = hdisplay;
	dp_debug_p->panel->vdisplay = vdisplay;
	dp_debug_p->panel->vrefresh = vrefresh;
	dp_debug_p->panel->aspect_ratio = aspect_ratio;
	edid_ctl->sel_mode->hdisplay = hdisplay;
	edid_ctl->sel_mode->vdisplay = vdisplay;
	edid_ctl->sel_mode->vrefresh = vrefresh;
	edid_ctl->sel_mode->aspect_ratio = aspect_ratio;
	store_user_edid_modes(hdisplay, vdisplay, vrefresh, aspect_ratio);
	return size;
}

static struct kobj_attribute disp_attrs[] = {
	__ATTR(edid_modes, 0664, edid_modes_show, edid_modes_store),
	__ATTR(hpd, 0664, NULL, dp_debug_hpd_store),
};

int __init nubia_dp_preference_init(void)
{
	int ret = 0;
	int i = 0;

	edid_ctl = kzalloc(sizeof(struct edid_control), GFP_KERNEL);
	if (!edid_ctl)
		DP_ERR(": edid_ctl alloc failed !\n");
	edid_ctl->edid_modes = kzalloc(EDID_MODES_SIZE, GFP_KERNEL);
	if (!edid_ctl->edid_modes)
		DP_ERR(": edid_ctl->edid_modes alloc failed !\n");
	edid_ctl->sel_mode = kzalloc(sizeof(struct selected_edid_mode),
			GFP_KERNEL);
	if (!edid_ctl->sel_mode)
		DP_ERR(": edid_ctl->sel_mode alloc failed !\n");
	INIT_LIST_HEAD(&edid_ctl->user_modes);
	if (!nubia_global_enhace_kobj)
		DP_ERR(": lcd_enhance fail nubia_global_enhace_kobj = %x\n",
			nubia_global_enhace_kobj);
	for (i = 0; i < ARRAY_SIZE(disp_attrs); i++) {
		ret = sysfs_create_file(nubia_global_enhace_kobj,
				&disp_attrs[i].attr);
		if (ret)
			DP_ERR(": create sysfs file failed !\n");
	}
	return ret;
}

void __exit nubia_dp_preference_exit(void)
{
	int i = 0;
	struct user_edid_mode *mode;

	for (i = 0; i < ARRAY_SIZE(disp_attrs); i++)
		sysfs_remove_file(nubia_global_enhace_kobj,
				&disp_attrs[i].attr);
	kobject_put(nubia_global_enhace_kobj);
	if (edid_ctl->edid_modes)
		kfree(edid_ctl->edid_modes);
	if (edid_ctl->sel_mode)
		kfree(edid_ctl->sel_mode);
	if (edid_ctl)
		kfree(edid_ctl);
	list_for_each_entry (mode, &edid_ctl->user_modes, list) {
		list_del(&mode->list);
		kfree(mode);
	}
}

MODULE_AUTHOR("NUBIA");
MODULE_DESCRIPTION("Nubia DisplayPort");
MODULE_LICENSE("GPL");
