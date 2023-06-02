#ifndef RESOLVE_WAV_WAVE_H
#define RESOLVE_WAV_WAVE_H
 
#include <stdint.h>
#pragma pack (2) /*指定按2字节对齐*/
 
typedef struct WAV_RIFF {
    /* RIFF标志 */
    char ChunkID[4];     /* "RIFF" */
    /* 文件大小 */
    uint32_t ChunkSize;  /* 36 + Subchunk2Size */
    /* 文件格式 */
    char Format[4];      /* "WAVE" */
} RIFF_t;
 
typedef struct WAV_FMT {
    /* 格式块标识 */
    char Subchunk1ID[4];    /* "fmt " */
    /* 格式块长度 */
    uint32_t Subchunk1Size; /* 16 for PCM */

    uint16_t AudioFormat;   /* 编码格式代码*/
    uint16_t NumChannels;   /* 声道数 */
    uint32_t SampleRate;    /* 采样频率 */
    uint32_t ByteRate;      /* 传输速率 */
    uint16_t BlockAlign;    /* 数据块对齐单位 */
    uint16_t BitsPerSample; /* 采样位数(长度) */
} FMT_t;
 
typedef struct WAV_data {
    /* sub-chunk "data" */
    char Subchunk2ID[4];    /* "data" */
    /* sub-chunk-size */
    uint32_t Subchunk2Size; /* data size */
    /* sub-chunk-data */
//    Data_block_t block;
} Data_t;
 
//typedef struct WAV_data_block {
//} Data_block_t;
 
typedef struct WAV_fotmat {
    RIFF_t riff;
    FMT_t fmt;
    Data_t data;
} Wav;
 
#pragma pack () /*取消指定对齐，恢复缺省对齐*/
#endif //RESOLVE_WAV_WAVE_H