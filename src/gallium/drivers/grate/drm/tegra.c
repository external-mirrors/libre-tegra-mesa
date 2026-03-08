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

#include <xf86drm.h>

#include "private.h"

static simple_mtx_t table_lock = SIMPLE_MTX_INITIALIZER;

/* lookup a buffer, call with table_lock mutex locked */
static struct drm_tegra_bo * lookup_bo(void *table, uint32_t key)
{
	struct drm_tegra_bo_bucket *bucket;
	struct drm_tegra_bo *bo;
	void *value;

	if (drmHashLookup(table, key, &value))
		return NULL;

	bo = value;

	if (!list_is_empty(&bo->bo_list)) {
		/* mark BO as available for access under valgrind */
		drm_tegra_reset_bo(bo, 0, false);

		/* take out BO from the bucket */
		list_del(&bo->bo_list);

		/* update bucket stats */
		bucket = drm_tegra_get_bucket(bo->drm, bo->size);
		bucket->num_entries--;
	}

	/* found, increment reference count */
	p_atomic_inc(&bo->ref);

	return bo;
}

static void drm_tegra_bo_setup_guards(struct drm_tegra_bo *bo)
{
#ifndef NDEBUG
	struct drm_tegra *drm = bo->drm;
	struct drm_tegra_gem_mmap args;
	uint64_t guard = 0x5351317315731757;
	uint8_t *map;
	size_t size;
	unsigned i;
	int err;

	if (!drm->debug_bo_front_guard && !drm->debug_bo_back_guard)
		return;

	size = bo->size;

	if (drm->debug_bo_back_guard)
		size += 4096;

	memset(&args, 0, sizeof(args));
	args.handle = bo->handle;

	err = drmCommandWriteRead(drm->fd, DRM_TEGRA_GEM_MMAP,
				  &args, sizeof(args));
	if (err < 0) {
		VDBG_BO(bo, "failed get mapping offset err %d (%s)\n",
			err, strerror(-err));
		abort();
	}

	map = mmap(0, bo->offset + size, PROT_READ | PROT_WRITE, MAP_SHARED,
		   drm->fd, args.offset);
	if (map == MAP_FAILED) {
		VDBG_BO(bo, "failed to map guard 0x%llX err %d (%s)\n",
			args.offset, -errno, strerror(errno));
		abort();
	}

	if (drm->debug_bo_front_guard)
		bo->guard_front = (uint64_t *)map;

	if (drm->debug_bo_back_guard)
		bo->guard_back = (uint64_t *)(map + bo->offset + bo->size);

	VDBG_BO(bo, "front %p back %p\n", bo->guard_front, bo->guard_back);

	/* we only interested in guards mapping, hence unmap the actual BO */
	if (bo->size >= 4096)
		munmap(map + bo->offset, ALIGN_POT(bo->size - 4095, 4096));

	if (bo->handle & 1)
		guard = ~guard;

	if (drm->debug_bo_front_guard)
		for (i = 0; i < 512; i++)
			bo->guard_front[i] = guard;

	if (drm->debug_bo_back_guard)
		for (i = 0; i < 512; i++)
			bo->guard_back[i] = guard;
#endif
}

static void drm_tegra_bo_check_guards(struct drm_tegra_bo *bo)
{
#ifndef NDEBUG
	struct drm_tegra *drm = bo->drm;
	uint64_t guard_check = 0x5351317315731757;
	uint64_t guard;
	unsigned i;

	if (!drm->debug_bo_front_guard && !drm->debug_bo_back_guard)
		return;

	VDBG_BO(bo, "front %p back %p\n", bo->guard_front, bo->guard_back);

	if (bo->handle & 1)
		guard_check = ~guard_check;

	if (drm->debug_bo_front_guard) {
		for (i = 0; i < 512; i++) {
			guard = bo->guard_front[i];

			if (guard != guard_check) {
				VDBG_BO(bo, "front guard is corrupted: entry %u is 0x%16" PRIX64 ", should be 0x%16" PRIX64 "\n",
					i, guard, guard_check);
				abort();
			}
		}
	}

	if (drm->debug_bo_back_guard) {
		for (i = 0; i < 512; i++) {
			guard = bo->guard_back[i];

			if (guard != guard_check) {
				VDBG_BO(bo, "back guard is corrupted: entry %u is 0x%16" PRIX64 ", should be 0x%16" PRIX64 "\n",
					i, guard, guard_check);
				abort();
			}
		}
	}
#endif
}

static void drm_tegra_bo_unmap_guards(struct drm_tegra_bo *bo)
{
#ifndef NDEBUG
	unsigned long unaligned = (unsigned long)bo->guard_back;
	unsigned long aligned = ALIGN_POT(unaligned - 4095, 4096);
	void *guard_back = (void *)aligned;
	unsigned guard_back_size = 4096;
	struct drm_tegra *drm = bo->drm;

	if (!drm->debug_bo_front_guard && !drm->debug_bo_back_guard)
		return;

	VDBG_BO(bo, "front %p back %p (%u)\n",
		bo->guard_front, guard_back, guard_back_size);

	if (bo->size & 4095)
		guard_back_size *= 2;

	if (drm->debug_bo_front_guard)
		munmap(bo->guard_front, 4096);

	if (drm->debug_bo_back_guard)
		munmap(guard_back, guard_back_size);

	bo->guard_front = NULL;
	bo->guard_back = NULL;
#endif
}

int drm_tegra_bo_free(struct drm_tegra_bo *bo)
{
	struct drm_tegra *drm = bo->drm;
	struct drm_gem_close args;
	void *map_cached;
	int err;

	DBG_BO(bo, "\n");

#ifndef NDEBUG
	if (drm->debug_bo) {
		drm->debug_bos_allocated--;
		drm->debug_bos_total_size -= bo->debug_size;
	}
#endif
	drm_tegra_bo_unmap_guards(bo);

	if (bo->map) {
		if (RUNNING_ON_VALGRIND)
			VG_BO_UNMMAP(bo);
		else
			munmap(bo->map, bo->offset + bo->size);

	} else if (bo->map_cached) {
		map_cached = drm_tegra_bo_cache_map(bo);

		if (!RUNNING_ON_VALGRIND)
			munmap(map_cached, bo->offset + bo->size);
	} else {
		goto vg_free;
	}

#ifndef NDEBUG
	if (drm->debug_bo) {
		drm->debug_bos_mapped--;
		drm->debug_bos_total_pages -= bo->debug_size / 4096;
	}
#endif
vg_free:
	VG_BO_FREE(bo);

	if (bo->name)
		drmHashDelete(drm->name_table, bo->name);

	drmHashDelete(drm->handle_table, bo->handle);

	memset(&args, 0, sizeof(args));
	args.handle = bo->handle;

	err = drmIoctl(drm->fd, DRM_IOCTL_GEM_CLOSE, &args);
	if (err < 0)
		err = -errno;

	free(bo);

	DBG_BO_STATS(drm);

	return err;
}

static void drm_tegra_setup_debug(struct drm_tegra *drm)
{
#ifndef NDEBUG
	char *str;

	str = getenv("LIBDRM_TEGRA_DEBUG_BO");
	drm->debug_bo = (str && strcmp(str, "1") == 0);

	str = getenv("LIBDRM_TEGRA_DEBUG_BO_BACK_GUARD");
	drm->debug_bo_back_guard = (str && strcmp(str, "1") == 0);

	str = getenv("LIBDRM_TEGRA_DEBUG_BO_FRONT_GUARD");
	drm->debug_bo_front_guard = (str && strcmp(str, "1") == 0);
#endif
}

static int drm_tegra_wrap(struct drm_tegra **drmp, int fd, bool close,
			  int version_major)
{
	struct drm_tegra *drm;

	if (fd < 0 || !drmp)
		return -EINVAL;

	drm = calloc(1, sizeof(*drm));
	if (!drm)
		return -ENOMEM;

	drm->version = version_major;
	drm->close = close;
	drm->fd = fd;

	drm_tegra_bo_cache_init(&drm->bo_cache, false);
	drm->handle_table = drmHashCreate();
	drm->name_table = drmHashCreate();
	list_inithead(&drm->mmap_cache.list);

	if (!drm->handle_table || !drm->name_table)
		return -ENOMEM;

	drm_tegra_setup_debug(drm);

	*drmp = drm;

	return 0;
}

void drm_tegra_close(struct drm_tegra *drm)
{
	if (!drm)
		return;

	drm_tegra_bo_cache_cleanup(drm, 0);
	drmHashDestroy(drm->handle_table);
	drmHashDestroy(drm->name_table);

	if (drm->close)
		close(drm->fd);

	free(drm);
}

struct drm_tegra_bo *drm_tegra_bo_ref(struct drm_tegra_bo *bo)
{
	if (bo) {
		DBG_BO(bo, "\n");

		p_atomic_inc(&bo->ref);
	}

	return bo;
}

int drm_tegra_bo_get_handle(struct drm_tegra_bo *bo, uint32_t *handle)
{
	if (!bo || !handle)
		return -EINVAL;

	*handle = bo->handle;

	return 0;
}

int __drm_tegra_bo_map(struct drm_tegra_bo *bo, void **ptr)
{
	struct drm_tegra *drm = bo->drm;
	struct drm_tegra_gem_mmap args;
	uint8_t *map;
	int err;

	map = drm_tegra_bo_cache_map(bo);
	if (map) {
		DBG_BO(bo, "success from cache\n");
		goto out;
	}

#if HAVE_VALGRIND
	if (RUNNING_ON_VALGRIND && bo->map_vg) {
		map = bo->map_vg;
		goto map_cnt;
	}
#endif

	memset(&args, 0, sizeof(args));
	args.handle = bo->handle;

	err = drmCommandWriteRead(drm->fd, DRM_TEGRA_GEM_MMAP,
				  &args, sizeof(args));
	if (err < 0) {
		VDBG_BO(bo, "failed get mapping offset err %d (%s)\n",
			err, strerror(-err));
		return err;
	}

	map = mmap(0, bo->offset + bo->size, PROT_READ | PROT_WRITE, MAP_SHARED,
		   drm->fd, args.offset);
	if (map == MAP_FAILED) {
		VDBG_BO(bo, "failed to map offset 0x%llX err %d (%s)\n",
			args.offset, -errno, strerror(errno));
		*ptr = NULL;
		return -errno;
	}

	map += bo->offset;
#if HAVE_VALGRIND
map_cnt:
#endif
#ifndef NDEBUG
	if (drm->debug_bo && ptr == &bo->map) {
		drm->debug_bos_mapped++;
		drm->debug_bos_total_pages += bo->debug_size / 4096;
	}
#endif
	DBG_BO(bo, "success\n");
out:
	if (ptr == &bo->map)
		DBG_BO_STATS(drm);

	*ptr = map;

	return 0;
}

int drm_tegra_bo_map(struct drm_tegra_bo *bo, void **ptr)
{
	int err = 0;

	if (!bo)
		return -EINVAL;

	simple_mtx_lock(&table_lock);

	if (!bo->map) {
		err = __drm_tegra_bo_map(bo, &bo->map);
		if (err)
			goto out;

		VG_BO_MMAP(bo);

		bo->mmap_ref = 1;
	} else {
		DBG_BO(bo, "\n");
		bo->mmap_ref++;
	}
out:
	if (ptr)
		*ptr = bo->map;

	simple_mtx_unlock(&table_lock);

	return err;
}

int drm_tegra_bo_unmap(struct drm_tegra_bo *bo)
{
	if (!bo)
		return -EINVAL;

	DBG_BO(bo, "\n");

	simple_mtx_lock(&table_lock);

	if (bo->mmap_ref == 0)
		goto unlock;

	if (--bo->mmap_ref > 0)
		goto unlock;

	VG_BO_UNMMAP(bo);

	drm_tegra_bo_cache_unmap(bo);
	bo->map = NULL;
unlock:
	simple_mtx_unlock(&table_lock);

	return 0;
}

int drm_tegra_bo_get_name(struct drm_tegra_bo *bo, uint32_t *name)
{
	if (!bo || !name)
		return -EINVAL;

	if (!bo->name) {
		struct drm_gem_flink args;
		int err;

		memset(&args, 0, sizeof(args));
		args.handle = bo->handle;

		err = drmIoctl(bo->drm->fd, DRM_IOCTL_GEM_FLINK, &args);
		if (err < 0) {
			VDBG_BO(bo, "err %d strerror(%s)\n",
				err, strerror(-err));
			return -errno;
		}

		simple_mtx_lock(&table_lock);

		drmHashInsert(bo->drm->name_table, args.name, bo);
		bo->name = args.name;

		simple_mtx_unlock(&table_lock);
	}

	*name = bo->name;

	DBG_BO(bo, "\n");

	return 0;
}

int drm_tegra_bo_cpu_prep(struct drm_tegra_bo *bo,
			  uint32_t flags, uint32_t timeout_us)
{
	struct drm_tegra_gem_cpu_prep args;
	int ret;

	if (!bo)
		return -EINVAL;

	memset(&args, 0, sizeof(args));
	args.flags = flags;
	args.handle = bo->handle;
	args.timeout = timeout_us;

	ret = drmCommandWriteRead(bo->drm->fd, DRM_TEGRA_GEM_CPU_PREP, &args,
				  sizeof(args));
	if (ret && ret != -EBUSY && ret != -ETIMEDOUT)
		VDBG_BO(bo, "failed flags 0x%08X timeout_us %u err %d (%s)\n",
			flags, timeout_us, ret, strerror(-ret));
	else
		VDBG_BO(bo, "%s flags 0x%08X timeout_us %u\n",
			ret ? "busy" : "idling", flags, timeout_us);

	return ret;
}

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
