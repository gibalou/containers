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

#ifndef _H264_NALU_PARSER_H_
#define _H264_NALU_PARSER_H_

#include "containers/containers.h"
#include "containers/core/containers_bits.h"

/**************************************************************************//**
 * Parse an H.264 NAL unit.
 *
 * @param ctx  The container context.
 * @param p    The buffer containing the NAL unit.
 * @param p    The size of the NAL unit.
 * @return  The resulting status of the function.
 */
VC_CONTAINER_STATUS_T h264_parse_nal_unit(VC_CONTAINER_T *ctx,
      uint8_t *p, uint32_t size);

/**************************************************************************//**
 * Parse an H.264 sequence parameter set.
 *
 * @param p_ctx  The container context.
 * @param bits   The bit stream containing the sequence parameter set.
 * @return  The resulting status of the function.
 */
VC_CONTAINER_STATUS_T h264_parse_sequence_parameter_set(VC_CONTAINER_T *p_ctx,
      VC_CONTAINER_BITS_T *bits);

#endif /* _H264_NALU_PARSER_H_ */
