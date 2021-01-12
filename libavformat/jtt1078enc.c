/*
 * JT/T 1078 muxer
 * Copyright 2021 Zhibin Dong <developerdong@gmail.com>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "internal.h"
#include "jtt1078.h"
#include "libavutil/opt.h"

static const AVCodecTag jtt1078_codec_ids[] = {
        {AV_CODEC_ID_ADPCM_G722,   G722},
        {AV_CODEC_ID_G723_1,       G723},
        {AV_CODEC_ID_G729,         G729},
        {AV_CODEC_ID_PCM_ALAW,     G711A},
        {AV_CODEC_ID_PCM_MULAW,    G711U},
        {AV_CODEC_ID_ADPCM_G726LE, G726},
        {AV_CODEC_ID_AAC,          AAC},
        {AV_CODEC_ID_MP3,          MP3},
        {AV_CODEC_ID_ADPCM_YAMAHA, ADPCMA},
        {AV_CODEC_ID_AMR_NB,       AMR},
        {AV_CODEC_ID_H264,         H264},
        {AV_CODEC_ID_H265,         H265},
        {AV_CODEC_ID_CAVS,         AVS},
        {AV_CODEC_ID_NONE,         0}
};

typedef struct JTT1078Context {
    const AVClass *class;
    int version;
    char *sim_no;
    int channel_no;
} JTT1078Context;

static int jtt1078_write_packet(AVFormatContext *format, AVPacket *pkt) {
    return 0;
}

#define OFFSET(x) offsetof(JTT1078Context, x)
static const AVOption options[] = {
        {"version",    "Published year of JT/T 1078",              OFFSET(version),    AV_OPT_TYPE_INT,    {.i64=2016}, 2016,              INT_MAX, AV_OPT_FLAG_ENCODING_PARAM},
        {"sim_no",     "SIM card number of the device (12 chars)", OFFSET(sim_no),     AV_OPT_TYPE_STRING, {.str="000000000000"}, INT_MIN, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM},
        {"channel_no", "Logical channel number",                   OFFSET(channel_no), AV_OPT_TYPE_INT,    {.i64=0},    1, 37,                      AV_OPT_FLAG_ENCODING_PARAM},
        {NULL},
};

#if CONFIG_JTT1078_MUXER
static const AVClass jtt1078_muxer_class = {
        .class_name = "JT/T 1078 muxer",
        .item_name  = av_default_item_name,
        .option     = options,
        .version    = LIBAVUTIL_VERSION_INT,
};

AVOutputFormat ff_jtt1078_muxer = {
        .name           = "jtt1078",
        .long_name      = NULL_IF_CONFIG_SMALL("JT/T 1078"),
        .write_packet   = jtt1078_write_packet,
        .codec_tag      = jtt1078_codec_ids,
        .flags          = AVFMT_VARIABLE_FPS | AVFMT_NODIMENSIONS | AVFMT_SEEK_TO_PTS,
        .priv_data_size = sizeof(JTT1078Context),
        .priv_class     = &jtt1078_muxer_class,
};
#endif
