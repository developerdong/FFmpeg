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
#include <inttypes.h>
#include "jtt1078.h"
#include "libavutil/opt.h"

static const AVCodecTag jtt1078_codec_ids[] = {
        {AV_CODEC_ID_ADPCM_G722,   G722},
        {AV_CODEC_ID_PCM_ALAW,     G711A},
        {AV_CODEC_ID_PCM_MULAW,    G711U},
        {AV_CODEC_ID_ADPCM_G726LE, G726},
        {AV_CODEC_ID_PCM_S16BE,    S16BE_MONO},
        {AV_CODEC_ID_AAC,          AAC},
        {AV_CODEC_ID_MP3,          MP3},
        {AV_CODEC_ID_H264,         H264},
        {AV_CODEC_ID_H265,         H265},
        {AV_CODEC_ID_NONE,         0}
};

typedef struct JTT1078Context {
    const AVClass *class;
    int version;
    char *sim_card;
    int logical_channel;
    uint16_t packet_number;
    int64_t last_i_dts;
    int64_t last_dts;
} JTT1078Context;

static int jtt1078_init(struct AVFormatContext *s) {
    // set initial packet number
    JTT1078Context *jtt1078 = s->priv_data;
    jtt1078->packet_number = 0;
    // set timestamp
    jtt1078->last_i_dts = AV_NOPTS_VALUE;
    jtt1078->last_dts = AV_NOPTS_VALUE;
    // set time base
    for (unsigned int i = 0; i < s->nb_streams; i++) {
        s->streams[i]->time_base = av_make_q(1, 1000);
    }
    return 0;
}

static DataType jtt1078_get_video_frame_type(AVPacket *pkt, PayloadType payload_type) {
    if (payload_type == H264 || payload_type == H265) {
        int side_data_size;
        uint8_t *side_data = av_packet_get_side_data(pkt, AV_PKT_DATA_QUALITY_STATS, &side_data_size);
        if (side_data != NULL && side_data_size >= 5) {
            switch (side_data[4]) {
                case AV_PICTURE_TYPE_I:
                    return VIDEO_I_FRAME;
                case AV_PICTURE_TYPE_P:
                    return VIDEO_P_FRAME;
                case AV_PICTURE_TYPE_B:
                    return VIDEO_B_FRAME;
            }
        }
    }
    return RAW_DATA;
}

static int jtt1078_write_packet(AVFormatContext *format, AVPacket *pkt) {
    JTT1078Context *jtt1078 = format->priv_data;
    AVIOContext * io = format->pb;
    AVCodecParameters *par = format->streams[pkt->stream_index]->codecpar;
    if (jtt1078->version == 2016) {
        // hex chars to bytes
        uint8_t sim_card[6];
        sscanf(jtt1078->sim_card, "%2"SCNx8"%2"SCNx8"%2"SCNx8"%2"SCNx8"%2"SCNx8"%2"SCNx8,
               sim_card, sim_card + 1, sim_card + 2, sim_card + 3, sim_card + 4, sim_card + 5);
        // transform ff codec id to jtt1078 payload type
        PayloadType payload_type = ff_codec_get_tag(jtt1078_codec_ids, par->codec_id);
        if (par->codec_id == AV_CODEC_ID_AAC) {
            switch (par->profile) {
                case FF_PROFILE_AAC_LOW:
                case FF_PROFILE_MPEG2_AAC_LOW:
                    payload_type = AACLC;
                    break;
                case FF_PROFILE_AAC_HE:
                case FF_PROFILE_AAC_HE_V2:
                case FF_PROFILE_MPEG2_AAC_HE:
                    payload_type = HEAAC;
                    break;
                default:
                    payload_type = AAC;
            }
        } else if (par->codec_id == AV_CODEC_ID_PCM_S16BE) {
            if (par->channel_layout == AV_CH_LAYOUT_STEREO || par->channels == 2) {
                payload_type = S16BE_STEREO;
            } else {
                payload_type = S16BE_MONO;
            }
        }
        // transform ff media type to jtt1078 data type
        DataType data_type;
        switch (par->codec_type) {
            case AVMEDIA_TYPE_VIDEO:
                data_type = jtt1078_get_video_frame_type(pkt, payload_type);
                break;
            case AVMEDIA_TYPE_AUDIO:
                data_type = AUDIO_FRAME;
                break;
            case AVMEDIA_TYPE_DATA:
                data_type = RAW_DATA;
                break;
            default:
                return AVERROR_INVALIDDATA;
        }
        // begin to transfer data
        int sent = 0;
        while (sent < pkt->size) {
            // write frame header flag
            avio_wb32(io, 0x30316364);
            avio_w8(io, 0b10000001);
            // write frame boundary and payload type
            if (pkt->size - sent <= 950) {
                avio_w8(io, 0b10000000 | payload_type);
            } else {
                avio_w8(io, payload_type);
            }
            // write the serial number of packet
            avio_wb16(io, jtt1078->packet_number++);
            // write sim card number
            avio_write(io, sim_card, 6);
            // write logical channel number
            avio_w8(io, jtt1078->logical_channel);
            // write data type and packet type
            PacketType packet_type;
            if (pkt->size <= 950) {
                packet_type = ATOMIC_PACKET;
            } else if (sent == 0) {
                packet_type = FIRST_PACKET;
            } else if (pkt->size - sent <= 950) {
                packet_type = LAST_PACKET;
            } else {
                packet_type = INTERMEDIATE_PACKET;
            }
            avio_w8(io, (data_type << 4) | packet_type);
            if (data_type != RAW_DATA) {
                // write timestamp
                avio_wb64(io, pkt->pts);
                if (data_type != AUDIO_FRAME) {
                    // write last i frame interval
                    avio_wb16(io, jtt1078->last_i_dts == AV_NOPTS_VALUE ? 0 : pkt->dts - jtt1078->last_i_dts);
                    if (data_type == VIDEO_I_FRAME && (packet_type == ATOMIC_PACKET || packet_type == LAST_PACKET)) {
                        jtt1078->last_i_dts = pkt->dts;
                    }
                    // write last frame interval
                    avio_wb16(io, jtt1078->last_dts == AV_NOPTS_VALUE ? 0 : pkt->dts - jtt1078->last_dts);
                    if (packet_type == ATOMIC_PACKET || packet_type == LAST_PACKET) {
                        jtt1078->last_dts = pkt->dts;
                    }
                }
            }
            // write subsequent data length
            uint16_t data_length = (pkt->size - sent <= 950) ? (pkt->size - sent) : 950;
            avio_wb16(io, data_length);
            // write frame data
            avio_write(io, pkt->data + sent, data_length);
            sent += data_length;
        }
        return 0;
    }
    return AVERROR_PATCHWELCOME;
}

#define OFFSET(x) offsetof(JTT1078Context, x)
static const AVOption options[] = {
        {"version",         "Published year of JT/T 1078",              OFFSET(version),         AV_OPT_TYPE_INT,    {.i64=2016},           2016,    INT_MAX, AV_OPT_FLAG_ENCODING_PARAM},
        {"sim_card",        "SIM card number of the device (12 chars)", OFFSET(sim_card),        AV_OPT_TYPE_STRING, {.str="000000000000"}, INT_MIN, INT_MAX, AV_OPT_FLAG_ENCODING_PARAM},
        {"logical_channel", "Logical channel number",                   OFFSET(logical_channel), AV_OPT_TYPE_INT,    {.i64=1},              1,       37,      AV_OPT_FLAG_ENCODING_PARAM},
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
        .audio_codec    = AV_CODEC_ID_AAC,
        .video_codec    = AV_CODEC_ID_H264,
        .init           = jtt1078_init,
        .write_packet   = jtt1078_write_packet,
        .codec_tag      = (const AVCodecTag *const[]) {jtt1078_codec_ids, NULL},
        .flags          = AVFMT_VARIABLE_FPS | AVFMT_NODIMENSIONS | AVFMT_SEEK_TO_PTS,
        .priv_data_size = sizeof(JTT1078Context),
        .priv_class     = &jtt1078_muxer_class,
};
#endif
