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

/*
Common definitions between simple reader and writer.

This simple format consists of a main metadata file which references one or
more elementary stream files.
The metadata file starts with a header describing the elementary streams
which are available. This header has the following form:

S1MPL3
TRACK video, h264, 1920, 1080
URI elementary_stream.h264
TRACK audio, mp4a, 2, 44100, 0, 0
URI elementary_stream.mp4a
3LPM1S

The first field after the track identifier is the type of stream (video, audio,
subpicture), followed by the fourcc of the codec.
For video streams, this is followed by the width and height.
For audio streams, this is followed by the number of channels, sample rate,
bits per sample and block alignment.

Following the header, each line represents a packet of data in the form:
<track_num> <size> <pts> <flags>
*/

#ifndef SIMPLE_COMMON_H
#define SIMPLE_COMMON_H

#define SIGNATURE_STRING "S1MPL3"
#define SIGNATURE_END_STRING "3LPM1S"

/** List of configuration options supported in the header */
#define CONFIG_VARIANT                  "VARIANT"
#define CONFIG_URI                      "URI"
#define CONFIG_CODEC_VARIANT            "CODEC_VARIANT"
#define CONFIG_BITRATE                  "BITRATE"
#define CONFIG_UNFRAMED                 "UNFRAMED"
#define CONFIG_VIDEO_CROP               "VIDEO_CROP"
#define CONFIG_VIDEO_ASPECT             "VIDEO_ASPECT"

#endif /* SIMPLE_COMMON_H */
