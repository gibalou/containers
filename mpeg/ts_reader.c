/*
Copyright (c) 2015, Gildas Bazin
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
#include <string.h>

#define CONTAINER_IS_BIG_ENDIAN
//#define ENABLE_CONTAINERS_LOG_FORMAT
//#define ENABLE_CONTAINERS_LOG_FORMAT_VERBOSE
#include "core/containers_bits.h"
#include "core/containers_private.h"
#include "core/containers_io_helpers.h"
#include "core/containers_utils.h"
#include "core/containers_logging.h"
#undef CONTAINER_HELPER_LOG_INDENT
#define CONTAINER_HELPER_LOG_INDENT(a) (4*(a)->priv->module->level)
#include "core/containers_bits.h"

/******************************************************************************
Defines.
******************************************************************************/
#define COUNT_OF(a) (sizeof(a)/sizeof((a)[0]))

#define TS_PROBE_PACKETS_NUM 16
#define TS_PROBE_PACKETS_NUM_MIN 2
#define TS_PROBE_BYTES_MAX 65536

#define TS_PID_MAX 8192
#define TS_PMT_MAX 32 /* Arbitrary */
#define TS_TRACKS_MAX 2

#define TS_SYNC_FAIL_MAX 65536 /** Maximum number of byte-wise sync attempts,
                                   should be enough to stride at least one
                                   PES packet (length encoded using 16 bits). */

/** Maximum number of pack/packet start codes scanned when searching for tracks
    at open time or when resyncing. */
#define TS_PACK_SCAN_MAX 128

#define TID_PAT 0
#define TID_PMT 2

/******************************************************************************
Type definitions.
******************************************************************************/
typedef struct TS_PAT_T
{
   uint16_t programs_num;
   uint16_t program[TS_PID_MAX];
   uint16_t pid[TS_PID_MAX];
   bool valid;
   uint8_t version;
} TS_PAT_T;

typedef struct VC_CONTAINER_STATE_T
{
   TS_PAT_T pat;
} VC_CONTAINER_STATE_T;

typedef struct VC_CONTAINER_TRACK_MODULE_T
{
   /** Coding and elementary stream id of the track */
   uint32_t stream_id;

} VC_CONTAINER_TRACK_MODULE_T;

typedef struct VC_CONTAINER_MODULE_T
{
   /** Logging indentation level */
   uint32_t level;

   /** Track data */
   int tracks_num;
   VC_CONTAINER_TRACK_T *tracks[TS_TRACKS_MAX];

   int64_t data_offset;
   unsigned int packet_size;

   VC_CONTAINER_TRACK_T *pid_map[TS_PID_MAX];

   VC_CONTAINER_STATE_T state;

} VC_CONTAINER_MODULE_T;

/******************************************************************************
Function prototypes
******************************************************************************/
VC_CONTAINER_STATUS_T ts_reader_open( VC_CONTAINER_T * );

/******************************************************************************
Prototypes for local functions
******************************************************************************/

/******************************************************************************
Local Functions
******************************************************************************/

/*****************************************************************************/
static VC_CONTAINER_STATUS_T ts_probe( VC_CONTAINER_T *ctx, unsigned int *size)
{
   static const int packet_size[] = {188, 192, 204};
   int64_t offset, start = STREAM_POSITION(ctx);
   bool found = false;
   unsigned int i = 0, j = 0;
   uint8_t byte;

   do
   {
      /* Finding the very first start code */
      while((byte = _READ_U8(ctx)) != 0x47 &&
            STREAM_STATUS(ctx) == VC_CONTAINER_SUCCESS &&
            STREAM_POSITION(ctx) - start < TS_PROBE_BYTES_MAX);

      LOG_DEBUG(ctx, "found 1st packet at %"PRId64, STREAM_POSITION(ctx)-1);

      if(byte != 0x47)
         break; /* No start code found */

      offset = STREAM_POSITION(ctx) - 1;

      /* Look for further start codes at the specified intervals */
      for(j = 0; j < sizeof(packet_size)/sizeof(packet_size[0]); j++)
      {
         LOG_DEBUG(ctx, "trying for %i", packet_size[j]);
         for(; STREAM_STATUS(ctx) == VC_CONTAINER_SUCCESS && i < TS_PROBE_PACKETS_NUM; i++)
         {
            SEEK(ctx, STREAM_POSITION(ctx) + packet_size[j]-1);
            if(_READ_U8(ctx) != 0x47)
            {
               LOG_DEBUG(ctx, "not a start code at %"PRId64" (%i)", STREAM_POSITION(ctx)-1, i);
               break;
            }
         }

         found = i == TS_PROBE_PACKETS_NUM || (i >= TS_PROBE_PACKETS_NUM_MIN && STREAM_EOS(ctx));
         SEEK(ctx, offset + (!found ? 1 : 0));

         if (found)
            break;
      }

   } while(!found);

   LOG_DEBUG(ctx, "found %i packets of size %i at offset %"PRId64, i, packet_size[j], offset);
   *size = packet_size[j];
   return found ? VC_CONTAINER_SUCCESS : VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T ts_read_pat( VC_CONTAINER_T *ctx,
   VC_CONTAINER_STATE_T *state, uint8_t *buffer, unsigned int size )
{
   VC_CONTAINER_BITS_T bits;
   unsigned int section_length, version;
   uint16_t program, pid;
   BITS_INIT(ctx, &bits, buffer, size);
LOG_DEBUG(ctx, "buffer %2.2x,%2.2x,%2.2x,%2.2x", buffer[0], buffer[1],
 buffer[2], buffer[3]);

   LOG_FORMAT(ctx, "PAT (size %u)", size);
   ctx->priv->module->level++;

   if (BITS_READ_UINT(ctx, &bits, 8, "table_id") != TID_PAT)
      goto error;
   BITS_SKIP_UINT(ctx, &bits, 1, "section_syntax_indicator");
   if (BITS_READ_UINT(ctx, &bits, 1, "marker"))
      goto error;
   BITS_SKIP_UINT(ctx, &bits, 2, "reserved");
   section_length = BITS_READ_UINT(ctx, &bits, 12, "section_length");
   if (section_length > 0x3FD)
      goto error;
   if (section_length > BITS_BYTES_AVAILABLE(ctx, &bits))
   {
      LOG_ERROR(ctx, "PAT buffer too small (%i/%i)", section_length, BITS_BYTES_AVAILABLE(ctx, &bits));
      goto error;
   }
   BITS_SKIP_UINT(ctx, &bits, 16, "transport_stream_id");
   BITS_SKIP_UINT(ctx, &bits, 2, "reserved");
   version = BITS_READ_UINT(ctx, &bits, 5, "version_number");
   BITS_SKIP_UINT(ctx, &bits, 1, "current_next_indicator");
   BITS_SKIP_UINT(ctx, &bits, 8, "section_number");
   BITS_SKIP_UINT(ctx, &bits, 8, "last_section_number");

   /* If we already have this PAT, we can safely ignore it */
   if (state->pat.valid && state->pat.version == version)
      goto skip;

   for (section_length -= 5; section_length >= 8; section_length -= 4)
   {
      program = BITS_READ_UINT(ctx, &bits, 16, "program_number");

      ctx->priv->module->level++;
      BITS_SKIP_UINT(ctx, &bits, 3, "reserved");
      if (!program)
         pid = BITS_READ_UINT(ctx, &bits, 13, "network_PID");
      else
         pid = BITS_READ_UINT(ctx, &bits, 13, "program_map_PID");
      ctx->priv->module->level--;

      if (pid == 0x1fff)
         continue;
      if (state->pat.programs_num >= COUNT_OF(state->pat.program))
      {
         LOG_ERROR(ctx, "too many programs in PAT, discarding %x/%x", program, pid);
         continue;
      }
      LOG_DEBUG(ctx, "adding program %x/%x", program, pid);
      state->pat.program[state->pat.programs_num] = program;
      state->pat.pid[state->pat.programs_num++] = pid;
   }
LOG_DEBUG(ctx, "section length %i", section_length);
   if (section_length != 4)
      goto error;
   BITS_SKIP_UINT(ctx, &bits, 32, "CRC_32");

 skip:
   ctx->priv->module->level--;
   return VC_CONTAINER_SUCCESS;
 error:
   ctx->priv->module->level--;
   LOG_ERROR(ctx, "corrupted PAT");
   return VC_CONTAINER_ERROR_CORRUPTED;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T ts_read_pmt( VC_CONTAINER_T *ctx,
   VC_CONTAINER_STATE_T *state, uint8_t *buffer, unsigned int size )
{
   VC_CONTAINER_BITS_T bits;
   unsigned int section_length, version;
   BITS_INIT(ctx, &bits, buffer, size);

   LOG_FORMAT(ctx, "PMT (size %u)", size);
   ctx->priv->module->level++;

   if (BITS_READ_UINT(ctx, &bits, 8, "table_id") != TID_PMT)
      goto error;
   BITS_SKIP_UINT(ctx, &bits, 1, "section_syntax_indicator");
   if (BITS_READ_UINT(ctx, &bits, 1, "marker"))
      goto error;
   BITS_SKIP_UINT(ctx, &bits, 2, "reserved");
   section_length = BITS_READ_UINT(ctx, &bits, 12, "section_length");
   if (section_length > 0x3FD)
      goto error;
   if (section_length > BITS_BYTES_AVAILABLE(ctx, &bits))
   {
      LOG_ERROR(ctx, "PMT buffer too small (%i/%i)", section_length, BITS_BYTES_AVAILABLE(ctx, &bits));
      goto error;
   }
   BITS_SKIP_UINT(ctx, &bits, 16, "program_number");
   BITS_SKIP_UINT(ctx, &bits, 2, "reserved");
   version = BITS_READ_UINT(ctx, &bits, 5, "version_number");
   BITS_SKIP_UINT(ctx, &bits, 1, "current_next_indicator");
   BITS_SKIP_UINT(ctx, &bits, 8, "section_number");
   BITS_SKIP_UINT(ctx, &bits, 8, "last_section_number");
   BITS_SKIP_UINT(ctx, &bits, 3, "reserved");
   BITS_SKIP_UINT(ctx, &bits, 13, "PCR_PID");
   BITS_SKIP_UINT(ctx, &bits, 4, "reserved");
   BITS_SKIP_UINT(ctx, &bits, 12, "program_info_length");

 skip:
   ctx->priv->module->level--;
   return VC_CONTAINER_SUCCESS;
 error:
   ctx->priv->module->level--;
   LOG_ERROR(ctx, "corrupted PMT");
   return VC_CONTAINER_ERROR_CORRUPTED;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T ts_read_packet( VC_CONTAINER_T *ctx, VC_CONTAINER_STATE_T *state )
{
   VC_CONTAINER_MODULE_T *module = ctx->priv->module;
   unsigned int adaptation_field_control, adaptation_field_length = 0;
   unsigned int pid;
   uint8_t p[4];

   READ_BYTES(ctx, p, 4);
   if(p[0] != 0x47)
   {
      LOG_ERROR(ctx, "invalid sync byte at offset %"PRId64, STREAM_POSITION(ctx)-4);
      return VC_CONTAINER_ERROR_CORRUPTED;
   }

   LOG_DEBUG(ctx, "transport_error_indicator: %i", p[1]>>7);
   LOG_DEBUG(ctx, "payload_unit_start_indicator: %i", (p[1]>>6)&0x1);
   LOG_DEBUG(ctx, "transport priority: %i", (p[1]>>5)&0x1);
   LOG_DEBUG(ctx, "PID: %x", ((p[1]&0x1F)<<8)|p[2]);
   pid = ((p[1]&0x1F)<<8)|p[2];
   LOG_DEBUG(ctx, "transport_scrambling_control: %i", p[3]>>6);
   LOG_DEBUG(ctx, "adaptation_field_control: %i", (p[3]>>4)&0x3);
   adaptation_field_control = (p[3]>>4)&0x3;
   LOG_DEBUG(ctx, "continuity_counter: %i", p[3]&0xF);
   if(adaptation_field_control == 0x2 || adaptation_field_control == 0x3)
   {
      adaptation_field_length = READ_U8(ctx, "adaptation_field_length") + 1;
      SKIP_BYTES(ctx, adaptation_field_length - 1);
      if (adaptation_field_control == 0x2)
      {
         SKIP_BYTES(ctx, module->packet_size - 4 - adaptation_field_length);
         return VC_CONTAINER_SUCCESS;
      }
   }

   /* PID 0 is reserved for the PAT */
   if (pid == 0)
   {
      uint8_t buffer[200];
      LOG_DEBUG(ctx, "found PAT section");
      READ_BYTES(ctx, buffer, module->packet_size - 4 - adaptation_field_length);
      return ts_read_pat( ctx, state, buffer, module->packet_size - 4 - adaptation_field_length );
   }

   /*  */
   if (pid == 0x1001)
   {
      uint8_t buffer[200];
      LOG_DEBUG(ctx, "found PMT section");
      READ_BYTES(ctx, buffer, module->packet_size - 4 - adaptation_field_length);
      return ts_read_pmt( ctx, state, buffer, module->packet_size - 4 - adaptation_field_length );
   }

   SKIP_BYTES(ctx, module->packet_size - 4 - adaptation_field_length);

#if 0
   adaptation_field() {
   adaptation_field_length 8 uimsbf
   if (adaptation_field_length > 0) {
   discontinuity_indicator 1 bslbf
   random_access_indicator 1 bslbf
   elementary_stream_priority_indicator 1 bslbf
   PCR_flag 1 bslbf
   OPCR_flag 1 bslbf
   splicing_point_flag 1 bslbf
   transport_private_data_flag 1 bslbf
   adaptation_field_extension_flag 1 bslbf
   if (PCR_flag = = '1') {
   program_clock_reference_base 33 uimsbf
   reserved 6 bslbf
   program_clock_reference_extension 9 uimsbf
   }
   if (OPCR_flag = = '1') {
   original_program_clock_reference_base 33 uimsbf
   reserved 6 bslbf
   original_program_clock_reference_extension 9 uimsbf
   }
   if (splicing_point_flag = = '1') {
   splice_countdown 8 tcimsbf
   }
   if (transport_private_data_flag = = '1') {
   transport_private_data_length 8 uimsbf
   for (i = 0; i < transport_private_data_length; i++) {
   private_data_byte 8 bslbf
   }
   }
   if (adaptation_field_extension_flag = = '1') {
   adaptation_field_extension_length 8 uimsbf
   ltw_flag 1 bslbf
   piecewise_rate_flag 1 bslbf
   seamless_splice_flag 1 bslbf
   reserved 5 bslbf
   if (ltw_flag = = '1') {
   ltw_valid_flag 1 bslbf
   ltw_offset 15 uimsbf
   }
   if (piecewise_rate_flag = = '1') {
   reserved 2 bslbf
   piecewise_rate 22 uimsbf
   }
   if (seamless_splice_flag = = '1') {
   splice_type 4 bslbf
   DTS_next_AU[32..30] 3 bslbf
   marker_bit 1 bslbf
   DTS_next_AU[29..15] 15 bslbf
   marker_bit 1 bslbf
   DTS_next_AU[14..0] 15 bslbf
   marker_bit 1 bslbf
   }
   for (i = 0; i < N; i++) {
   reserved 8 bslbf
   }
   }
   for (i = 0; i < N; i++) {
   stuffing_byte 8 bslbf
   }
   }
   }
 #endif
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************
Functions exported as part of the Container Module API
*****************************************************************************/

/*****************************************************************************/
static VC_CONTAINER_STATUS_T ts_reader_read( VC_CONTAINER_T *ctx,
   VC_CONTAINER_PACKET_T *p_packet, uint32_t flags )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_MODULE_T *module = ctx->priv->module;
   VC_CONTAINER_PARAM_UNUSED(module);
   VC_CONTAINER_PARAM_UNUSED(p_packet);
   VC_CONTAINER_PARAM_UNUSED(flags);

   while(ts_read_packet(ctx, &module->state) == VC_CONTAINER_SUCCESS);

   return status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T ts_reader_seek( VC_CONTAINER_T *ctx,
   int64_t *p_offset, VC_CONTAINER_SEEK_MODE_T mode, VC_CONTAINER_SEEK_FLAGS_T flags )
{
   VC_CONTAINER_MODULE_T *module = ctx->priv->module;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   VC_CONTAINER_PARAM_UNUSED(module);
   VC_CONTAINER_PARAM_UNUSED(p_offset);
   VC_CONTAINER_PARAM_UNUSED(flags);

   if(mode != VC_CONTAINER_SEEK_MODE_TIME || !STREAM_SEEKABLE(ctx))
      return VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION;

   return status;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T ts_reader_close( VC_CONTAINER_T *ctx )
{
   VC_CONTAINER_MODULE_T *module = ctx->priv->module;
   unsigned int i;

   for(i = 0; i < ctx->tracks_num; i++)
      vc_container_free_track(ctx, ctx->tracks[i]);
   free(module);
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T ts_reader_open( VC_CONTAINER_T *ctx )
{
   const char *extension = vc_uri_path_extension(ctx->priv->uri);
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   VC_CONTAINER_MODULE_T *module = 0;
   unsigned int packet_size = 188;

   /* Check if the user has specified a container */
   vc_uri_find_query(ctx->priv->uri, 0, "container", &extension);

   /* Since MPEG is difficult to auto-detect, we use the extension as
      part of the autodetection */
   if(!extension)
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   if(strcasecmp(extension, "ts") && strcasecmp(extension, "mts"))
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   if((status = ts_probe(ctx, &packet_size)) != VC_CONTAINER_SUCCESS)
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;  /* We didn't find a valid TS packet  */

   LOG_INFO(ctx, "using ts reader");


   /* Need to allocate context before searching for streams */
   module = malloc(sizeof(*module));
   if(!module) { status = VC_CONTAINER_ERROR_OUT_OF_MEMORY; goto error; }
   memset(module, 0, sizeof(*module));
   ctx->priv->module = module;
   ctx->tracks = module->tracks;
   module->packet_size = packet_size;

   /* Store offset so we can get back to what we consider the first pack or
      packet */
   module->data_offset = STREAM_POSITION(ctx);

   if(STREAM_SEEKABLE(ctx)) ctx->capabilities |= VC_CONTAINER_CAPS_CAN_SEEK;

   ctx->priv->pf_close = ts_reader_close;
   ctx->priv->pf_read = ts_reader_read;
   ctx->priv->pf_seek = ts_reader_seek;

   return STREAM_STATUS(ctx);

 error:
   LOG_DEBUG(ctx, "ts: error opening stream (%i)", status);
   if(module) ts_reader_close(ctx);
   return status;
}

/********************************************************************************
 Entrypoint function
 ********************************************************************************/

#if !defined(ENABLE_CONTAINERS_STANDALONE) && defined(__HIGHC__)
# pragma weak reader_open ts_reader_open
#endif
