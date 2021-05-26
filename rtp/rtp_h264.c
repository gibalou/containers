/*
Copyright (c) 2012, Broadcom Europe Ltd
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
#include <stdio.h>
#include <string.h>

#include "containers.h"

#include "core/containers_logging.h"
#include "core/containers_list.h"
#include "core/containers_bits.h"
#include "rtp_priv.h"
#include "rtp_base64.h"
#include "rtp_h264.h"

/******************************************************************************
Defines and constants.
******************************************************************************/

/** H.264 payload flag bits */
typedef enum
{
   H264F_NEXT_PACKET_IS_START = 0,
   H264F_INSIDE_FRAGMENT,
   H264F_OUTPUT_NAL_HEADER,
} h264_flag_bit_t;

/** Bit mask to extract F zero bit from NAL unit header */
#define NAL_UNIT_FZERO_MASK 0x80
/** Bit mask to extract NAL unit type from NAL unit header */
#define NAL_UNIT_TYPE_MASK 0x1F

/** NAL unit type codes */
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

/** Fragment unit header indicator bits */
typedef enum
{
   FRAGMENT_UNIT_HEADER_RESERVED = 5,
   FRAGMENT_UNIT_HEADER_END = 6,
   FRAGMENT_UNIT_HEADER_START = 7,
} fragment_unit_header_bit_t;

/** H.264 RTP timestamp clock rate */
#define H264_TIMESTAMP_CLOCK    90000

/******************************************************************************
Type definitions
******************************************************************************/

typedef struct h264_payload_tag
{
   uint32_t nal_unit_size;          /**< Number of NAL unit bytes left to write */
   uint8_t flags;                   /**< H.264 payload flags */
   uint8_t header_bytes_to_write;   /**< Number of start code bytes left to write */
   uint8_t nal_header;              /**< Header for next NAL unit */
} H264_PAYLOAD_T;

/******************************************************************************
Function prototypes
******************************************************************************/
VC_CONTAINER_STATUS_T h264_parameter_handler(VC_CONTAINER_T *p_ctx,
      VC_CONTAINER_TRACK_T *track, const VC_CONTAINERS_LIST_T *params);

/******************************************************************************
Local Functions
******************************************************************************/

/**************************************************************************//**
 * Remove emulation prevention bytes from a buffer.
 * These are 0x03 bytes inserted to prevent misinterprentation of a byte
 * sequence in a buffer as a start code.
 *
 * @param sprop      The buffer from which bytes are to be removed.
 * @param sprop_size The number of bytes in the buffer.
 * @return  The new number of bytes in the buffer.
 */
static uint32_t h264_remove_emulation_prevention_bytes(uint8_t *sprop,
      uint32_t sprop_size)
{
   uint32_t offset = 0;
   uint8_t nal_unit_type = sprop[offset++];
   uint32_t new_sprop_size = sprop_size;
   uint8_t first_byte, second_byte;

   nal_unit_type &= 0x1F;  /* Just keep NAL unit type bits */

   /* Certain NAL unit types need a byte triplet passed first */
   if (nal_unit_type == NAL_UNIT_PREFIX || nal_unit_type == NAL_UNIT_EXTENSION)
      offset += 3;

   /* Make sure there is enough data for there to be a 0x00 0x00 0x03 sequence */
   if (offset + 2 >= new_sprop_size)
      return new_sprop_size;

   /* Keep a rolling set of the last couple of bytes */
   first_byte = sprop[offset++];
   second_byte = sprop[offset++];

   while (offset < new_sprop_size)
   {
      uint8_t next_byte = sprop[offset];

      if (!first_byte && !second_byte && next_byte == 0x03)
      {
         /* Remove the emulation prevention byte (0x03) */
         new_sprop_size--;
         if (offset == new_sprop_size) /* No more data to check */
            break;
         memmove(&sprop[offset], &sprop[offset + 1], new_sprop_size - offset);
         next_byte = sprop[offset];
      } else
         offset++;

      first_byte = second_byte;
      second_byte = next_byte;
   }

   return new_sprop_size;
}

/**************************************************************************//**
 * Decode the sprop parameter sets URI parameter and update track information.
 *
 * @param p_ctx   The RTP container context.
 * @param track   The track to be updated.
 * @param params  The URI parameter list.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T h264_get_sprop_parameter_sets(VC_CONTAINER_T *p_ctx,
      VC_CONTAINER_TRACK_T *track,
      const VC_CONTAINERS_LIST_T *params)
{
   VC_CONTAINER_STATUS_T status;
   PARAMETER_T param;
   size_t str_len;
   uint32_t extradata_size = 0;
   uint8_t *sprop;
   const char *set;
   const char *comma;

   /* Get the value of sprop-parameter-sets, base64 decode the (comma separated)
    * sets, store all of them in track->priv->extradata and also decode to
    * validate and fill in video format info. */

   param.name = "sprop-parameter-sets";
   if (!vc_containers_list_find_entry(params, &param) || !param.value)
   {
      LOG_ERROR(p_ctx, "H.264: sprop-parameter-sets is required, but not found");
      return VC_CONTAINER_ERROR_FORMAT_INVALID;
   }

   /* First pass, calculate total size of buffer needed */
   set = param.value;
   do {
      comma = strchr(set, ',');
      str_len = comma ? (size_t)(comma - set) : strlen(set);
      /* Allow space for the NAL unit and a start code */
      extradata_size += rtp_base64_byte_length(set, str_len) + 4;
      set = comma + 1;
   } while (comma);

   if (!extradata_size)
   {
      LOG_ERROR(p_ctx, "H.264: sprop-parameter-sets doesn't contain useful data");
      return VC_CONTAINER_ERROR_FORMAT_INVALID;
   }

   status = vc_container_track_allocate_extradata(p_ctx, track, extradata_size);
   if(status != VC_CONTAINER_SUCCESS) return status;

   track->format->extradata_size = extradata_size;
   sprop = track->priv->extradata;

   /* Now decode the data into the buffer */
   set = param.value;
   do {
      uint8_t *next_sprop;
      uint32_t sprop_size;

      comma = strchr(set, ',');
      str_len = comma ? (size_t)(comma - set) : strlen(set);

      /* Insert a start code (0x00000001 in network order) */
      *sprop++ = 0x00; *sprop++ = 0x00; *sprop++ = 0x00; *sprop++ = 0x01;
      extradata_size -= 4;

      next_sprop = rtp_base64_decode(set, str_len, sprop, extradata_size);
      if (!next_sprop)
      {
         LOG_ERROR(p_ctx, "H.264: sprop-parameter-sets failed to decode");
         return VC_CONTAINER_ERROR_FORMAT_INVALID;
      }

      sprop_size = next_sprop - sprop;
      if (sprop_size)
      {
         uint32_t new_sprop_size;

         /* Need to remove emulation prevention bytes before decoding */
         new_sprop_size = h264_remove_emulation_prevention_bytes(sprop, sprop_size);

         /* If necessary, decode sprop again, to put back the emulation prevention bytes */
         if (new_sprop_size != sprop_size)
            rtp_base64_decode(set, str_len, sprop, sprop_size);

         extradata_size -= sprop_size;
         sprop = next_sprop;
      }

      set = comma + 1;
   } while (comma);

   return VC_CONTAINER_SUCCESS;
}

/**************************************************************************//**
 * Check URI parameter list for unsupported features.
 *
 * @param p_ctx   The RTP container context.
 * @param params  The URI parameter list.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T h264_check_unsupported_features(VC_CONTAINER_T *p_ctx,
      const VC_CONTAINERS_LIST_T *params)
{
   uint32_t u32_unused;

   /* Limitation: interleaving not yet supported */
   if (rtp_get_parameter_u32(params, "sprop-interleaving-depth", &u32_unused) ||
         rtp_get_parameter_u32(params, "sprop-deint-buf-req", &u32_unused) ||
         rtp_get_parameter_u32(params, "sprop-init-buf-time", &u32_unused) ||
         rtp_get_parameter_u32(params, "sprop-max-don-diff", &u32_unused))
   {
      LOG_ERROR(p_ctx, "H.264: Interleaved packetization is not supported");
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   }

   return VC_CONTAINER_SUCCESS;
}

/**************************************************************************//**
 * Get and check the packetization mode URI parameter.
 *
 * @param p_ctx   The RTP container context.
 * @param params  The URI parameter list.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T h264_get_packetization_mode(VC_CONTAINER_T *p_ctx,
      const VC_CONTAINERS_LIST_T *params)
{
   uint32_t packetization_mode;

   if (rtp_get_parameter_u32(params, "packetization-mode", &packetization_mode))
   {
      /* Only modes 0 and 1 are supported, no interleaving */
      if (packetization_mode > 1)
      {
         LOG_ERROR(p_ctx, "H.264: Unsupported packetization mode: %u", packetization_mode);
         return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
      }
   }

   return VC_CONTAINER_SUCCESS;
}

/**************************************************************************//**
 * Initialise payload bit stream for a new RTP packet.
 *
 * @param p_ctx      The RTP container context.
 * @param t_module   The track module with the new RTP packet.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T h264_new_rtp_packet(VC_CONTAINER_T *p_ctx,
      VC_CONTAINER_TRACK_MODULE_T *t_module)
{
   VC_CONTAINER_BITS_T *payload = &t_module->payload;
   H264_PAYLOAD_T *extra = (H264_PAYLOAD_T *)t_module->extra;
   uint8_t unit_header;
   uint8_t fragment_header;

   /* Read the NAL unit type and process as necessary */
   unit_header = BITS_READ_U8(p_ctx, payload, 8, "nal_unit_header");

   /* When the top bit is set, the NAL unit is invalid */
   if (unit_header & NAL_UNIT_FZERO_MASK)
   {
      LOG_DEBUG(p_ctx, "H.264: Invalid NAL unit (top bit of header set)");
      return VC_CONTAINER_ERROR_FORMAT_INVALID;
   }

   /* In most cases, a new packet means a new NAL unit, which will need a start code and the header */
   extra->header_bytes_to_write = 5;
   extra->nal_header = unit_header;
   extra->nal_unit_size = BITS_BYTES_AVAILABLE(p_ctx, payload);

   switch (unit_header & NAL_UNIT_TYPE_MASK)
   {
   case NAL_UNIT_STAP_A:
      /* Single Time Aggregation Packet A */
      CLEAR_BIT(extra->flags, H264F_INSIDE_FRAGMENT);
      /* Trigger reading NAL unit length and header */
      extra->nal_unit_size = 0;
      break;

   case NAL_UNIT_FU_A:
      /* Fragementation Unit A */
      fragment_header = BITS_READ_U8(p_ctx, payload, 8, "fragment_header");
      extra->nal_unit_size--;

      if (BIT_IS_CLEAR(fragment_header, FRAGMENT_UNIT_HEADER_START) ||
            BIT_IS_SET(extra->flags, H264F_INSIDE_FRAGMENT))
      {
         /* This is a continuation packet, prevent start code and header from being output */
         extra->header_bytes_to_write = 0;

         /* If this is the end of a fragment, the next FU will be a new one */
         if (BIT_IS_SET(fragment_header, FRAGMENT_UNIT_HEADER_END))
            CLEAR_BIT(extra->flags, H264F_INSIDE_FRAGMENT);
      } else {
         /* Start of a new fragment. */
         SET_BIT(extra->flags, H264F_INSIDE_FRAGMENT);

         /* Merge type from fragment header and the rest from NAL unit header to form real NAL unit header */
         fragment_header &= NAL_UNIT_TYPE_MASK;
         fragment_header |= (unit_header & ~NAL_UNIT_TYPE_MASK);
         extra->nal_header = fragment_header;
      }
      break;

   case NAL_UNIT_STAP_B:
   case NAL_UNIT_MTAP16:
   case NAL_UNIT_MTAP24:
   case NAL_UNIT_FU_B:
      LOG_ERROR(p_ctx, "H.264: Unsupported RTP NAL unit type: %u", unit_header & NAL_UNIT_TYPE_MASK);
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   default:
      /* Single NAL unit case */
      CLEAR_BIT(extra->flags, H264F_INSIDE_FRAGMENT);
   }

   return VC_CONTAINER_SUCCESS;
}

/**************************************************************************//**
 * H.264 payload handler.
 * Extracts/skips data from the payload according to the NAL unit headers.
 *
 * @param p_ctx      The RTP container context.
 * @param track      The track being read.
 * @param p_packet   The container packet information, or NULL.
 * @param flags      The container read flags.
 * @return  The resulting status of the function.
 */
static VC_CONTAINER_STATUS_T h264_payload_handler(VC_CONTAINER_T *p_ctx,
      VC_CONTAINER_TRACK_T *track,
      VC_CONTAINER_PACKET_T *p_packet,
      uint32_t flags)
{
   VC_CONTAINER_TRACK_MODULE_T *t_module = track->priv->module;
   VC_CONTAINER_BITS_T *payload = &t_module->payload;
   H264_PAYLOAD_T *extra = (H264_PAYLOAD_T *)t_module->extra;
   uint32_t packet_flags = 0;
   uint8_t header_bytes_to_write;
   uint32_t size, offset;
   uint8_t *data_ptr;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;
   bool last_nal_unit_in_packet = false;

   if (BIT_IS_SET(t_module->flags, TRACK_NEW_PACKET))
   {
      status = h264_new_rtp_packet(p_ctx, t_module);
      if (status != VC_CONTAINER_SUCCESS)
         return status;
   }

   if (BIT_IS_SET(extra->flags, H264F_NEXT_PACKET_IS_START))
   {
      packet_flags |= VC_CONTAINER_PACKET_FLAG_FRAME_START;

      if (!(flags & VC_CONTAINER_READ_FLAG_INFO))
         CLEAR_BIT(extra->flags, H264F_NEXT_PACKET_IS_START);
   }

   if (!extra->nal_unit_size && BITS_BYTES_AVAILABLE(p_ctx, payload))
   {
      uint32_t stap_unit_header;

      /* STAP-A packet: read NAL unit size and header from payload */
      stap_unit_header = BITS_READ_U32(p_ctx, payload, 24, "STAP unit header");
      extra->nal_unit_size = stap_unit_header >> 8;
      if (extra->nal_unit_size > BITS_BYTES_AVAILABLE(p_ctx, payload))
      {
         LOG_ERROR(p_ctx, "H.264: STAP-A NAL unit size bigger than payload");
         return VC_CONTAINER_ERROR_FORMAT_INVALID;
      }
      extra->header_bytes_to_write = 5;
      extra->nal_header = (uint8_t)stap_unit_header;
   }

   header_bytes_to_write = extra->header_bytes_to_write;
   size = extra->nal_unit_size + header_bytes_to_write;

   if (p_packet && !(flags & VC_CONTAINER_READ_FLAG_SKIP))
   {
      if (flags & VC_CONTAINER_READ_FLAG_INFO)
      {
         /* In order to set the frame end flag correctly, need to work out if this
          * is the only NAL unit or last in an aggregated packet */
         last_nal_unit_in_packet = (extra->nal_unit_size == BITS_BYTES_AVAILABLE(p_ctx, payload));
      } else {
         offset = 0;
         data_ptr = p_packet->data;

         if (size > p_packet->buffer_size)
         {
            /* Buffer not big enough */
            size = p_packet->buffer_size;
         }

         /* Insert start code and header into the data stream */
         while (offset < size && header_bytes_to_write)
         {
            uint8_t header_byte;

            switch (header_bytes_to_write)
            {
            case 2: header_byte = 0x01; break;
            case 1: header_byte = extra->nal_header; break;
            default: header_byte = 0x00;
            }
            data_ptr[offset++] = header_byte;
            header_bytes_to_write--;
         }
         extra->header_bytes_to_write = header_bytes_to_write;

         if (offset < size)
         {
            BITS_COPY_BYTES(p_ctx, payload, size - offset, data_ptr + offset, "Packet data");
            extra->nal_unit_size -= (size - offset);
         }

         /* If we've read the final bytes of the packet, this must be the last (or only)
          * NAL unit in it */
         last_nal_unit_in_packet = !BITS_BYTES_AVAILABLE(p_ctx, payload);
      }
      p_packet->size = size;
   } else {
      extra->header_bytes_to_write = 0;
      BITS_SKIP_BYTES(p_ctx, payload, extra->nal_unit_size, "Packet data");
      last_nal_unit_in_packet = !BITS_BYTES_AVAILABLE(p_ctx, payload);
      extra->nal_unit_size = 0;
   }

   /* The marker bit on an RTP packet indicates the frame ends at the end of packet */
   if (last_nal_unit_in_packet && BIT_IS_SET(t_module->flags, TRACK_HAS_MARKER))
   {
      packet_flags |= VC_CONTAINER_PACKET_FLAG_FRAME_END;

      /* If this was the last packet of a frame, the next one must be the start */
      if (!(flags & VC_CONTAINER_READ_FLAG_INFO))
         SET_BIT(extra->flags, H264F_NEXT_PACKET_IS_START);
   }

   if (p_packet)
      p_packet->flags = packet_flags;

   return status;
}

/*****************************************************************************
Functions exported as part of the RTP parameter handler API
 *****************************************************************************/

/**************************************************************************//**
 * H.264 parameter handler.
 * Parses the URI parameters to set up the track for an H.264 stream.
 *
 * @param p_ctx   The reader context.
 * @param track   The track to be updated.
 * @param params  The URI parameter list.
 * @return  The resulting status of the function.
 */
VC_CONTAINER_STATUS_T h264_parameter_handler(VC_CONTAINER_T *p_ctx,
      VC_CONTAINER_TRACK_T *track,
      const VC_CONTAINERS_LIST_T *params)
{
   H264_PAYLOAD_T *extra;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_SUCCESS;

   VC_CONTAINER_PARAM_UNUSED(p_ctx);
   VC_CONTAINER_PARAM_UNUSED(params);

   /* See RFC3984, section 8.1, for parameter names and details. */
   extra = (H264_PAYLOAD_T *)malloc(sizeof(H264_PAYLOAD_T));
   if (!extra)
      return VC_CONTAINER_ERROR_OUT_OF_MEMORY;
   track->priv->module->extra = extra;
   memset(extra, 0, sizeof(H264_PAYLOAD_T));

   /* Mandatory parameters */
   status = h264_get_sprop_parameter_sets(p_ctx, track, params);
   if (status != VC_CONTAINER_SUCCESS) return status;

   /* Unsupported parameters */
   status = h264_check_unsupported_features(p_ctx, params);
   if (status != VC_CONTAINER_SUCCESS) return status;

   /* Optional parameters */
   status = h264_get_packetization_mode(p_ctx, params);
   if (status != VC_CONTAINER_SUCCESS) return status;

   track->priv->module->payload_handler = h264_payload_handler;
   SET_BIT(extra->flags, H264F_NEXT_PACKET_IS_START);

   track->format->flags |= VC_CONTAINER_ES_FORMAT_FLAG_FRAMED;
   track->priv->module->timestamp_clock = H264_TIMESTAMP_CLOCK;

   return status;
}

