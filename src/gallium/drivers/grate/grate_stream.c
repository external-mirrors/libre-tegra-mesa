/*
 * Copyright (c) 2016-2017 Dmitry Osipenko <digetx@gmail.com>
 * Copyright (C) 2012-2013 NVIDIA Corporation.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS\n", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Arto Merilainen <amerilainen@nvidia.com>
 */

#include "util/hash_table.h"
#include <linux/errno.h>
#include <linux/types.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "host1x01_hardware.h"
#include "hw_host1x01_uclass.h"
#include "private.h"
#include "grate_stream.h"

#define ErrorMsg(fmt, args...) \
    fprintf(stderr, "%s:%d/%s(): " fmt, \
            __FILE__, __LINE__, __func__, ##args)

static void __delete_entry_bo_mapping(struct hash_entry *entry) {
   drm_tegra_channel_unmap(entry->data);
}

/*
 * grate_stream_create(channel)
 *
 * Create a stream for given channel. This function preallocates several
 * command buffers for later usage to improve performance. Streams are
 * used for generating command buffers opcode by opcode using
 * grate_stream_push().
 */

int
grate_stream_create(struct drm_tegra *drm,
                    struct drm_tegra_channel *channel,
                    struct grate_stream *stream,
                    uint32_t words_num)
{
   stream->status    = GRATE_STREAM_FREE;
   stream->channel   = channel;
   stream->num_words = words_num;
   stream->bo_mappings = _mesa_pointer_hash_table_create(NULL);

   return 0;
}

/*
 * grate_stream_destroy(stream)
 *
 * Destroy the given stream object. All resrouces are released.
 */

void
grate_stream_destroy(struct grate_stream *stream)
{
   if (!stream)
      return;

   _mesa_hash_table_destroy(stream->bo_mappings, __delete_entry_bo_mapping);
   drm_tegra_job_free(stream->job);
}

/*
 * grate_stream_flush(stream, fence)
 *
 * Send the current contents of stream buffer. The stream must be
 * synchronized correctly (we cannot send partial streams). If
 * pointer to fence is given, the fence will contain the syncpoint value
 * that is reached when operations in the buffer are finished.
 */

int
grate_stream_flush(struct grate_stream *stream)
{
   int result = 0;

   if (!stream)
      return -1;

   /* Reflushing is fine */
   if (stream->status == GRATE_STREAM_FREE)
      return 0;

   /* Return error if stream is constructed badly */
   if (stream->status != GRATE_STREAM_READY) {
      result = -1;
      goto cleanup;
   }

   result = drm_tegra_job_submit(stream->job, NULL);
   if (result != 0) {
      ErrorMsg("drm_tegra_job_submit() failed %d\n", result);
      result = -1;
      goto cleanup;
   }

   result = drm_tegra_job_wait(stream->job, 1000);
   if (result != 0) {
      ErrorMsg("drm_tegra_job_wait() failed %d\n", result);
      result = -1;
   }

cleanup:
   _mesa_hash_table_clear(stream->bo_mappings, __delete_entry_bo_mapping);

   drm_tegra_job_free(stream->job);

   stream->job = NULL;
   stream->status = GRATE_STREAM_FREE;

   return result;
}

/*
 * grate_stream_begin(stream, num_words, fence, num_fences, num_syncpt_incrs,
 *          num_relocs, class_id)
 *
 * Start constructing a stream.
 *  - num_words refer to the maximum number of words the stream can contain.
 *  - fence is a pointer to a table that contains syncpoint preconditions
 *    before the stream execution can start.
 *  - num_fences indicate the number of elements in the fence table.
 *  - num_relocs indicate the number of memory references in the buffer.
 *  - class_id refers to the class_id that is selected in the beginning of a
 *    stream. If no class id is given, the default class id (=usually the
 *    client device's class) is selected.
 *
 * This function verifies that the current buffer has enough room for holding
 * the whole stream (this is computed using num_words and num_relocs). The
 * function blocks until the stream buffer is ready for use.
 */

int
grate_stream_begin(struct grate_stream *stream)
{
   int ret;

   /* check stream and its state */
   if (!(stream && stream->status == GRATE_STREAM_FREE)) {
      ErrorMsg("Stream status isn't FREE\n");
      return -1;
   }

   ret = drm_tegra_job_new(stream->channel, &stream->job);
   if (ret != 0) {
      ErrorMsg("drm_tegra_job_new() failed %d\n", ret);
      return -1;
   }

   ret = drm_tegra_job_get_pushbuf(stream->job, &stream->buffer.pushbuf);
   if (ret != 0) {
      ErrorMsg("drm_tegra_job_get_pushbuf() failed %d\n", ret);
      drm_tegra_job_free(stream->job);
      return -1;
   }

   ret = drm_tegra_pushbuf_begin(stream->buffer.pushbuf, stream->num_words, &stream->buffer.ptr);
   if (ret != 0) {
      ErrorMsg("drm_tegra_pushbuf_prepare() failed %d\n", ret);
      drm_tegra_job_free(stream->job);
      return -1;
   }

   stream->class_id = 0;
   stream->status = GRATE_STREAM_CONSTRUCT;

   return 0;
}

static int
__grate_stream_get_channel_mapping(struct grate_stream *stream,
                                   struct drm_tegra_bo *bo,
                                   uint32_t flags,
                                   struct drm_tegra_mapping **mapping)
{
   assert(stream && bo && mapping);
   if (!stream || !bo || !mapping)
       return -EINVAL;

   int ret = 0;
   struct hash_entry *bo_mapping_entry = _mesa_hash_table_search(stream->bo_mappings, bo);
   if (bo_mapping_entry) {
      *mapping = bo_mapping_entry->data;
   } else {
      ret = drm_tegra_channel_map(stream->channel, bo, 0, mapping);
      if (ret < 0) {
         stream->status = GRATE_STREAM_CONSTRUCTION_FAILED;
         ErrorMsg("drm_tegra_channel_map() failed: %d\n", ret);
         return ret;
      }
      
      _mesa_hash_table_insert(stream->bo_mappings, bo, *mapping);
   }
   
   return ret;
}

/*
 * grate_stream_push_reloc(stream, h, offset)
 *
 * Push a memory reference to the stream.
 */

int
grate_stream_push_reloc(struct grate_stream *stream,
                        struct drm_tegra_bo *bo,
                        unsigned offset)
{
   int ret;
   struct drm_tegra_mapping *mapping = NULL;

   if (!(stream && stream->status == GRATE_STREAM_CONSTRUCT)) {
      ErrorMsg("Stream status isn't CONSTRUCT\n");
      return -1;
   }
   
   ret = __grate_stream_get_channel_mapping(stream, bo, 0, &mapping);
   if (ret < 0)
      return ret;

   ret = drm_tegra_pushbuf_relocate(stream->buffer.pushbuf,
                                    &stream->buffer.ptr, mapping, offset, 0, 0);
   if (ret < 0) {
      stream->status = GRATE_STREAM_CONSTRUCTION_FAILED;
      ErrorMsg("drm_tegra_pushbuf_relocate() failed %d\n", ret);
      return ret;
   }

   return ret;
}

/*
 * grate_stream_push(stream, word)
 *
 * Push a single word to given stream.
 */

int
grate_stream_push(struct grate_stream *stream, uint32_t word)
{
   if (!(stream && stream->status == GRATE_STREAM_CONSTRUCT)) {
      ErrorMsg("Stream status isn't CONSTRUCT\n");
      return -1;
   }

   *stream->buffer.ptr++ = word;

   return 0;
}

/*
 * grate_stream_end(stream)
 *
 * Mark end of stream. This function pushes last syncpoint increment for
 * marking end of stream.
 */
 
int
grate_stream_end(struct grate_stream *stream)
{
   int ret;

   if (!(stream && stream->status == GRATE_STREAM_CONSTRUCT)) {
      ErrorMsg("Stream status isn't CONSTRUCT\n");
      return -1;
   }

   ret = drm_tegra_pushbuf_end(stream->buffer.pushbuf,
                                stream->buffer.ptr);
   stream->buffer.ptr = 0;
   if (ret != 0) {
      stream->status = GRATE_STREAM_CONSTRUCTION_FAILED;
      ErrorMsg("drm_tegra_pushbuf_sync() failed %d\n", ret);
      return -1;
   }

   stream->status = GRATE_STREAM_READY;

   return 0;
}

/*
 * grate_reloc (variable, handle, offset)
 *
 * This function creates a reloc allocation. The function should be used in
 * conjunction with grate_stream_push_words.
 */

struct grate_reloc
grate_reloc(const void *var_ptr, struct drm_tegra_bo *bo,
            uint32_t offset, uint32_t var_offset)
{
   struct grate_reloc reloc = {var_ptr, bo, offset, var_offset};
   return reloc;
}

/*
 * grate_stream_push_words(stream, addr, words, ...)
 *
 * Push words from given address to stream. The function takes
 * reloc structs as its argument. You can generate the structs with grate_reloc
 * function.
 */

int grate_stream_push_words(struct grate_stream *stream, const void *addr,
                            unsigned words, int num_relocs, ...)
{
   struct grate_reloc reloc_arg;
   struct drm_tegra_mapping *mapping;
   va_list ap;
   uint32_t *pushbuf_ptr;
   int ret;

   if (!(stream && stream->status == GRATE_STREAM_CONSTRUCT)) {
      ErrorMsg("Stream status isn't CONSTRUCT\n");
      return -1;
   }

   ret = drm_tegra_pushbuf_begin(stream->buffer.pushbuf, words, &stream->buffer.ptr);
   if (ret != 0) {
      stream->status = GRATE_STREAM_CONSTRUCTION_FAILED;
      ErrorMsg("drm_tegra_pushbuf_prepare() failed %d\n", ret);
      return -1;
   }

   /* Class id should be set explicitly, for simplicity. */
   if (stream->class_id == 0) {
      stream->status = GRATE_STREAM_CONSTRUCTION_FAILED;
      ErrorMsg("HOST1X class not specified\n");
      return -1;
   }

   /* Copy the contents */
   pushbuf_ptr = stream->buffer.ptr;
   memcpy(pushbuf_ptr, addr, words * sizeof(uint32_t));

   /* Copy relocs */
   va_start(ap, num_relocs);
   for (; num_relocs; num_relocs--) {
      reloc_arg = va_arg(ap, struct grate_reloc);

      stream->buffer.ptr  = pushbuf_ptr;
      stream->buffer.ptr += reloc_arg.var_offset / sizeof(uint32_t);
      
      ret = __grate_stream_get_channel_mapping(stream, reloc_arg.bo, 0, &mapping);
      if (ret < 0)
         break;

      ret = drm_tegra_pushbuf_relocate(stream->buffer.pushbuf,
                                       &stream->buffer.ptr, mapping,
                                       reloc_arg.offset, 0, 0);
      if (ret != 0) {
         stream->status = GRATE_STREAM_CONSTRUCTION_FAILED;
         ErrorMsg("__grate_stream_get_channel_mapping() failed %d\n", ret);
         break;
      }
   }
   va_end(ap);

   stream->buffer.ptr = pushbuf_ptr + words;

   return ret ? -1 : 0;
}
