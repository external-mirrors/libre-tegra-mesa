/*
 * Copyright © 2012, 2013 Thierry Reding
 * Copyright © 2013 Erik Faye-Lund
 * Copyright © 2014 NVIDIA Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __DRM_OPENTEGRA_H__
#define __DRM_OPENTEGRA_H__ 1

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "uapi/tegra.h"

#ifndef __maybe_unused
#define __maybe_unused  __attribute__((unused))
#endif

enum drm_tegra_soc_id {
	DRM_TEGRA_INVALID_SOC,
	DRM_TEGRA_UNKOWN_SOC,
	DRM_TEGRA20_SOC,
	DRM_TEGRA30_SOC,
	DRM_TEGRA114_SOC,
};

static __maybe_unused const char * const drm_tegra_soc_names[] = {
	[DRM_TEGRA_INVALID_SOC] = "invalid",
	[DRM_TEGRA_UNKOWN_SOC] = "unknown",
	[DRM_TEGRA20_SOC] = "Tegra20",
	[DRM_TEGRA30_SOC] = "Tegra30",
	[DRM_TEGRA114_SOC] = "Tegra114",
};

struct drm_tegra_bo;

enum drm_tegra_soc_id drm_tegra_get_soc_id(struct drm_tegra *drm);

int drm_tegra_bo_cpu_prep(struct drm_tegra_bo *bo,
			  uint32_t flags, uint32_t timeout_us);
#endif /* __DRM_TEGRA_H__ */
