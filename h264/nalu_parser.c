/*
Copyright (c) 2012, Broadcom Europe Ltd
Copyright (c) 2017, Gildas Bazin
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

#define ENABLE_CONTAINERS_LOG_FORMAT
#ifndef ENABLE_CONTAINERS_LOG_FORMAT
#define ENABLE_CONTAINERS_LOG_FORMAT
#endif
#ifndef ENABLE_CONTAINERS_LOG_FORMAT_VERBOSE
#define ENABLE_CONTAINERS_LOG_FORMAT_VERBOSE
#endif

#include "containers/containers.h"

#include "containers/core/containers_common.h"
#include "containers/core/containers_logging.h"
#include "containers/core/containers_bits.h"
#include "containers/core/containers_private.h"
#include "containers/core/containers_io_helpers.h"

#include "nalu_parser.h"

/******************************************************************************
Defines and constants.
******************************************************************************/

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

#define MACROBLOCK_WIDTH   16
#define MACROBLOCK_HEIGHT  16

typedef enum
{
   CHROMA_FORMAT_MONO = 0,
   CHROMA_FORMAT_YUV_420 = 1,
   CHROMA_FORMAT_YUV_422 = 2,
   CHROMA_FORMAT_YUV_444 = 3,
   CHROMA_FORMAT_YUV_444_PLANAR = 4,
   CHROMA_FORMAT_RGB = 5,
} CHROMA_FORMAT_T;

static uint32_t chroma_sub_width[] = {
   1, 2, 2, 1, 1, 1
};

static uint32_t chroma_sub_height[] = {
   1, 2, 1, 1, 1, 1
};

/******************************************************************************
Local Functions
******************************************************************************/

/**************************************************************************//**
 * Remove emulation prevention bytes from a buffer.
 * These are 0x03 bytes inserted to prevent misinterprentation of a byte
 * sequence in a buffer as a start code.
 *
 * @param p    The buffer from which bytes are to be removed.
 * @param size The number of bytes in the buffer.
 * @return  The new number of bytes in the buffer.
 */
static uint32_t h264_remove_emulation_prevention_bytes(uint8_t *p,
      uint32_t size)
{
   uint32_t offset = 0;
   uint8_t nal_unit_type = p[offset++];
   uint32_t new_size = size;
   uint8_t first_byte, second_byte;

   nal_unit_type &= 0x1F;  /* Just keep NAL unit type bits */

   /* Certain NAL unit types need a byte triplet passed first */
   if (nal_unit_type == NAL_UNIT_PREFIX || nal_unit_type == NAL_UNIT_EXTENSION)
      offset += 3;

   /* Make sure there is enough data for there to be a 0x00 0x00 0x03 sequence */
   if (offset + 2 >= new_size)
      return new_size;

   /* Keep a rolling set of the last couple of bytes */
   first_byte = p[offset++];
   second_byte = p[offset++];

   while (offset < new_size)
   {
      uint8_t next_byte = p[offset];

      if (!first_byte && !second_byte && next_byte == 0x03)
      {
         /* Remove the emulation prevention byte (0x03) */
         new_size--;
         if (offset == new_size) /* No more data to check */
            break;
         memmove(p + offset, p + offset + 1, new_size - offset);
         next_byte = p[offset];
      } else
         offset++;

      first_byte = second_byte;
      second_byte = next_byte;
   }

   return new_size;
}

/**************************************************************************//**
 * Skip a scaling list in a bit stream.
 *
 * @param ctx                    The container context.
 * @param bits                   The bit stream containing the scaling list.
 * @param size_of_scaling_list   The size of the scaling list.
 */
static void h264_skip_scaling_list(VC_CONTAINER_T *ctx,
      VC_CONTAINER_BITS_T *bits,
      unsigned int size_of_scaling_list)
{
   uint32_t last_scale = 8, next_scale = 8;
   int32_t delta_scale;
   unsigned int i;

   /* Algorithm taken from H.264 section 7.3.2.1.1.1 */
   for (i = 0; i < size_of_scaling_list; i++)
   {
      if (next_scale)
      {
         delta_scale = BITS_READ_S32_EXP(ctx, bits, "delta_scale");
         next_scale = (last_scale + delta_scale + 256) & 0xFF;

         if (next_scale)
            last_scale = next_scale;
      }
   }
}

/**************************************************************************//**
 * Get the chroma format from the bit stream.
 *
 * @param ctx   The container context.
 * @param bits  The bit stream containing the scaling list.
 * @return  The chroma format index.
 */
static uint32_t h264_get_chroma_format(VC_CONTAINER_T *ctx,
      VC_CONTAINER_BITS_T *bits)
{
   uint32_t chroma_format_idc;

   chroma_format_idc = BITS_READ_U32_EXP(ctx, bits, "chroma_format_idc");
   if (chroma_format_idc == 3 && BITS_READ_U32(ctx, bits, 1, "separate_colour_plane_flag"))
      chroma_format_idc = CHROMA_FORMAT_YUV_444_PLANAR;

   BITS_SKIP_EXP(ctx, bits, "bit_depth_luma_minus8");
   BITS_SKIP_EXP(ctx, bits, "bit_depth_chroma_minus8");
   BITS_SKIP(ctx, bits, 1, "qpprime_y_zero_transform_bypass_flag");

   if (BITS_READ_U32(ctx, bits, 1, "seq_scaling_matrix_present_flag"))
   {
      uint32_t scaling_lists = (chroma_format_idc == 3) ? 12 : 8;
      uint32_t i;

      for (i = 0; i < scaling_lists; i++)
      {
         if (BITS_READ_U32(ctx, bits, 1, "seq_scaling_list_present_flag"))
            h264_skip_scaling_list(ctx, bits, (i < 6) ? 16 : 64);
      }
   }

   return chroma_format_idc;
}

/**************************************************************************//**
 * Parse an H.264 sequence parameter set.
 *
 * @param ctx   The container context.
 * @param bits  The bit stream containing the sequence parameter set.
 * @return  The resulting status of the function.
 */
VC_CONTAINER_STATUS_T h264_parse_sequence_parameter_set(VC_CONTAINER_T *ctx,
      VC_CONTAINER_BITS_T *bits)
{
   uint32_t pic_order_cnt_type, chroma_format_idc;
   uint32_t pic_width_in_mbs_minus1, pic_height_in_map_units_minus1, frame_mbs_only_flag;
   uint32_t frame_crop_left_offset, frame_crop_right_offset, frame_crop_top_offset, frame_crop_bottom_offset;
   uint8_t profile_idc;

   BITS_SKIP(ctx, bits, 1, "forbidden_zero_bit");
   BITS_SKIP(ctx, bits, 2, "nal_ref_idc");
   BITS_READ_U32(ctx, bits, 5, "nal_unit_type");

   /* This structure is defined by H.264 section 7.3.2.1.1 */
   profile_idc = BITS_READ_U8(ctx, bits, 8, "profile_idc");
   BITS_SKIP(ctx, bits, 16, "Rest of profile_level_id");

   BITS_READ_U32_EXP(ctx, bits, "seq_parameter_set_id");

   chroma_format_idc = CHROMA_FORMAT_RGB;
   if (profile_idc == 100 || profile_idc == 110 || profile_idc == 122 ||
         profile_idc == 244 || profile_idc == 44 || profile_idc == 83 ||
         profile_idc == 86 || profile_idc == 118 || profile_idc == 128)
   {
      chroma_format_idc = h264_get_chroma_format(ctx, bits);
      if (chroma_format_idc > CHROMA_FORMAT_YUV_444_PLANAR)
         goto error;
   }

   BITS_READ_U32_EXP(ctx, bits, "log2_max_frame_num_minus4");
   pic_order_cnt_type = BITS_READ_U32_EXP(ctx, bits, "pic_order_cnt_type");
   if (pic_order_cnt_type == 0)
   {
      BITS_READ_U32_EXP(ctx, bits, "log2_max_pic_order_cnt_lsb_minus4");
   }
   else if (pic_order_cnt_type == 1)
   {
      uint32_t num_ref_frames_in_pic_order_cnt_cycle;
      uint32_t ii;

      BITS_SKIP(ctx, bits, 1, "delta_pic_order_always_zero_flag");
      BITS_SKIP_EXP(ctx, bits, "offset_for_non_ref_pic");
      BITS_SKIP_EXP(ctx, bits, "offset_for_top_to_bottom_field");
      num_ref_frames_in_pic_order_cnt_cycle = BITS_READ_U32_EXP(ctx, bits, "num_ref_frames_in_pic_order_cnt_cycle");

      for (ii = 0; ii < num_ref_frames_in_pic_order_cnt_cycle; ii++)
         BITS_SKIP_EXP(ctx, bits, "offset_for_ref_frame");
   }

   BITS_READ_U32_EXP(ctx, bits, "max_num_ref_frames");
   BITS_READ_U32(ctx, bits, 1, "gaps_in_frame_num_value_allowed_flag");

   pic_width_in_mbs_minus1 = BITS_READ_U32_EXP(ctx, bits, "pic_width_in_mbs_minus1");
   pic_height_in_map_units_minus1 = BITS_READ_U32_EXP(ctx, bits, "pic_height_in_map_units_minus1");
   frame_mbs_only_flag = BITS_READ_U32(ctx, bits, 1, "frame_mbs_only_flag");

   /* Can now set the overall width and height in pixels */
   int width = (pic_width_in_mbs_minus1 + 1) * MACROBLOCK_WIDTH;
   int height = (2 - frame_mbs_only_flag) * (pic_height_in_map_units_minus1 + 1) * MACROBLOCK_HEIGHT;

   if (!frame_mbs_only_flag)
      BITS_SKIP(ctx, bits, 1, "mb_adaptive_frame_field_flag");
   BITS_SKIP(ctx, bits, 1, "direct_8x8_inference_flag");

   if (BITS_READ_U32(ctx, bits, 1, "frame_cropping_flag"))
   {
      /* Visible area is restricted */
      frame_crop_left_offset = BITS_READ_U32_EXP(ctx, bits, "frame_crop_left_offset");
      frame_crop_right_offset = BITS_READ_U32_EXP(ctx, bits, "frame_crop_right_offset");
      frame_crop_top_offset = BITS_READ_U32_EXP(ctx, bits, "frame_crop_top_offset");
      frame_crop_bottom_offset = BITS_READ_U32_EXP(ctx, bits, "frame_crop_bottom_offset");

      /* Need to adjust offsets for 4:2:0 and 4:2:2 chroma formats and field/frame flag */
      frame_crop_left_offset *= chroma_sub_width[chroma_format_idc];
      frame_crop_right_offset *= chroma_sub_width[chroma_format_idc];
      frame_crop_top_offset *= chroma_sub_height[chroma_format_idc] * (2 - frame_mbs_only_flag);
      frame_crop_bottom_offset *= chroma_sub_height[chroma_format_idc] * (2 - frame_mbs_only_flag);

      if ((frame_crop_left_offset + frame_crop_right_offset) >= width ||
            (frame_crop_top_offset + frame_crop_bottom_offset) >= height)
      {
         LOG_ERROR(ctx, "H.264: frame crop offsets (%u, %u, %u, %u) larger than frame (%u, %u)",
               frame_crop_left_offset, frame_crop_right_offset, frame_crop_top_offset,
               frame_crop_bottom_offset, width, height);
         goto error;
      }

      int x_offset = frame_crop_left_offset;
      int y_offset = frame_crop_top_offset;
      int visible_width = width - frame_crop_left_offset - frame_crop_right_offset;
      int visible_height = height - frame_crop_top_offset - frame_crop_bottom_offset;
   } else {
      int visible_width = width;
      int visible_height = height;
   }

   /* vui_parameters may follow, but these will not be decoded */

   if (!BITS_VALID(ctx, bits))
      goto error;

   return VC_CONTAINER_SUCCESS;

error:
   LOG_ERROR(ctx, "H.264: sequence_parameter_set failed to decode");
   return VC_CONTAINER_ERROR_FORMAT_INVALID;
}

/**************************************************************************//**
 * Parse an H.264 picture parameter set.
 *
 * @param ctx   The container context.
 * @param bits  The bit stream containing the picture parameter set.
 * @return  The resulting status of the function.
 */
VC_CONTAINER_STATUS_T h264_parse_picture_parameter_set(VC_CONTAINER_T *ctx,
      VC_CONTAINER_BITS_T *bits)
{
   BITS_SKIP(ctx, bits, 1, "forbidden_zero_bit");
   BITS_SKIP(ctx, bits, 2, "nal_ref_idc");
   BITS_READ_U32(ctx, bits, 5, "nal_unit_type");

   /* This structure is defined by H.264 section 7.3.2.2 */
   BITS_READ_U32_EXP(ctx, bits, "pic_parameter_set_id");
   BITS_READ_U32_EXP(ctx, bits, "seq_parameter_set_id");
   BITS_SKIP(ctx, bits, 1, "entropy_coding_mode_flag");
   BITS_SKIP(ctx, bits, 1, "bottom_field_pic_order_in_frame_present_flag");
   BITS_READ_U32_EXP(ctx, bits, "num_slice_groups_minus1");
   return VC_CONTAINER_SUCCESS;
}

/**************************************************************************//**
 * Parse an H.264 slice.
 *
 * @param ctx   The container context.
 * @param bits  The bit stream containing the slice.
 * @return  The resulting status of the function.
 */
VC_CONTAINER_STATUS_T h264_parse_slice(VC_CONTAINER_T *ctx,
      VC_CONTAINER_BITS_T *bits)
{
   BITS_SKIP(ctx, bits, 1, "forbidden_zero_bit");
   BITS_SKIP(ctx, bits, 2, "nal_ref_idc");
   unsigned int nal_unit_type = BITS_READ_U32(ctx, bits, 5, "nal_unit_type");

   BITS_READ_U32_EXP(ctx, bits, "first_mb_in_slice");
   unsigned int slice_type = BITS_READ_U32_EXP(ctx, bits, "slice_type") % 5;
   BITS_READ_U32_EXP(ctx, bits, "pic_parameter_set_id");

   if (0 /* separate_colour_plane_flag*/)
      BITS_READ_U32(ctx, bits, 2, "colour_plane_id");

   unsigned int log2_max_frame_num = 0 /* log2_max_frame_num_minus4 */ + 4; 
   unsigned int frame_num = BITS_READ_U32(ctx, bits, log2_max_frame_num, "frame_num");

#if 0
   static unsigned int prev_frame_num;
   if (frame_num != prev_frame_num && nal_unit_type != 5 &&
      frame_num != (prev_frame_num + 1) % (1 << log2_max_frame_num))
      LOG_FORMAT(ctx, "detected frame gap GAP");

   static unsigned int count_since_idr = 0;
   if (nal_unit_type == 5) count_since_idr = 0;

   if (nal_unit_type != 5 && !(count_since_idr & 0x1) && prev_frame_num != frame_num)
      LOG_FORMAT(ctx, "MISSING LAYER");

   count_since_idr++;
   prev_frame_num = frame_num;
#endif


   if (nal_unit_type == 5) {
       BITS_READ_U32_EXP(ctx, bits, "idr_pic_id");
   }

   return VC_CONTAINER_SUCCESS;
}

/**************************************************************************//**
 * Parse an H.264 sei.
 *
 * @param ctx   The container context.
 * @param bits  The bit stream containing the sei.
 * @return  The resulting status of the function.
 */
VC_CONTAINER_STATUS_T h264_parse_sei(VC_CONTAINER_T *ctx,
      VC_CONTAINER_BITS_T *bits)
{
   BITS_SKIP(ctx, bits, 1, "forbidden_zero_bit");
   BITS_SKIP(ctx, bits, 2, "nal_ref_idc");
   BITS_READ_U32(ctx, bits, 5, "nal_unit_type");

   return VC_CONTAINER_SUCCESS;
}

/**************************************************************************//**
 * Parse an H.264 prefix.
 *
 * @param ctx   The container context.
 * @param bits  The bit stream containing the prefix.
 * @return  The resulting status of the function.
 */
VC_CONTAINER_STATUS_T h264_parse_prefix(VC_CONTAINER_T *ctx,
      VC_CONTAINER_BITS_T *bits)
{
   BITS_SKIP(ctx, bits, 1, "forbidden_zero_bit");
   BITS_SKIP(ctx, bits, 2, "nal_ref_idc");
   BITS_READ_U32(ctx, bits, 5, "nal_unit_type");

   if (BITS_READ_U32(ctx, bits, 1, "svc_extension_flag")) {
      /* SVC */
      BITS_SKIP(ctx, bits, 15, "15");
      BITS_READ_U32(ctx, bits, 3, "temporal_id");
   } else {
      /* MVC */
      BITS_SKIP(ctx, bits, 17, "17");
      BITS_READ_U32(ctx, bits, 3, "temporal_id");
   }

   return VC_CONTAINER_SUCCESS;
}

/**************************************************************************//**
 * Parse an H.264 NAL unit.
 *
 * @param ctx  The container context.
 * @param p    The buffer containing the NAL unit.
 * @param p    The size of the NAL unit.
 * @return  The resulting status of the function.
 */
VC_CONTAINER_STATUS_T h264_parse_nal_unit(VC_CONTAINER_T *ctx,
      uint8_t *p, uint32_t size)
{
   VC_CONTAINER_BITS_T bits;
   BITS_INIT(ctx, &bits, p, size);
  
   switch (p[0] & NAL_UNIT_TYPE_MASK)
   {
   case NAL_UNIT_SEQUENCE_PARAMETER_SET:
      LOG_FORMAT(ctx, "NALU SPS, size: %u", BITS_BYTES_AVAILABLE(ctx, &bits));
      return h264_parse_sequence_parameter_set(ctx, &bits);
   case NAL_UNIT_PICTURE_PARAMETER_SET:
      LOG_FORMAT(ctx, "NALU PPS, size: %u", BITS_BYTES_AVAILABLE(ctx, &bits));
      return h264_parse_picture_parameter_set(ctx, &bits);
   case NAL_UNIT_NON_IDR:
   case NAL_UNIT_PARTITION_A:
   case NAL_UNIT_PARTITION_B:
   case NAL_UNIT_PARTITION_C:
   case NAL_UNIT_IDR:
      LOG_FORMAT(ctx, "NALU SLICE, size: %u", BITS_BYTES_AVAILABLE(ctx, &bits));
      return h264_parse_slice(ctx, &bits);
   case NAL_UNIT_SEI:
      LOG_FORMAT(ctx, "NALU SEI, size: %u", BITS_BYTES_AVAILABLE(ctx, &bits));
      return h264_parse_sei(ctx, &bits);
   case NAL_UNIT_PREFIX:
   case NAL_UNIT_EXTENSION:
      LOG_FORMAT(ctx, "NALU PREFIX, size: %u", BITS_BYTES_AVAILABLE(ctx, &bits));
      return h264_parse_prefix(ctx, &bits);
   default:
      LOG_FORMAT(ctx, "NALU %u, size: %u", p[0] & NAL_UNIT_TYPE_MASK, BITS_BYTES_AVAILABLE(ctx, &bits));
      return VC_CONTAINER_SUCCESS;
   }
}
