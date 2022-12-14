/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _GSP_LAYER_H
#define _GSP_LAYER_H

#include <linux/device.h>
#include <drm/gsp_cfg.h>

#define gsp_layer_common_copy_from_user(layer, layer_user) \
do { \
	(layer)->common.type = (layer_user)->common.type; \
	(layer)->common.enable = (layer_user)->common.enable; \
	(layer)->common.wait_fd = (layer_user)->common.wait_fd; \
	(layer)->common.sig_fd = (layer_user)->common.sig_fd; \
	(layer)->common.mem_data.share_fd = (layer_user)->common.share_fd; \
	(layer)->common.mem_data.uv_offset = \
				(layer_user)->common.offset.uv_offset; \
	(layer)->common.mem_data.v_offset = \
				(layer_user)->common.offset.v_offset; \
	(layer)->common.src_addr = (layer_user)->common.src_addr; \
} while (0)

int gsp_layer_get_dmabuf(struct gsp_layer *layer);
void gsp_layer_put_dmabuf(struct gsp_layer *layer);

int gsp_layer_iommu_map(struct gsp_layer *layer, struct device *dev);
void gsp_layer_iommu_unmap(struct gsp_layer *layer, struct device *dev);

int gsp_layer_need_iommu(struct gsp_layer *layer);

int gsp_layer_has_share_fd(struct gsp_layer *layer);

int gsp_layer_to_type(struct gsp_layer *layer);
int gsp_layer_to_wait_fd(struct gsp_layer *layer);
int gsp_layer_to_share_fd(struct gsp_layer *layer);

int gsp_layer_is_filled(struct gsp_layer *layer);
void gsp_layer_set_filled(struct gsp_layer *layer);

void gsp_layer_common_print(struct gsp_layer *layer);
#endif
