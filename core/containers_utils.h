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
#ifndef VC_CONTAINERS_UTILS_H
#define VC_CONTAINERS_UTILS_H

#include "containers.h"
#include "containers_codecs.h"
#include "core/containers_waveformat.h"

/*****************************************************************************
 * Type definitions
 *****************************************************************************/

/** Definition of the Global Unique Identifier type as used by some containers */
typedef struct GUID_T
{
   uint32_t word0;
   uint16_t short0;
   uint16_t short1;
   uint8_t bytes[8];

} GUID_T;

/** Allocate and zero initialize an elementary stream format structure
 * @param extradata_size size of the extradata buffer to allocate
 *
 * @return point to allocated format structure
 */
VC_CONTAINER_ES_FORMAT_T *vc_container_format_create(unsigned int extradata_size);

/** Free a previously allocated elementary stream format structure along with
 * its extradata buffer
 * @format pointer to the format structure
 */
void vc_container_format_delete(VC_CONTAINER_ES_FORMAT_T *format);

/** Allocate or resize extradata buffer in elementary stream format structure
 * @format pointer to the format structure
 * @size size of the extradata buffer required
 *
 * @return allocation status
 */
VC_CONTAINER_STATUS_T vc_container_format_extradata_alloc(
   VC_CONTAINER_ES_FORMAT_T *format, unsigned int size);

/** Copy elementary stream format structure
 * @param out pointer to destination format structure
 * @param in pointer to source format structure
 * @max_extra_buffer_size sanity check the extra data size
 *
 * @return copy operation status
 */
VC_CONTAINER_STATUS_T vc_container_format_copy( VC_CONTAINER_ES_FORMAT_T *out,
   VC_CONTAINER_ES_FORMAT_T *in, unsigned int max_extra_buffer_size );

/** Compare 2 elementary stream format structures
 * @format1 pointer to 1st format structure
 * @format2 pointer to 2nd format structure
 * @b_ignore_extradata set to true to ignore the extradata buffer when comparing
 *
 * @return 0 if identical
 */
int vc_container_format_cmp(VC_CONTAINER_ES_FORMAT_T *format1, VC_CONTAINER_ES_FORMAT_T *format2,
   bool b_ignore_extradata);

int utf8_from_charset(const char *charset, char *out, unsigned int out_size,
                      const void *in, unsigned int in_size);

unsigned int vc_container_es_format_to_waveformatex(VC_CONTAINER_ES_FORMAT_T *format,
                                                    uint8_t *buffer, unsigned int buffer_size);
unsigned int vc_container_es_format_to_bitmapinfoheader(VC_CONTAINER_ES_FORMAT_T *format,
                                                        uint8_t *buffer, unsigned int buffer_size);
VC_CONTAINER_STATUS_T vc_container_waveformatex_to_es_format(uint8_t *p,
   unsigned int buffer_size, unsigned int *extra_offset, unsigned int *extra_size,
   VC_CONTAINER_ES_FORMAT_T *format);
VC_CONTAINER_STATUS_T vc_container_bitmapinfoheader_to_es_format(uint8_t *p,
   unsigned int buffer_size, unsigned int *extra_offset, unsigned int *extra_size, 
   VC_CONTAINER_ES_FORMAT_T *format);

/** Find the greatest common denominator of 2 numbers.
 * @param a first number
 * @param b second number
 *
 * @return greatest common denominator of a and b
 */
int64_t vc_container_maths_gcd(int64_t a, int64_t b);

/** Reduce a rational number to it's simplest form.
 * @param num Pointer to the numerator of the rational number to simplify
 * @param den Pointer to the denominator of the rational number to simplify
 */
void vc_container_maths_rational_simplify(uint32_t *num, uint32_t *den);

/** Print format in human readable form
 * @param level log level used for printing
 * @param format point to the format structure to print
 */
void vc_container_print_es_format(unsigned level, VC_CONTAINER_ES_FORMAT_T *format);

#endif /* VC_CONTAINERS_UTILS_H */
