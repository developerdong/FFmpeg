/*
 * JT/T 1078 demuxer
 * Copyright 2020 Zhibin Dong <developerdong@gmail.com>
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
#include <stdbool.h>

typedef struct JTT1078Context {
    const AVClass *class;
    int version;
} JTT1078Context;

static int jtt1078_probe(const AVProbeData *p) {
    const uint8_t *d = p->buf;
    if (d[0] == 0x30 && d[1] == 0x31 && d[2] == 0x63 && d[3] == 0x64 && d[4] == 0b10000001) {
        return AVPROBE_SCORE_MAX;
    }
    return 0;
}

static int jtt1078_read_header(AVFormatContext *s) {
    s->ctx_flags |= AVFMTCTX_NOHEADER;
    return 0;
}

static enum AVMediaType jtt1078_data_type_to_media_type(DataType data_type) {
    switch (data_type) {
        case VIDEO_I_FRAME:
        case VIDEO_P_FRAME:
        case VIDEO_B_FRAME:
            return AVMEDIA_TYPE_VIDEO;
        case AUDIO_FRAME:
            return AVMEDIA_TYPE_AUDIO;
        case RAW_DATA:
            return AVMEDIA_TYPE_DATA;
        default:
            return AVMEDIA_TYPE_UNKNOWN;
    }
}

static enum AVCodecID jtt1078_payload_type_to_codec_id(PayloadType payload_type) {
    switch (payload_type) {
        case G721:
            return AV_CODEC_ID_NONE;
        case G722:
            return AV_CODEC_ID_ADPCM_G722;
        case G723:
            return AV_CODEC_ID_G723_1;
        case G728:
            return AV_CODEC_ID_NONE;
        case G729:
            return AV_CODEC_ID_G729;
        case G711A:
            return AV_CODEC_ID_PCM_ALAW;
        case G711U:
            return AV_CODEC_ID_PCM_MULAW;
        case G726:
            return AV_CODEC_ID_ADPCM_G726LE;
        case G729A:
            return AV_CODEC_ID_G729;
        case DVI4_3:
        case DVI4_4:
        case DVI4_8K:
        case DVI4_16K:
        case LPC:
            return AV_CODEC_ID_NONE;
        case S16BE_STEREO:
        case S16BE_MONO:
            return AV_CODEC_ID_PCM_S16BE;
        case MPEGAUDIO:
            return AV_CODEC_ID_MP3;
        case LPCM:
            return AV_CODEC_ID_NONE;
        case AAC:
            return AV_CODEC_ID_AAC;
        case WMA9STD:
        case HEAAC:
        case PCM_VOICE:
        case PCM_AUDIO:
        case AACLC:
            return AV_CODEC_ID_NONE;
        case MP3:
            return AV_CODEC_ID_MP3;
        case ADPCMA:
            return AV_CODEC_ID_ADPCM_YAMAHA;
        case MP4AUDIO:
            return AV_CODEC_ID_NONE;
        case AMR:
            return AV_CODEC_ID_AMR_NB;
        case RAW:
            return AV_CODEC_ID_NONE;
        case H264:
            return AV_CODEC_ID_H264;
        case H265:
            return AV_CODEC_ID_H265;
        case AVS:
            return AV_CODEC_ID_CAVS;
        case SVAC:
        default:
            return AV_CODEC_ID_NONE;
    }
}

static bool jtt1078_same_codec(AVCodecParameters *codec_params, DataType data_type, PayloadType payload_type) {
    if (codec_params != NULL &&
        codec_params->codec_type == jtt1078_data_type_to_media_type(data_type) &&
        codec_params->codec_id == jtt1078_payload_type_to_codec_id(payload_type)) {
        return true;
    }
    return false;
}

static int jtt1078_read_packet(AVFormatContext *format, AVPacket *pkt) {
    JTT1078Context *jtt1078 = format->priv_data;
    AVIOContext *io = format->pb;
    if (jtt1078->version == 2016) {
        bool frame_boundary;
        PayloadType payload_type;
        DataType data_type;
        uint64_t timestamp_ms;
        uint16_t data_length;
        int64_t err;
        while (true) {
            // parse the fields of one packet
            err = avio_skip(io, 5);
            if (err < 0) {
                return err;
            }
            uint8_t byte_5 = avio_r8(io);
            frame_boundary = (byte_5 & 0b10000000) > 0;
            payload_type = (PayloadType) (byte_5 & 0b01111111);
            err = avio_skip(io, 9);
            if (err < 0) {
                return err;
            }
            uint8_t byte_15 = avio_r8(io);
            data_type = (DataType) (byte_15 >> 4);
            switch (data_type) {
                case VIDEO_I_FRAME:
                case VIDEO_P_FRAME:
                case VIDEO_B_FRAME:
                    timestamp_ms = avio_rb64(io);
                    err = avio_skip(io, 4);
                    if (err < 0) {
                        return err;
                    }
                    break;
                case AUDIO_FRAME:
                    timestamp_ms = avio_rb64(io);
                    break;
                case RAW_DATA:
                default:
                    return AVERROR_INVALIDDATA;
            }
            data_length = avio_rb16(io);
            if (data_length > 950) {
                return AVERROR_INVALIDDATA;
            }
            if (data_type == AUDIO_FRAME) {
                // check if audio data has hisilicon header
                uint8_t hisilicon_header[4];
                err = avio_read(io, hisilicon_header, 4);
                if (err < 0) {
                    return err;
                }
                if (hisilicon_header[0] == 0x00 &&
                    hisilicon_header[1] == 0x01 &&
                    hisilicon_header[2] == (data_length - 4) / 2 &&
                    hisilicon_header[3] == 0x00) {
                    // we should drop hisilicon header
                    data_length -= 4;
                } else {
                    err = avio_seek(io, -4, SEEK_CUR);
                    if (err < 0) {
                        return err;
                    }
                }
            }
            // av_append_packet may init the packet, so we must set the fields of packet after this function call
            err = av_append_packet(io, pkt, data_length);
            if (err < 0) {
                return err;
            }
            if (frame_boundary) {
                // now we have all the fields we need, it's time to set the fields of packet
                unsigned int stream_index = UINT_MAX;
                for (unsigned int i = 0; i < format->nb_streams; i++) {
                    if (jtt1078_same_codec(format->streams[i]->codecpar, data_type, payload_type)) {
                        stream_index = i;
                        break;
                    }
                }
                if (stream_index == UINT_MAX) {
                    // if we don't have corresponding stream, we should create it
                    AVStream *stream = avformat_new_stream(format, NULL);
                    if (stream == NULL) {
                        return AVERROR(ENOMEM);
                    }
                    // set common params
                    stream->time_base = av_make_q(1, 1000);
                    stream->codecpar->codec_type = jtt1078_data_type_to_media_type(data_type);
                    stream->codecpar->codec_id = jtt1078_payload_type_to_codec_id(payload_type);
                    // set specific params
                    switch (payload_type) {
                        case G711A:
                        case G711U:
                            stream->codecpar->sample_rate = 8000;
                            stream->codecpar->channels = 1;
                            break;
                        case G726:
                            stream->codecpar->sample_rate = 8000;
                            stream->codecpar->bits_per_coded_sample = data_length / 40;
                            stream->codecpar->bit_rate =
                                    stream->codecpar->sample_rate * stream->codecpar->bits_per_coded_sample;
                            stream->codecpar->channels = 1;
                            break;
                        default:
                            break;
                    }
                    // set stream index
                    stream_index = format->nb_streams - 1;
                }
                // now we have corresponding stream
                pkt->pts = timestamp_ms;
                pkt->stream_index = (int) stream_index;
                if (data_type == VIDEO_I_FRAME) {
                    pkt->flags |= AV_PKT_FLAG_KEY;
                } else if (data_type == VIDEO_B_FRAME) {
                    pkt->flags |= AV_PKT_FLAG_DISPOSABLE;
                }
                break;
            }
        }
        return 0;
    }
    return AVERROR_PATCHWELCOME;
}

#define OFFSET(x) offsetof(JTT1078Context, x)
static const AVOption options[] = {
    {"version", "Published year of JT/T 1078", OFFSET(version), AV_OPT_TYPE_INT, {.i64=2016}, 2016, INT_MAX, AV_OPT_FLAG_DECODING_PARAM},
    {NULL},
};

#if CONFIG_JTT1078_DEMUXER
static const AVClass jtt1078_demuxer_class = {
    .class_name     = "JT/T 1078 demuxer",
    .item_name      = av_default_item_name,
    .option         = options,
    .version        = LIBAVUTIL_VERSION_INT,
};

AVInputFormat ff_jtt1078_demuxer = {
    .name           = "jtt1078",
    .long_name      = NULL_IF_CONFIG_SMALL("JT/T 1078"),
    .read_probe     = jtt1078_probe,
    .read_header    = jtt1078_read_header,
    .read_packet    = jtt1078_read_packet,
    .priv_data_size = sizeof(JTT1078Context),
    .priv_class     = &jtt1078_demuxer_class,
};
#endif
