//
// Created by zhibin on 2020/12/16.
//

#ifndef FFMPEG_JTT1078_H
#define FFMPEG_JTT1078_H

typedef enum DataType {
    VIDEO_I_FRAME = 0b0000,
    VIDEO_P_FRAME = 0b0001,
    VIDEO_B_FRAME = 0b0010,
    AUDIO_FRAME = 0b0011,
    RAW_DATA = 0b0100,
} DataType;

typedef enum PacketType {
    ATOMIC_PACKET = 0b0000,
    FIRST_PACKET = 0b0001,
    LAST_PACKET = 0b0010,
    INTERMEDIATE_PACKET = 0b0011,
} PacketType;

typedef enum PayloadType {
    G721 = 1,
    G722 = 2,
    G723 = 3,
    G728 = 4,
    G729 = 5,
    G711A = 6,
    G711U = 7,
    G726 = 8,
    G729A = 9,
    DVI4_3 = 10,
    DVI4_4 = 11,
    DVI4_8K = 12,
    DVI4_16K = 13,
    LPC = 14,
    S16BE_STEREO = 15,
    S16BE_MONO = 16,
    MPEGAUDIO = 17,
    LPCM = 18,
    AAC = 19,
    WMA9STD = 20,
    HEAAC = 21,
    PCM_VOICE = 22,
    PCM_AUDIO = 23,
    AACLC = 24,
    MP3 = 25,
    ADPCMA = 26,
    MP4AUDIO = 27,
    AMR = 28,
    RAW = 91,
    H264 = 98,
    H265 = 99,
    AVS = 100,
    SVAC = 101,
} PayloadType;

#endif //FFMPEG_JTT1078_H
