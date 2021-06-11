/*
Copyright (c) 2012, Broadcom Europe Ltd
Copyright (c) 2021, Amazon.com, Inc. or its affiliates.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include "containers.h"
#include "core/containers_common.h"
#include "core/containers_logging.h"
#include "core/containers_utils.h"
#include "core/containers_io.h"

static int print_info(VC_CONTAINER_T *ctx, bool b_reader);
static int parse_cmdline(unsigned int argc, char **argv);

static int test_mp4(void);

static struct
{
   const char *name;
   int (*test)(void);
} tests_all[] =
{
   {"mp4", test_mp4},
};
#define TESTS_NUM (sizeof(tests_all)/sizeof(tests_all[0]))

static unsigned tests_index[TESTS_NUM];
static unsigned int tests_num = 0;

static int32_t verbosity = VC_CONTAINER_LOG_ERROR|VC_CONTAINER_LOG_INFO;
static int32_t verbosity_input = -1, verbosity_output = -1;

#define DECLARE_ES_FORMATS(fmt, num, esflags) \
   VC_CONTAINER_ES_SPECIFIC_FORMAT_T fmt_es[num] = {{{0}}}; \
   VC_CONTAINER_ES_FORMAT_T fmt[num] = {{0}}; \
   for(int i = 0; i < num; i++) { \
      fmt[i].type = &fmt_es[i]; \
      fmt[i].flags = esflags; \
   }

/*****************************************************************************/
int main(int argc, char **argv)
{
   unsigned int i, passed = 0;

   if(parse_cmdline(argc, argv))
      goto error;

   /* Set the general verbosity */
   vc_container_log_set_verbosity(0, verbosity);

   if(verbosity_input < 0) verbosity_input = verbosity;
   if(verbosity_output < 0) verbosity_output = verbosity;

   /* Run tests */
   for(i = 0; i < tests_num; i++)
   {
      unsigned int index = tests_index[i];
      int ret;

      LOG_INFO(0, "run test %i:%s", i, tests_all[index].name);
      ret = tests_all[index].test();
      if(ret)
         LOG_ERROR(0, "test %s failed (%i)", tests_all[index].name, ret);
      else
         passed++;
   }

   LOG_INFO(0, "Tests passed: %u, failed: %u", passed, tests_num - passed);

 error:
   return passed && passed == tests_num ? 0 : -1;
}

static int parse_cmdline(unsigned int argc, char **argv)
{
   unsigned int i, j, k;
   int32_t *p_verbosity;
   const char *psz_name;

   /* Parse the command line arguments */
   for(i = 1; i < argc; i++)
   {
      if(!argv[i]) continue;

      if(argv[i][0] != '-')
      {
         /* Not an option argument so will be test name */
         if(tests_num >= TESTS_NUM)
         {
            LOG_ERROR(0, "too many tests specified");
            return 1;
         }

         /* Find the matching test */
         for(j = 0; j < TESTS_NUM; j++)
            if(!strcmp(argv[i], tests_all[j].name)) break;
         if(j == TESTS_NUM)
         {
            LOG_ERROR(0, "unrecognized test: %s", argv[i]);
            return 1;
         }

         tests_index[tests_num++] = j;
         continue;
      }

      /* We are now dealing with command line options */
      switch(argv[i][1])
      {
      case 'v':
         if(argv[i][2] == 'i') {j = 3; p_verbosity = &verbosity_input;}
         else if(argv[i][2] == 'o') {j = 3; p_verbosity = &verbosity_output;}
         else {j = 2; p_verbosity = &verbosity;}
         *p_verbosity = VC_CONTAINER_LOG_ERROR|VC_CONTAINER_LOG_INFO;
         for(k = 0; k < 2 && argv[i][j+k] == 'v'; k++)
            *p_verbosity = (*p_verbosity << 1) | 1 ;
         break;
      case 'h': goto usage;
      case 'l': goto list;
      default: goto invalid_option;
      }
      continue;
   }

   /* If no test specified, add all of them */
   if(!tests_num)
   {
      for(i = 0; i < TESTS_NUM; i++)
         tests_index[i] = i;
      tests_num = TESTS_NUM;
   }

   return 0;

 invalid_option:
   LOG_ERROR(0, "invalid command line option (%s)", argv[i]);

 usage:
   psz_name = strrchr(argv[0], '\\'); if(psz_name) psz_name++;
   if(!psz_name) {psz_name = strrchr(argv[0], '/'); if(psz_name) psz_name++;}
   if(!psz_name) psz_name = argv[0];
   LOG_INFO(0, "");
   LOG_INFO(0, "usage: %s [options] test", psz_name);
   LOG_INFO(0, "options list:");
   LOG_INFO(0, " -vxx  : general verbosity level (replace xx with a number of \'v\')");
   LOG_INFO(0, " -vixx : verbosity specific to the input container");
   LOG_INFO(0, " -voxx : verbosity specific to the output container");
   LOG_INFO(0, " -l    : list tests");
   LOG_INFO(0, " -h    : help");
   return 1;

 list:
   LOG_INFO(0, "");
   LOG_INFO(0, "tests list:");
   for(i = 0; i < TESTS_NUM; i++)
      LOG_INFO(0, " %s", tests_all[i].name);
   return 1;
}

static VC_CONTAINER_STATUS_T generate_container(const char *psz_out,
    unsigned int tracks, VC_CONTAINER_ES_FORMAT_T *fmt,
    unsigned int pkts_num, VC_CONTAINER_PACKET_T *pkts,
    bool b_info)
{
   VC_CONTAINER_STATUS_T status;
   VC_CONTAINER_T *ctx;
   unsigned i;

   LOG_INFO(0, "generating %s", psz_out);

   vc_container_log_set_default_verbosity(verbosity_output);

   ctx = vc_container_open_writer(psz_out, &status, 0, 0);
   if(!ctx)
   {
      LOG_ERROR(0, "error opening file %s (%i)", psz_out, status);
      return status;
   }

   for(i = 0; i < tracks; i++)
   {
      status = vc_container_control (ctx, VC_CONTAINER_CONTROL_TRACK_ADD, fmt + i);
      if(status != VC_CONTAINER_SUCCESS)
      {
         LOG_ERROR(0, "error adding track %i (%i)", i, status);
         goto error;
      }
   }

   for(i = 0; i < pkts_num; i++)
   {
      status = vc_container_write (ctx, pkts + i);
      if(status != VC_CONTAINER_SUCCESS)
      {
         LOG_ERROR(0, "error writing packet %i (%i)", i, status);
         goto error;
      }
   }

error:
   if(b_info)
      print_info(ctx, false);

   vc_container_close(ctx);
   return status;
}

static int verify_container(const char *psz_in,
    unsigned int tracks, VC_CONTAINER_ES_FORMAT_T *fmt,
    unsigned int pkts_num, VC_CONTAINER_PACKET_T *pkts,
    int64_t ts_offset_us,
    bool b_info)
{
   VC_CONTAINER_STATUS_T status;
   VC_CONTAINER_T *ctx;
   unsigned int i;

   LOG_INFO(0, "verifying %s", psz_in);

   vc_container_log_set_default_verbosity(verbosity_input);

   ctx = vc_container_open_reader(psz_in, &status, 0, 0);
   if(!ctx)
   {
      LOG_ERROR(0, "error opening file %s (%i)", psz_in, status);
      return status;
   }

   /* Check tracks definitions match */
   if(tracks != ctx->tracks_num)
   {
      LOG_ERROR(0, "unexpected tracks (%i/%i)", tracks, ctx->tracks_num);
      status = VC_CONTAINER_ERROR_CORRUPTED;
      goto error;
   }
   for(i = 0; i < tracks; i++)
   {
      if(vc_container_format_cmp(fmt + i, ctx->tracks[i]->format, false))
      {
         LOG_ERROR(0, "unexpected track format (%i)", i);
         vc_container_print_es_format(VC_CONTAINER_LOG_INFO, ctx->tracks[i]->format);
         vc_container_print_es_format(VC_CONTAINER_LOG_INFO, fmt + i);
         status = VC_CONTAINER_ERROR_CORRUPTED;
         goto error;
      }
   }

   /* Check packets match */
   for(i = 0; i < pkts_num; i++)
   {
      VC_CONTAINER_PACKET_T packet = {0};
      uint8_t buffer[1024];
      packet.buffer_size = sizeof(buffer);
      packet.data = buffer;

      status = vc_container_read(ctx, &packet, VC_CONTAINER_READ_FLAG_INFO);
      if(status != VC_CONTAINER_SUCCESS)
      {
         LOG_ERROR(0, "error reading packet %i (%i)", i, status);
         goto error;
      }

      if(packet.size != pkts[i].size || packet.frame_size != pkts[i].frame_size ||
         packet.pts + ts_offset_us != pkts[i].pts || packet.dts + ts_offset_us != pkts[i].dts ||
         packet.track != pkts[i].track || packet.flags != pkts[i].flags)
      {
         LOG_ERROR(0, "packet %i mismatch", i);
         LOG_INFO(0, "packet track:%i, num:%i, size:%i/%i, flags:%x, pts:%"PRId64"us, dts:%"PRId64"us",
            packet.track, packet.num, packet.size, packet.frame_size, packet.flags,
            packet.pts, packet.dts);
         LOG_INFO(0, "packet track:%i, num:%i, size:%i/%i, flags:%x, pts:%"PRId64"us, dts:%"PRId64"us",
            pkts[i].track, pkts[i].num, pkts[i].size, pkts[i].frame_size, pkts[i].flags,
            pkts[i].pts, pkts[i].dts);
         status = VC_CONTAINER_ERROR_CORRUPTED;
         goto error;
      }

      status = vc_container_read(ctx, &packet, 0);
      if(status != VC_CONTAINER_SUCCESS)
      {
         LOG_ERROR(0, "error skipping packet %i (%i)", i, status);
         goto error;
      }

      if(memcmp(buffer, pkts[i].data, pkts[i].size))
      {
         LOG_ERROR(0, "packet data %i mismatch", i);
         status = VC_CONTAINER_ERROR_CORRUPTED;
         goto error;
      }
   }

error:
   if(b_info)
      print_info(ctx, true);

   vc_container_close(ctx);
   return status;
}

static int print_info(VC_CONTAINER_T *ctx, bool b_reader)
{
   unsigned int i;

   LOG_DEBUG(0, "");
   if(b_reader) LOG_DEBUG(0, "----Reader Information----");
   else LOG_DEBUG(0, "----Writer Information----");

   LOG_DEBUG(0, "duration: %2.2fs, size: %"PRId64, ctx->duration/1000000.0, ctx->size);
   LOG_DEBUG(0, "capabilities: %x", ctx->capabilities);
   LOG_DEBUG(0, "");

   for(i = 0; i < ctx->tracks_num; i++)
   {
      LOG_DEBUG(0, "track: %i, enabled: %i", i, ctx->tracks[i]->is_enabled);
      vc_container_print_es_format(VC_CONTAINER_LOG_DEBUG, ctx->tracks[i]->format);
   }

   LOG_DEBUG(0, "--------------------------");
   LOG_DEBUG(0, "");

   return 0;
}

static void set_video_format(VC_CONTAINER_ES_FORMAT_T *fmt,
   VC_CONTAINER_FOURCC_T codec, VC_CONTAINER_FOURCC_T variant,
   unsigned width, unsigned height, bool extradata)
{
   fmt->es_type = VC_CONTAINER_ES_TYPE_VIDEO;
   fmt->codec = codec;
   fmt->codec_variant = variant;
   fmt->type->video.width = width;
   fmt->type->video.height = height;
   if(extradata)
   {
      fmt->extradata = (uint8_t *)fmt;
      fmt->extradata_size = sizeof(*fmt);
   }
}

static void set_audio_format(VC_CONTAINER_ES_FORMAT_T *fmt,
   VC_CONTAINER_FOURCC_T codec,
   unsigned channels, unsigned samplerate, bool extradata)
{
   fmt->es_type = VC_CONTAINER_ES_TYPE_AUDIO;
   fmt->codec = codec;
   fmt->type->audio.channels = channels;
   fmt->type->audio.sample_rate = samplerate;
   if(extradata)
   {
      fmt->extradata = (uint8_t *)fmt;
      fmt->extradata_size = sizeof(*fmt);
   }

   /* Hack to avoid the reader trying to parse the data */
   if(extradata && codec == VC_CONTAINER_CODEC_MP4A)
      fmt->extradata_size = 1;
}

static void fill_packets(VC_CONTAINER_PACKET_T *pkts, unsigned pkts_num,
   VC_CONTAINER_ES_FORMAT_T *fmt, unsigned fmts_num, uint64_t ts_offset_us)
{
#define DATASIZE 101
#define PATTERNS 5
   static uint8_t patterns[PATTERNS][DATASIZE] = {
      {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10},
      {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11},
      {200, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210},
      {100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110},
      {10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20}};
   static const unsigned time_incr_us = 15000;
   uint64_t ts_us = ts_offset_us;
   unsigned i, counters[10] = {0};
   assert(fmts_num < 10);

   for(i = 0; i < pkts_num; i++)
   {
      unsigned pattern = i % PATTERNS;
      unsigned track = i % fmts_num;
      pkts[i].buffer_size = DATASIZE;
      pkts[i].size = pkts[i].frame_size = DATASIZE - pattern;
      pkts[i].data = patterns[pattern];
      pkts[i].track = track;
      pkts[i].pts = pkts[i].dts = ts_us;
      ts_us += time_incr_us + pattern * 1000;
      if(fmt[track].es_type == VC_CONTAINER_ES_TYPE_VIDEO && !(counters[track] % 5))
         pkts[i].flags |= VC_CONTAINER_PACKET_FLAG_KEYFRAME;
      pkts[i].flags |= VC_CONTAINER_PACKET_FLAG_FRAME;
      counters[track]++;
   }
}

static int test_mp4(void)
{
   static const int64_t TS_OFFSET_US = 3000000;
   int ret;
   DECLARE_ES_FORMATS(fmts, 2, VC_CONTAINER_ES_FORMAT_FLAG_FRAMED);
   VC_CONTAINER_PACKET_T pkts[100] = {{0}};

   /* Test muxing / demuxing of H264+AAC */
   set_video_format(fmts, VC_CONTAINER_CODEC_H264, VC_CONTAINER_VARIANT_H264_AVC1,
      1920, 1080, true);
   set_audio_format(fmts + 1, VC_CONTAINER_CODEC_MP4A, 2, 48000, true);

   fill_packets(pkts, 100, fmts, 2, TS_OFFSET_US);

   ret = generate_container("test-h264-aac.mp4", 2, fmts, 100, pkts, true);
   if (!ret)
      ret = verify_container("test-h264-aac.mp4", 2, fmts, 100, pkts, TS_OFFSET_US, true);
   if (ret)
      return ret;

   /* Test muxing / demuxing of H265+OPUS */
   set_video_format(fmts, VC_CONTAINER_CODEC_H265, VC_CONTAINER_VARIANT_H265_HVC1,
      1920, 1080, true);
   set_audio_format(fmts + 1, VC_CONTAINER_CODEC_OPUS, 2, 48000, true);

   ret = generate_container("test-h265-opus.mp4", 2, fmts, 100, pkts, true);
   if (!ret)
      ret = verify_container("test-h265-opus.mp4", 2, fmts, 100, pkts, TS_OFFSET_US, true);

   return ret;
}
