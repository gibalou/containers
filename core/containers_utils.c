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
#include <string.h>

#include "containers.h"
#include "core/containers_common.h"
#include "core/containers_utils.h"
#include "core/containers_logging.h"

/******************************************************************************
Defines.
******************************************************************************/
#define BITMAPINFOHEADER_SIZE_MAX 40
#define MAX_EXTENSION_SIZE 4

#define VC_CONTAINER_ES_FORMAT_MAGIC ((uint32_t)VC_FOURCC('m','a','g','f'))
#define EXTRADATA_SIZE_DEFAULT 32
#define EXTRADATA_SIZE_MAX (10*1024)

/*****************************************************************************/
typedef struct VC_CONTAINER_ES_FORMAT_PRIVATE_T
{
   VC_CONTAINER_ES_FORMAT_T format;
   VC_CONTAINER_ES_SPECIFIC_FORMAT_T type;

   uint32_t magic;

   unsigned int extradata_size;
   uint8_t *extradata;

   uint8_t buffer[EXTRADATA_SIZE_DEFAULT];

} VC_CONTAINER_ES_FORMAT_PRIVATE_T;

/*****************************************************************************/
VC_CONTAINER_ES_FORMAT_T *vc_container_format_create(unsigned int extradata_size)
{
   VC_CONTAINER_ES_FORMAT_PRIVATE_T *private;
   VC_CONTAINER_STATUS_T status;

   private = malloc(sizeof(*private));
   if(!private) return 0;
   memset(private, 0, sizeof(*private));

   private->magic = VC_CONTAINER_ES_FORMAT_MAGIC;
   private->format.type = (void *)&private->type;
   private->extradata_size = EXTRADATA_SIZE_DEFAULT;

   status = vc_container_format_extradata_alloc(&private->format, extradata_size);
   if(status != VC_CONTAINER_SUCCESS)
   {
      free(private);
      return NULL;
   }

   return &private->format;
}

/*****************************************************************************/
void vc_container_format_delete(VC_CONTAINER_ES_FORMAT_T *format)
{
   VC_CONTAINER_ES_FORMAT_PRIVATE_T *private = (VC_CONTAINER_ES_FORMAT_PRIVATE_T *)format;
   vc_container_assert(private->magic == VC_CONTAINER_ES_FORMAT_MAGIC);
   if(private->extradata) free(private->extradata);
   free(private);
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T vc_container_format_extradata_alloc(
   VC_CONTAINER_ES_FORMAT_T *format, unsigned int size)
{
   VC_CONTAINER_ES_FORMAT_PRIVATE_T *private = (VC_CONTAINER_ES_FORMAT_PRIVATE_T *)format;
   vc_container_assert(private->magic == VC_CONTAINER_ES_FORMAT_MAGIC);

   /* Sanity check the size requested */
   if(size > EXTRADATA_SIZE_MAX)
      return VC_CONTAINER_ERROR_CORRUPTED;

   /* Allocate memory if needed */
   if(private->extradata_size < size)
   {
      if(private->extradata) free(private->extradata);
      private->extradata = malloc(size);
      if(!private->extradata)
         return VC_CONTAINER_ERROR_OUT_OF_MEMORY;
      private->extradata_size = size;
   }

   /* Set the fields in the actual format structure */
   if(private->extradata) private->format.extradata = private->extradata;
   else private->format.extradata = private->buffer;

   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T vc_container_format_copy( VC_CONTAINER_ES_FORMAT_T *p_out,
                                                VC_CONTAINER_ES_FORMAT_T *p_in,
                                                unsigned int extra_buffer_size)
{
   void *type = p_out->type;
   uint8_t *extradata = p_out->extradata;

   /* Check we have a sufficient buffer to copy the extra data */
   if(p_in->extradata_size > extra_buffer_size ||
      (p_in->extradata_size && !p_out->extradata))
      return VC_CONTAINER_ERROR_BUFFER_TOO_SMALL;

   *p_out->type = *p_in->type;
   *p_out = *p_in;
   p_out->type = type;
   p_out->extradata = extradata;
   if(p_in->extradata_size)
      memcpy(p_out->extradata, p_in->extradata, p_in->extradata_size);

   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
int vc_container_format_cmp(VC_CONTAINER_ES_FORMAT_T *p_fmt1,
                            VC_CONTAINER_ES_FORMAT_T *p_fmt2,
                            bool b_ignore_extradata)
{
   VC_CONTAINER_ES_FORMAT_T fmt1 = *p_fmt1, fmt2 = *p_fmt2;
   VC_CONTAINER_ES_SPECIFIC_FORMAT_T type1 = *p_fmt1->type, type2 = *p_fmt2->type;

   fmt1.type = fmt2.type = 0;
   fmt1.extradata = fmt2.extradata = 0;

   if (b_ignore_extradata)
      fmt1.extradata_size = fmt2.extradata_size = 0;

   if (memcmp(&fmt1, &fmt2, sizeof(fmt1)))
      return -1;

   if (memcmp(&type1, &type2, sizeof(type1)))
      return -1;

   if (!b_ignore_extradata &&
       memcmp(p_fmt1->extradata, p_fmt2->extradata, p_fmt1->extradata_size))
      return -1;

   return 0;
}

/*****************************************************************************/
int utf8_from_charset(const char *charset, char *out, unsigned int out_size,
                      const void *in, unsigned int in_size)
{
   unsigned int i;
   const uint16_t *in16 = (const uint16_t *)in;
   const uint8_t *in8 = (const uint8_t *)in;

   if(out_size < 1) return 1;
   if(!strcmp(charset, "UTF16-LE")) goto utf16le;
   if(!strcmp(charset, "UTF8")) goto utf8;
   else return 1;

 utf16le:
   for(i = 0; i < in_size / 2 && in16[i] && i < out_size - 1; i++)
   {
      out[i] = in16[i];
   }
   out[i] = 0;
   return 0;

 utf8:
   for(i = 0; i < in_size && in8[i] && i < out_size - 1; i++)
   {
      out[i] = in8[i];
   }
   out[i] = 0;
   return 0;
}

/*****************************************************************************/
unsigned int vc_container_es_format_to_waveformatex(VC_CONTAINER_ES_FORMAT_T *format,
                                                    uint8_t *buffer, unsigned int buffer_size)
{
   uint16_t waveformat = codec_to_waveformat(format->codec);

   if(format->es_type != VC_CONTAINER_ES_TYPE_AUDIO ||
      waveformat == WAVE_FORMAT_UNKNOWN) return 0;

   if(!buffer) return format->extradata_size + 18;

   if(buffer_size < format->extradata_size + 18) return 0;

   /* Build a waveformatex header */
   buffer[0] = waveformat;
   buffer[1] = waveformat >> 8;
   buffer[2] = format->type->audio.channels;
   buffer[3] = 0;
   buffer[4] = (format->type->audio.sample_rate >> 0) & 0xFF;
   buffer[5] = (format->type->audio.sample_rate >> 8) & 0xFF;
   buffer[6] = (format->type->audio.sample_rate >> 16) & 0xFF;
   buffer[7] = (format->type->audio.sample_rate >> 24) & 0xFF;
   buffer[8] = (format->bitrate >> 3) & 0xFF;
   buffer[9] = (format->bitrate >> 11) & 0xFF;
   buffer[10] = (format->bitrate >> 19) & 0xFF;
   buffer[11] = (format->bitrate >> 27) & 0xFF;
   buffer[12] = (format->type->audio.block_align >> 0) & 0xFF;
   buffer[13] = (format->type->audio.block_align >> 8) & 0xFF;
   buffer[14] = (format->type->audio.bits_per_sample >> 0) & 0xFF;
   buffer[15] = (format->type->audio.bits_per_sample >> 8) & 0xFF;
   buffer[16] = (format->extradata_size >> 0) & 0xFF;
   buffer[17] = (format->extradata_size >> 8) & 0xFF;
   memcpy(buffer+18, format->extradata, format->extradata_size);
   return format->extradata_size + 18;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T vc_container_waveformatex_to_es_format(uint8_t *p,
   unsigned int buffer_size, unsigned int *extra_offset, unsigned int *extra_size,
   VC_CONTAINER_ES_FORMAT_T *format)
{
   VC_CONTAINER_FOURCC_T fourcc;
   uint32_t waveformat_id;

   if(!p || buffer_size < 16) return VC_CONTAINER_ERROR_INVALID_ARGUMENT;
   waveformat_id = (p[1] << 8) | p[0];
   fourcc = waveformat_to_codec(waveformat_id);

   /* Read the waveformatex header */
   if(extra_offset) *extra_offset = 16;
   if(extra_size) *extra_size = 0;
   format->type->audio.channels = p[2];
   format->type->audio.sample_rate = (p[7] << 24) | (p[6] << 16) | (p[5] << 8) | p[4];
   format->bitrate = ((p[11] << 24) | (p[10] << 16) | (p[9] << 8) | p[8]) * 8;
   format->type->audio.block_align = (p[13] << 8) | p[12];
   format->type->audio.bits_per_sample = (p[15] << 8) | p[14];

   if(waveformat_id == WAVE_FORMAT_PCM && format->type->audio.bits_per_sample == 8)
      fourcc = VC_CONTAINER_CODEC_PCM_UNSIGNED_LE;

   if(buffer_size >= 18)
   {
      if(extra_size)
      {
         *extra_size = (p[17] << 8) | p[16];
         if(*extra_size + 18 > buffer_size) *extra_size = buffer_size - 18;
      }
      if(extra_offset) *extra_offset = 18;
   }

   /* Skip the MPEGLAYER3WAVEFORMAT structure */
   if(waveformat_id == WAVE_FORMAT_MPEGLAYER3 && extra_size)
   {
      if(extra_offset) *extra_offset += *extra_size;
      *extra_size = 0;
   }

   format->es_type = VC_CONTAINER_ES_TYPE_AUDIO;
   format->codec = fourcc;

   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
unsigned int vc_container_es_format_to_bitmapinfoheader(VC_CONTAINER_ES_FORMAT_T *format,
                                                        uint8_t *buffer, unsigned int buffer_size)
{
   uint32_t fourcc = codec_to_vfw_fourcc(format->codec);
   uint32_t size = BITMAPINFOHEADER_SIZE_MAX + format->extradata_size;

   if(format->es_type != VC_CONTAINER_ES_TYPE_VIDEO ||
      fourcc == VC_CONTAINER_CODEC_UNKNOWN) return 0;

   if(!buffer) return size;
   if(buffer_size < size) return 0;

   /* Build a bitmapinfoheader header */
   memset(buffer, 0, BITMAPINFOHEADER_SIZE_MAX);
   buffer[0] = (size >> 0) & 0xFF;
   buffer[1] = (size >> 8) & 0xFF;
   buffer[2] = (size >> 16) & 0xFF;
   buffer[3] = (size >> 24) & 0xFF;
   buffer[4] = (format->type->video.width >> 0) & 0xFF;
   buffer[5] = (format->type->video.width >> 8) & 0xFF;
   buffer[6] = (format->type->video.width >> 16) & 0xFF;
   buffer[7] = (format->type->video.width >> 24) & 0xFF;
   buffer[8] = (format->type->video.height >> 0) & 0xFF;
   buffer[9] = (format->type->video.height >> 8) & 0xFF;
   buffer[10] = (format->type->video.height >> 16) & 0xFF;
   buffer[11] = (format->type->video.height >> 24) & 0xFF;
   memcpy(buffer + 16, &fourcc, 4);
   memcpy(buffer + BITMAPINFOHEADER_SIZE_MAX, format->extradata, format->extradata_size);
   return size;
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T vc_container_bitmapinfoheader_to_es_format(uint8_t *p,
   unsigned int buffer_size, unsigned int *extra_offset, unsigned int *extra_size, 
   VC_CONTAINER_ES_FORMAT_T *format)
{
   VC_CONTAINER_FOURCC_T fourcc;
   
   if(!p || buffer_size < BITMAPINFOHEADER_SIZE_MAX) return VC_CONTAINER_ERROR_INVALID_ARGUMENT;
   /* size = (p[3] << 24) | (p[2] << 16) | (p[1] << 8) | p[0]; */
   format->type->video.width = (p[7] << 24) | (p[6] << 16) | (p[5] << 8) | p[4];
   format->type->video.height = (p[11] << 24) | (p[10] << 16) | (p[9] << 8) | p[8];
   memcpy(&fourcc, p + 16, 4);
  
   format->es_type = VC_CONTAINER_ES_TYPE_VIDEO;
   format->codec = vfw_fourcc_to_codec(fourcc);
   
   /* If no mapping is found from vfw, try a more generic one */
   if (format->codec == fourcc && (fourcc = fourcc_to_codec(fourcc)) != VC_CONTAINER_CODEC_UNKNOWN)
      format->codec = fourcc;

   if(extra_offset) *extra_offset = BITMAPINFOHEADER_SIZE_MAX;
   if(extra_size) 
   {
      if (buffer_size > BITMAPINFOHEADER_SIZE_MAX)
         *extra_size = buffer_size - BITMAPINFOHEADER_SIZE_MAX;
      else
         *extra_size = 0;
   }

   return VC_CONTAINER_SUCCESS;   
}

/*****************************************************************************/
int64_t vc_container_maths_gcd(int64_t a, int64_t b)
{
   while(b != 0)
   {
      int64_t t = b;
      b = a % b;
      a = t;
   }
   return a;
}

/*****************************************************************************/
void vc_container_maths_rational_simplify(uint32_t *num, uint32_t *den)
{
   int64_t div = vc_container_maths_gcd((int64_t)*num, (int64_t)*den);
   if(div)
   {
      *num /= div;
      *den /= div;
   }
}

/*****************************************************************************/
void vc_container_print_es_format(unsigned level, VC_CONTAINER_ES_FORMAT_T *format)
{
   const char *name_type;

   switch(format->es_type)
   {
   case VC_CONTAINER_ES_TYPE_AUDIO: name_type = "audio"; break;
   case VC_CONTAINER_ES_TYPE_VIDEO: name_type = "video"; break;
   case VC_CONTAINER_ES_TYPE_SUBPICTURE: name_type = "subpicture"; break;
   default: name_type = "unknown"; break;
   }

   vc_container_log(0, level, "type: %s, fourcc: %4.4s, bps: %i, framed: %i", name_type,
       (char *)&format->codec, format->bitrate,
       !!(format->flags & VC_CONTAINER_ES_FORMAT_FLAG_FRAMED));
   vc_container_log(0, level, " extra data: %i, %p", format->extradata_size, format->extradata);
   switch(format->es_type)
   {
   case VC_CONTAINER_ES_TYPE_AUDIO:
      vc_container_log(0, level, " samplerate: %i, channels: %i, bps: %i, block align: %i",
         format->type->audio.sample_rate, format->type->audio.channels,
         format->type->audio.bits_per_sample, format->type->audio.block_align);
      vc_container_log(0, level, " gapless delay: %i gapless padding: %i",
         format->type->audio.gap_delay, format->type->audio.gap_padding);
      vc_container_log(0, level, " language: %4.4s", format->language);
      break;

   case VC_CONTAINER_ES_TYPE_VIDEO:
      vc_container_log(0, level, " width: %i, height: %i, (%i,%i,%i,%i)",
         format->type->video.width, format->type->video.height,
         format->type->video.x_offset, format->type->video.y_offset,
         format->type->video.visible_width, format->type->video.visible_height);
      vc_container_log(0, level, " pixel aspect ratio: %i/%i, frame rate: %i/%i",
         format->type->video.par_num, format->type->video.par_den,
         format->type->video.frame_rate_num, format->type->video.frame_rate_den);
      break;

   case VC_CONTAINER_ES_TYPE_SUBPICTURE:
      vc_container_log(0, level, " language: %4.4s, encoding: %i", format->language,
         format->type->subpicture.encoding);
      break;

   default: break;
   }
}
