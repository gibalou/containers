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
#include "core/containers_metadata.h"

/*****************************************************************************/
static struct {
   VC_CONTAINER_METADATA_KEY_T key;
   const char *name;
} meta_key_conv[] =
{  {VC_CONTAINER_METADATA_KEY_TITLE, "title"},
   {VC_CONTAINER_METADATA_KEY_ARTIST, "artist"},
   {VC_CONTAINER_METADATA_KEY_ALBUM, "album"},
   {VC_CONTAINER_METADATA_KEY_DESCRIPTION, "description"},
   {VC_CONTAINER_METADATA_KEY_YEAR, "year"},
   {VC_CONTAINER_METADATA_KEY_GENRE, "genre"},
   {VC_CONTAINER_METADATA_KEY_TRACK, "track"},
   {VC_CONTAINER_METADATA_KEY_LYRICS, "lyrics"},
   {VC_CONTAINER_METADATA_KEY_COMMENTS, "comments"},
   {VC_CONTAINER_METADATA_KEY_ENCODEDBY, "encoded_by"},
   {VC_CONTAINER_METADATA_KEY_COPYRIGHT, "copyright"},
   {VC_CONTAINER_METADATA_KEY_UNKNOWN, 0} };

const char *vc_container_metadata_id_to_string(VC_CONTAINER_METADATA_KEY_T key)
{
   int i;
   for(i = 0; meta_key_conv[i].key != VC_CONTAINER_METADATA_KEY_UNKNOWN; i++ )
      if(meta_key_conv[i].key == key) break;
   return meta_key_conv[i].name;
}

/*****************************************************************************/
VC_CONTAINER_METADATA_T *vc_container_metadata_append(VC_CONTAINER_T *ctx,
   VC_CONTAINER_METADATA_KEY_T key, unsigned int size, VC_CONTAINER_STATUS_T *p_status)
{
   VC_CONTAINER_METADATA_T *meta = NULL, **p_meta;
   VC_CONTAINER_STATUS_T status = VC_CONTAINER_ERROR_ALREADY_EXISTS;
   unsigned int i;

   for (i = 0; i != ctx->meta_num; ++i)
   {
      if (key == ctx->meta[i]->key) break;
   }

   /* Avoid duplicate entries for now */
   if (i < ctx->meta_num) goto error;

   /* Allocate a new metadata entry */
   status = VC_CONTAINER_ERROR_OUT_OF_MEMORY;
   if((meta = malloc(sizeof(VC_CONTAINER_METADATA_T) + size)) == NULL)
      goto error;

   /* We need to grow the array holding the metadata entries somehow, ideally,
      we'd like to use a linked structure of some sort but realloc is probably
      okay in this case */
   if((p_meta = realloc(ctx->meta, sizeof(VC_CONTAINER_METADATA_T *) * (ctx->meta_num + 1))) == NULL)
   {
      free(meta);
      meta = NULL;
      goto error;
   }

   status = VC_CONTAINER_SUCCESS;
   ctx->meta = p_meta;
   memset(meta, 0, sizeof(VC_CONTAINER_METADATA_T) + size);
   ctx->meta[ctx->meta_num] = meta;
   meta->key = key;
   meta->value = (char *)&meta[1];
   meta->size = size;
   ctx->meta_num++;

 error:
   if(p_status) *p_status = status;
   return meta;
}
