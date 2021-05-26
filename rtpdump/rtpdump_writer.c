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
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/time.h> /* gettimeofday */

#include <netinet/in.h> /* inet_addr */
#include <arpa/inet.h>

#define CONTAINER_IS_BIG_ENDIAN
#include "core/containers_private.h"
#include "core/containers_io_helpers.h"
#include "core/containers_utils.h"
#include "core/containers_writer_utils.h"
#include "core/containers_logging.h"

#include "rtpdump_common.h"

/******************************************************************************
Defines.
******************************************************************************/
#define MAX_RTP_PACKET_SIZE (4*1024)

/******************************************************************************
Type definitions
******************************************************************************/
typedef struct VC_CONTAINER_MODULE_T
{
   VC_CONTAINER_TRACK_T *track;
   bool header_done;
   uint64_t start;

} VC_CONTAINER_MODULE_T;

/******************************************************************************
Function prototypes
******************************************************************************/
VC_CONTAINER_STATUS_T rtpdump_writer_open( VC_CONTAINER_T * );
static VC_CONTAINER_STATUS_T rtpdump_writer_write( VC_CONTAINER_T *ctx,
   VC_CONTAINER_PACKET_T *packet );

/******************************************************************************
Local Functions
******************************************************************************/
static VC_CONTAINER_STATUS_T rtpdump_write_header( VC_CONTAINER_T *ctx )
{
   const char signature[] = "#!rtpplay1.0 127.0.0.1/0\n";
   struct timeval tv;

   gettimeofday(&tv, NULL);
   ctx->priv->module->start = tv.tv_sec * UINT64_C(1000) + tv.tv_usec / 1000;

   WRITE_BYTES(ctx, signature, sizeof(signature) - 1);
   WRITE_U32(ctx, tv.tv_sec, "seconds");
   WRITE_U32(ctx, tv.tv_usec, "useconds");
   WRITE_U32(ctx, inet_addr("127.0.0.1"), "source");
   WRITE_U16(ctx, 0, "port");
   WRITE_U16(ctx, 0, "padding");

   ctx->priv->module->header_done = true;
   return STREAM_STATUS(ctx);
}

static VC_CONTAINER_STATUS_T rtpdump_write_add_track( VC_CONTAINER_T *ctx,
   VC_CONTAINER_ES_FORMAT_T *format )
{
   VC_CONTAINER_PARAM_UNUSED(format);

   /* Allocate and initialise track data */
   if (ctx->tracks_num)
      return VC_CONTAINER_ERROR_OUT_OF_RESOURCES;

   ctx->tracks[0] = vc_container_allocate_track(ctx, 0);
   if (!ctx->tracks[0])
      return VC_CONTAINER_ERROR_OUT_OF_MEMORY;

   ctx->tracks_num++;
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************
Functions exported as part of the Container Module API
 *****************************************************************************/
static VC_CONTAINER_STATUS_T rtpdump_writer_close( VC_CONTAINER_T *ctx )
{
   VC_CONTAINER_MODULE_T *module = ctx->priv->module;
   vc_container_free_track(ctx, module->track);
   free(module);
   return VC_CONTAINER_SUCCESS;
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T rtpdump_writer_write( VC_CONTAINER_T *ctx,
   VC_CONTAINER_PACKET_T *packet )
{
   VC_CONTAINER_STATUS_T status;
   struct timeval tv;
   uint32_t size, offset;

   if (!ctx->priv->module->header_done)
   {
      status = rtpdump_write_header(ctx);
      if (status != VC_CONTAINER_SUCCESS)
         return status;
   }

   gettimeofday(&tv, NULL);
   offset = (tv.tv_sec * UINT64_C(1000) + tv.tv_usec / 1000) -
      ctx->priv->module->start;
   size = packet->size > MAX_RTP_PACKET_SIZE ? MAX_RTP_PACKET_SIZE : packet->size;

   /* Write the packet header */
   WRITE_U16(ctx, size + 8, "packet length");
   WRITE_U16(ctx, size, "actual header+payload length");
   WRITE_U32(ctx, offset, "msecs since start");

   /* Write the elementary stream */
   WRITE_BYTES(ctx, packet->data, size);

   return STREAM_STATUS(ctx);
}

/*****************************************************************************/
static VC_CONTAINER_STATUS_T rtpdump_writer_control( VC_CONTAINER_T *ctx,
   VC_CONTAINER_CONTROL_T operation, va_list args )
{
   VC_CONTAINER_ES_FORMAT_T *format;

   switch (operation)
   {
   case VC_CONTAINER_CONTROL_TRACK_ADD:
      format = (VC_CONTAINER_ES_FORMAT_T *)va_arg(args, VC_CONTAINER_ES_FORMAT_T *);
      return rtpdump_write_add_track(ctx, format);

   case VC_CONTAINER_CONTROL_TRACK_ADD_DONE:
      rtpdump_write_header( ctx );
      return VC_CONTAINER_SUCCESS;

   default: return VC_CONTAINER_ERROR_UNSUPPORTED_OPERATION;
   }
}

/*****************************************************************************/
VC_CONTAINER_STATUS_T rtpdump_writer_open( VC_CONTAINER_T *ctx )
{
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_ERROR_FORMAT_INVALID;
   const char *extension = vc_uri_path_extension(ctx->priv->uri);
   VC_CONTAINER_MODULE_T *module;

   /* Check if the user has specified a container */
   vc_uri_find_query(ctx->priv->uri, 0, "container", &extension);

   /* Check we're the right writer for this */
   if(!extension)
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;
   if(strcasecmp(extension, "rtpdump"))
      return VC_CONTAINER_ERROR_FORMAT_NOT_SUPPORTED;

   LOG_DEBUG(ctx, "using rtpdump writer");

   /* Allocate our context */
   module = malloc(sizeof(*module));
   if (!module) { status = VC_CONTAINER_ERROR_OUT_OF_MEMORY; goto error; }
   memset(module, 0, sizeof(*module));
   ctx->priv->module = module;
   ctx->tracks = &module->track;

   ctx->priv->pf_close = rtpdump_writer_close;
   ctx->priv->pf_write = rtpdump_writer_write;
   ctx->priv->pf_control = rtpdump_writer_control;
   return VC_CONTAINER_SUCCESS;

 error:
   LOG_DEBUG(ctx, "rtpdump: error opening stream (%i)", status);
   return status;
}

/********************************************************************************
 Entrypoint function
 ********************************************************************************/

#if !defined(ENABLE_CONTAINERS_STANDALONE) && defined(__HIGHC__)
# pragma weak writer_open rtpdump_writer_open
#endif
