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

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "grate_utils.h"

static enum drm_tegra_soc_id read_chip_id(const char *path)
{
	FILE *file = fopen(path, "r");
	if (file) {
		unsigned int id = 0;

		if (fscanf(file, "%d", &id) != 1)
			fprintf(stderr, "fscanf failed for %s\n", path);
		fclose(file);

		switch (id) {
		case 0x20:
			return DRM_TEGRA20_SOC;
		case 0x30:
			return DRM_TEGRA30_SOC;
		case 0x35:
			return DRM_TEGRA114_SOC;
		}

		return DRM_TEGRA_UNKOWN_SOC;
	}

	return DRM_TEGRA_INVALID_SOC;
}

enum drm_tegra_soc_id drm_tegra_get_soc_id(struct drm_tegra *drm)
{
	static enum drm_tegra_soc_id sid = DRM_TEGRA_INVALID_SOC;

	if (sid != DRM_TEGRA_INVALID_SOC)
		return sid;

	sid = read_chip_id("/sys/devices/soc0/soc_id");
	if (sid != DRM_TEGRA_INVALID_SOC)
		return sid;

	VDBG_DRM(drm, "failed to identify SoC version\n");
	sid = DRM_TEGRA_UNKOWN_SOC;

	return sid;
}
