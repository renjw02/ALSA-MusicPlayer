#pragma once

#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <unistd.h>
#include <functional>
#include <mutex>

extern "C" {
    #include <alsa/asoundlib.h>
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libswresample/swresample.h>
    #include <libavutil/samplefmt.h>
    #include <libavfilter/avfilter.h>
    #include <libavfilter/buffersink.h>
    #include <libavfilter/buffersrc.h>
}

#define ERROR_STR_SIZE 1024

class Parser {
private:
    AVFormatContext *format_ctx = nullptr;
    AVCodecContext *codec_ctx = nullptr;
    AVCodec *codec = nullptr;
    AVPacket *packet = nullptr;
    AVFrame *frame = nullptr;
    SwrContext *swr_ctx = nullptr;
    FILE *outfile = nullptr;
    // FILE *fp = nullptr;

    // atempo filter
    AVFilterContext* in_ctx = nullptr;
    AVFilterContext* out_ctx = nullptr;
    AVFilterGraph *filter_graph = nullptr;

    int ret = 0;
    int numBytes = 0;

    int outNbSamples = 0; // 目标 frame 的 sample 个数
    int outChannel = 2;  // 重采样后输出的通道
    int outSampleRate = 44100;  // 重采样后输出的采样率
    enum AVSampleFormat outFormat = AV_SAMPLE_FMT_S16P; // 重采样后输出的格式

    // 是否已经打开文件
    volatile bool isFileOpened = false;
    std::mutex openFileMutex;

    volatile bool isPlaying = false;

    bool jumpSignal = false;

    double jumpTarget = 0.0;
    std::mutex jumpMutex;
    
    double currentTime = 0.0;
    std::mutex currentTimeMutex;

    double currentTempo = 1.0;
    double targetTempo = 0.0;
    bool tempoSignal = false;
    std::mutex tempoMutex;

    int stream_index = -1;

    char errors[ERROR_STR_SIZE];

    int init_atempo_filter(AVFilterGraph **pGraph, AVFilterContext **src, AVFilterContext **out, const char *value);

public:
    Parser();
    ~Parser();
    
    void openFile(char const file_path[]);
    void release();
    void parse(std::function<void(void *, size_t)> callback);

    bool jump(double jumpTarget);

    void play();

    void pause();

    double getCurrentTime();

    double getTotalTime();

    bool setTempo(double tempo);
};