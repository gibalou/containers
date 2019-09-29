/*
Copyright (c) 2012, Broadcom Europe Ltd
Copyright (c) 2019, Gildas Bazin
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//#define ENABLE_CONTAINERS_LOG_FORMAT_VERBOSE

#include "containers/core/containers_private.h"
#include "containers/core/containers_io_helpers.h"
#include "containers/core/containers_utils.h"
#include "containers/core/containers_logging.h"
#include "containers/core/containers_bits.h"

static FILE *myfile;

/******************************************************************************
Defines.
******************************************************************************/
#define MAX_TRACKS 4

/******************************************************************************
Type definitions
******************************************************************************/
typedef struct FSV_HEADER_T
{
   int32_t version;
   char video_codec_name[32];
   char video_fmtp[128];
   uint32_t audio_rate;
   uint32_t audio_ptime;
   uint64_t created;
   int channels;
} FSV_HEADER_T;

typedef struct FSV_RTP_HEADER_T
{
   unsigned cc:4;                          /* CSRC count             */
   unsigned x:1;                           /* header extension flag  */
   unsigned p:1;                           /* padding flag           */
   unsigned version:2;                     /* protocol version       */
   unsigned pt:7;                          /* payload type           */
   unsigned m:1;                           /* marker bit             */
   unsigned seq:16;                        /* sequence number        */
   unsigned ts:32;                         /* timestamp              */
   unsigned ssrc:32;                       /* synchronization source */
} FSV_RTP_HEADER_T;

typedef struct FSV_PACKET_STATE_T
{
   unsigned int track_num;
   unsigned int flags;

   uint64_t metadata_offset; /* Offset in metadata stream */
   uint32_t data_size;       /* Size of current data packet */
   uint32_t data_left;       /* Data left to read in current packet */

   int64_t pts;

} FSV_PACKET_STATE_T;

typedef struct VC_CONTAINER_TRACK_MODULE_T
{
   FSV_PACKET_STATE_T *state;
   FSV_PACKET_STATE_T local_state;

   VC_CONTAINER_IO_T *io;
   uint64_t data_offset;     /* Current offset in data stream */

} VC_CONTAINER_TRACK_MODULE_T;

typedef struct VC_CONTAINER_MODULE_T
{
   FSV_HEADER_T header;

   VC_CONTAINER_TRACK_T *tracks[MAX_TRACKS];

   int64_t metadata_offset;

   /* Shared packet state. This is used when the tracks are in sync,
    * and for the track at the earliest position in the file when they are
    * not in sync */
   FSV_PACKET_STATE_T state;

} VC_CONTAINER_MODULE_T;

/******************************************************************************
Function prototypes
******************************************************************************/
VC_CONTAINER_STATUS_T fsv_reader_open( VC_CONTAINER_T * );

/******************************************************************************
Local Functions
******************************************************************************/
static VC_CONTAINER_STATUS_T fsv_read_header( VC_CONTAINER_T *ctx )
{
   VC_CONTAINER_MODULE_T *module = ctx->priv->module;
   return VC_CONTAINER_ERROR_CORRUPTED;
}

enum
{
   /* 0 unspecified */
   NAL_UNIT_NON_IDR = 1,
   NAL_UNIT_PARTITION_A = 2,
   NAL_UNIT_PARTITION_B = 3,
   NAL_UNIT_PARTITION_C = 4,
   NAL_UNIT_IDR = 5,
   NAL_UNIT_SEI = 6,
   NAL_UNIT_SEQUENCE_PARAMETER_SET = 7,
   NAL_UNIT_PICTURE_PARAMETER_SET = 8,
   NAL_UNIT_ACCESS_UNIT_DELIMITER = 9,
   NAL_UNIT_END_OF_SEQUENCE = 10,
   NAL_UNIT_END_OF_STREAM = 11,
   NAL_UNIT_FILLER = 12,
   NAL_UNIT_EXT_SEQUENCE_PARAMETER_SET = 13,
   NAL_UNIT_PREFIX = 14,
   NAL_UNIT_SUBSET_SEQUENCE_PARAMETER_SET = 15,
   /* 16 to 18 reserved */
   NAL_UNIT_AUXILIARY = 19,
   NAL_UNIT_EXTENSION = 20,
   /* 21 to 23 reserved */
   NAL_UNIT_STAP_A = 24,
   NAL_UNIT_STAP_B = 25,
   NAL_UNIT_MTAP16 = 26,
   NAL_UNIT_MTAP24 = 27,
   NAL_UNIT_FU_A = 28,
   NAL_UNIT_FU_B = 29,
   /* 30 to 31 unspecified */
};

static void fsv_read_rtp_packet_header(VC_CONTAINER_T *ctx,
   const uint8_t *packet, uint32_t size)
{
   VC_CONTAINER_BITS_T bits;
   uint32_t version, has_padding, has_extension, csrc_count, has_marker;
   uint32_t payload_type, ssrc, timestamp;
   uint16_t seq_num;

   BITS_INIT(ctx, &bits, packet, size);

   /* Break down fixed header area into component parts */
   version              = BITS_READ_U32(ctx, &bits, 2, "Version");
   has_padding          = BITS_READ_U32(ctx, &bits, 1, "Has padding");
   has_extension        = BITS_READ_U32(ctx, &bits, 1, "Has extension");
   csrc_count           = BITS_READ_U32(ctx, &bits, 4, "CSRC count");
   has_marker           = BITS_READ_U32(ctx, &bits, 1, "Has marker");
   payload_type         = BITS_READ_U32(ctx, &bits, 7, "Payload type");
   seq_num              = BITS_READ_U16(ctx, &bits, 16, "Sequence number");
   timestamp            = BITS_READ_U32(ctx, &bits, 32, "Timestamp");
   ssrc                 = BITS_READ_U32(ctx, &bits, 32, "SSRC");

uint8_t startcode[] = {0, 0, 0, 1};

   BITS_READ_U32(ctx, &bits, 1, "forbidden_zero_bit");
   BITS_READ_U32(ctx, &bits, 2, "nal_ref_idc");
   uint32_t ptype = BITS_READ_U32(ctx, &bits, 5, "nal_unit_type");
   if (ptype < 23) {
   fwrite (startcode, 1, 4, myfile);
   fwrite (packet + 12, 1, size - 12, myfile);
   }
   if (ptype == 28) {
     uint32_t first = BITS_READ_U32(ctx, &bits, 1, "start_bit");
     BITS_READ_U32(ctx, &bits, 1, "end_bit");
     if (first) {
       BITS_READ_U32(ctx, &bits, 1, "reserved_bit");
       BITS_READ_U32(ctx, &bits, 5, "FU-A nal_unit_type");
       fwrite (startcode, 1, 4, myfile);
       fwrite (packet + 13, 1, size - 13, myfile);
     }
     else {
       fwrite (packet + 14, 1, size - 14, myfile);
     }
   }
}

/*****************************************************************************
Functions exported as part of the Container Module API
 *****************************************************************************/
static VC_CONTAINER_STATUS_T fsv_reader_read( VC_CONTAINER_T *ctx,
   VC_CONTAINER_PACKET_T *packet, uint32_t flags )
{
   VC_CONTAINER_PARAM_UNUSED(ctx);
   VC_CONTAINER_PARAM_UNUSED(packet);
   VC_CONTAINER_PARAM_UNUSED(flags);
   return VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T fsv_reader_seek( VC_CONTAINER_T *ctx, int64_t *offset,
   VC_CONTAINER_SEEK_MODE_T mode, VC_CONTAINER_SEEK_FLAGS_T flags)
{
   VC_CONTAINER_PARAM_UNUSED(ctx);
   VC_CONTAINER_PARAM_UNUSED(offset);
   VC_CONTAINER_PARAM_UNUSED(mode);
   VC_CONTAINER_PARAM_UNUSED(flags);
   return VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T fsv_reader_close( VC_CONTAINER_T *ctx )
{
   VC_CONTAINER_MODULE_T *module = ctx->priv->module;

   for (; ctx->tracks_num > 0; ctx->tracks_num--)
   {
      VC_CONTAINER_TRACK_T *track = ctx->tracks[ctx->tracks_num-1];
      if (track->priv->module->io)
         vc_container_io_close(track->priv->module->io);
      vc_container_free_track(ctx, track);
   }

   free(module);
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T fsv_reader_open( VC_CONTAINER_T *ctx )
{
   const char *extension = vc_uri_path_extension(ctx->priv->uri);
   VC_CONTAINER_MODULE_T *module = 0;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_ERROR_FORMAT_INVALID;
   uint8_t buffer[4];
   unsigned int i;

   /* Check if the user has specified a container */
   vc_uri_find_query(ctx->priv->uri, 0, "container", &extension);

   /* Since FSV is difficult to auto-detect, we use the extension as
      part of the autodetection */
   if(!extension)
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   if(strcasecmp(extension, "fsv"))
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   LOG_DEBUG(ctx, "using fsv reader");

myfile = fopen("/tmp/dump.h264", "w");

   /* Allocate our context */
   module = malloc(sizeof(*module));
   if (!module) return VC_CONTAINER_ERROR_OUT_OF_MEMORY;
   memset(module, 0, sizeof(*module));
   ctx->priv->module = module;
   ctx->tracks = module->tracks;

   LOG_FORMAT(ctx, "version: %u", module->header.version);
   READ_BYTES(ctx, &module->header, sizeof(module->header));
   module->header.video_codec_name[sizeof(module->header.video_codec_name)-1] = 0;
   module->header.video_fmtp[sizeof(module->header.video_fmtp)-1] = 0;
   LOG_FORMAT(ctx, "video_codec_name: %s", module->header.video_codec_name);
   LOG_FORMAT(ctx, "video_fmtp: %s", module->header.video_fmtp);
   LOG_FORMAT(ctx, "audio_rate: %u", module->header.audio_rate);
   LOG_FORMAT(ctx, "audio_ptime: %u", module->header.audio_ptime);
   LOG_FORMAT(ctx, "created: %llu", module->header.created);
   LOG_FORMAT(ctx, "channels: %u", module->header.channels);

#define VID_BIT (1 << 31)
 for (i = 0; i < 10000; i++)
{
   uint32_t packet[4*1024];
   uint32_t fh = READ_U32(ctx, "frame header");
   LOG_FORMAT(ctx, "frame type %s", fh & VID_BIT ? "video" : "audio");
   if (!(fh & VID_BIT)) {
READ_BYTES(ctx, packet, (fh & ~VID_BIT) + 8);
continue;
   }
   LOG_FORMAT(ctx, "frame size %u", fh & ~VID_BIT);
   READ_BYTES(ctx, packet, (fh & ~VID_BIT) + 8);
   if (fh & VID_BIT)
     fsv_read_rtp_packet_header(ctx, packet, fh & ~VID_BIT);
}

fclose(myfile);
   /*
    *  We now have all the information we really need to start playing the stream
    */

   module->metadata_offset = STREAM_POSITION(ctx);

   /* Initialise state for all tracks */
   module->state.metadata_offset = module->metadata_offset;
   for (i = 0; i < ctx->tracks_num; i++)
   {
      VC_CONTAINER_TRACK_T *track = ctx->tracks[i];
      track->priv->module->state = &module->state;
   }

   /* Look for the codec configuration data for each track so
    * we can store it in the track format */
   for (i = 0; i < ctx->tracks_num; i++)
   {
      VC_CONTAINER_TRACK_T *track = ctx->tracks[i];
      VC_CONTAINER_PACKET_T packet;
      packet.track = i;
      status = VC_CONTAINER_ERROR_CONTINUE;

      while (status == VC_CONTAINER_ERROR_CONTINUE)
         status = fsv_reader_read(ctx, &packet,
            VC_CONTAINER_READ_FLAG_INFO | VC_CONTAINER_READ_FLAG_FORCE_TRACK);
      if (status != VC_CONTAINER_SUCCESS)
         continue;

      status = vc_container_track_allocate_extradata(ctx, track, packet.size);
      if (status != VC_CONTAINER_SUCCESS)
         continue;

      packet.data = track->format->extradata;
      packet.buffer_size = packet.size;
      packet.size = 0;
      status = fsv_reader_read(ctx, &packet,
         VC_CONTAINER_READ_FLAG_FORCE_TRACK);
      if (status != VC_CONTAINER_SUCCESS)
         continue;

      track->format->extradata_size = packet.size;
   }

   ctx->priv->pf_close = fsv_reader_close;
   ctx->priv->pf_read = fsv_reader_read;
   ctx->priv->pf_seek = fsv_reader_seek;
   return VC_CONTAINER_SUCCESS;

 error:
   LOG_ERROR(ctx, "fsv: error opening stream (%i)", status);
   fsv_reader_close(ctx);
   return status;
}

/********************************************************************************
 Entrypoint function
 ********************************************************************************/

#if !defined(ENABLE_CONTAINERS_STANDALONE) && defined(__HIGHC__)
# pragma weak reader_open fsv_reader_open
#endif
