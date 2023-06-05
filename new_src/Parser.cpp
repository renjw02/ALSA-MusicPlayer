#include "Parser.h"

#include <thread>

Parser::Parser() {
    packet = av_packet_alloc();
    frame = av_frame_alloc();
}

Parser::~Parser() {
    release();
    av_frame_free(&frame);
    av_packet_free(&packet);
}

void Parser::openFile(char const file_path[]) {
    // 获取打开文件的锁
    std::lock_guard<std::mutex> lock(openFileMutex);

    isFileOpened = false;

    std::cerr << "Open file name: " << file_path << std::endl;

    // 假如重新打开的话需要 release， release 一个空指针不会出错
    release();

    format_ctx = avformat_alloc_context();


    if (!format_ctx || !packet || !frame) {
        throw(std::runtime_error("Failed to alloc"));
    }

    // 打开音频文件
    if (avformat_open_input(&format_ctx, file_path, NULL, NULL) != 0) {
        throw(std::runtime_error("Failed to open audio file"));
    }

    // 获取音频流信息
    if (avformat_find_stream_info(format_ctx, NULL) < 0) {
        throw(std::runtime_error("Failed to find stream information"));
    }

    // 寻找音频流
    stream_index = av_find_best_stream(format_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
    if (stream_index < 0) {
        throw(std::runtime_error("Failed to find audio stream"));
    }

    // 初始化音频解码器上下文
    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        throw(std::runtime_error("Failed to allocate codec context"));
    }

    // 设置解码器上下文参数
    if (avcodec_parameters_to_context(codec_ctx, format_ctx->streams[stream_index]->codecpar) < 0) {
        throw(std::runtime_error("Failed to copy codec parameters to codec context"));
    }


    // 打开音频解码器
    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        throw(std::runtime_error("Failed to open codec"));
    }

    std::cerr 
            << "输入音乐, 解码器名称: " << codec->name 
            << " 通道数: " << codec_ctx->channels
            << " 采样率: " << codec_ctx->sample_rate 
            << " 通道布局: " << av_get_default_channel_layout(codec_ctx->channels)
            << " 采样格式: " << av_get_sample_fmt_name(codec_ctx->sample_fmt) << std::endl << std::flush;

    std::cerr 
        << "Parser 输出格式, 通道数: " << outChannel
        << " 采样率: " << outSampleRate
        << " 通道布局: " << av_get_default_channel_layout(outChannel)
        << " 采样格式: " << av_get_sample_fmt_name(outFormat) << std::endl << std::flush;

    // 获取音频转码器并设置采样参数初始化
    swr_ctx = swr_alloc_set_opts(0,
                                av_get_default_channel_layout(outChannel),
                                outFormat,
                                outSampleRate,
                                av_get_default_channel_layout(codec_ctx->channels),
                                codec_ctx->sample_fmt,
                                codec_ctx->sample_rate,
                                0,
                                0);
    ret = swr_init(swr_ctx);
    if (ret < 0) {
        throw(std::runtime_error("Failed to swr_init(pSwrContext)"));
    }

    std::cerr << "swr_init success" << std::endl << std::flush;

    if (init_atempo_filter(&filter_graph, &in_ctx, &out_ctx, std::to_string(currentTempo).c_str()) != 0) {
        throw(std::runtime_error("Codec not init audio filter!"));
    }

    std::cerr << "avfilter_init success..." << std::endl << std::flush;

    isFileOpened = true;
}

int Parser::init_atempo_filter(AVFilterGraph **pGraph, AVFilterContext **src, AVFilterContext **out, const char *value) {
    // 格式化filter init的parameter
    std::string sampleRate = std::to_string(codec_ctx->sample_rate);
    std::string sampleFmt = std::string(av_get_sample_fmt_name(codec_ctx->sample_fmt));
    std::string sampleChannel;
    if (codec_ctx->channels == 1) {
        sampleChannel = "mono";
    } else {
        sampleChannel = "stereo";
    }

    std::string abufferString = "sample_rate=" + sampleRate + ":sample_fmt=" + sampleFmt + ":channel_layout=" + sampleChannel;
    std::string aformatString = "sample_fmts=" + sampleFmt + ":sample_rates=" + sampleRate + ":channel_layouts=" + sampleChannel;

    // init
    AVFilterGraph *graph = avfilter_graph_alloc();
    
    // accept abuffer for receving input
    const AVFilter *abuffer = avfilter_get_by_name("abuffer");
    AVFilterContext *abuffer_ctx = avfilter_graph_alloc_filter(graph, abuffer, "src");

    // set parameter: 匹配原始音频采样率sample rate，数据格式sample_fmt， channel_layout声道
    if (avfilter_init_str(abuffer_ctx, abufferString.c_str()) < 0) {
        throw std::runtime_error( "error init abuffer filter\n");
    } 

    // init atempo filter
    const AVFilter *atempo = avfilter_get_by_name("atempo");
    AVFilterContext *atempo_ctx = avfilter_graph_alloc_filter(graph, atempo, "atempo");

    // 这里采用av_dict_set设置参数
    AVDictionary *args = NULL;
    av_dict_set(&args, "tempo", value, 0);//这里传入外部参数，可以动态修改
    if (avfilter_init_dict(atempo_ctx, &args) < 0) {
        throw std::runtime_error("error init atempo filter\n");
    }

    const AVFilter *aformat = avfilter_get_by_name("aformat");
    AVFilterContext *aformat_ctx = avfilter_graph_alloc_filter(graph, aformat, "aformat");
    if (avfilter_init_str(aformat_ctx, aformatString.c_str()) < 0) {
        throw std::runtime_error("error init aformat filter");
    }

    // 初始化sink用于输出
    const AVFilter *sink = avfilter_get_by_name("abuffersink");
    AVFilterContext *sink_ctx = avfilter_graph_alloc_filter(graph, sink, "sink");

    if (avfilter_init_str(sink_ctx, NULL) < 0) {//无需参数
        throw std::runtime_error("error init sink filter\n");
    }

    // 链接各个filter上下文
    if (avfilter_link(abuffer_ctx, 0, atempo_ctx, 0) != 0) {
        throw std::runtime_error("error link to atempo filter\n");
    }
    if (avfilter_link(atempo_ctx, 0, aformat_ctx, 0) != 0) {
        throw std::runtime_error("error link to aformat filter\n");
    }
    if (avfilter_link(aformat_ctx, 0, sink_ctx, 0) != 0) {
        throw std::runtime_error("error link to sink filter\n");
    }
    if (avfilter_graph_config(graph, NULL) < 0) {
        throw std::runtime_error("error config filter graph\n");
    }

    *pGraph = graph;
    *src = abuffer_ctx;
    *out = sink_ctx;
    return 0;
}

void Parser::parse(std::function<void(void *, size_t)> callback) {

    // 用来存储返回值
    int ret = 0;
    
    while (true) {
        if (!isFileOpened) {
            // 进入等待状态，直到 isFileOpened 为 true，没开文件播放个锤子
            continue;
        }
        if (!isPlaying) {
            // avcodec_flush_buffers( codec_ctx );
            // 进入等待状态，直到 isPlaying 为 true
            while(!isPlaying) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
            continue;
        }
        {
            std::lock_guard<std::mutex> lock(tempoMutex);
            if (isTempoChanged) {
                avfilter_graph_free(&filter_graph);
                if (init_atempo_filter(&filter_graph, &in_ctx, &out_ctx, std::to_string(targetTempo).c_str()) != 0) {
                    av_log(NULL, AV_LOG_ERROR, "Codec not init audio filter!");
                    throw(std::runtime_error("Codec not init audio filter!"));
                }
                isTempoChanged = false;

                avcodec_flush_buffers( codec_ctx );
            }
        }
        // 如果有跳转 
        {
            // 获取锁
            std::lock_guard<std::mutex> lock(jumpMutex);
            if(haveJumpSignal) {
                // 跳转到目标时间戳
                // std::cout << "Jump Target: " << jumpTarget << std::endl; 
                AVStream * stream = format_ctx->streams[packet->stream_index];
                int64_t target = jumpTarget / av_q2d(stream->time_base);
                ret = av_seek_frame(format_ctx, packet->stream_index, target, AVSEEK_FLAG_ANY);
                if (ret < 0) {
                    av_strerror(ret, errors, ERROR_STR_SIZE);
                    av_log(NULL, AV_LOG_ERROR, "Failed to av_seek_frame, %d(%s)\n", ret, errors);
                    throw std::runtime_error("Failed to av_seek_framem, " + std::string(errors));
                }
                haveJumpSignal = false;
                
                avcodec_flush_buffers( codec_ctx );
            }
        }

        {
            // 这块不能和文件读取一起进行
            std::lock_guard<std::mutex> lock(openFileMutex);
            
            // 读取一帧数据的数据包
            ret = av_read_frame(format_ctx, packet);
            if(ret < 0) {
                // 读取失败, 文件读完了
                isPlaying = false;
                continue; 
            }
            // 将封装包发往解码器
            if (packet->stream_index == stream_index) {
                ret = avcodec_send_packet(codec_ctx, packet);
                if (ret < 0) {
                    av_strerror(ret, errors, ERROR_STR_SIZE);
                    av_log(NULL, AV_LOG_ERROR, "Failed to avcodec_send_packet, %d(%s)\n", ret, errors);
                    throw std::runtime_error("Failed to avcodec_send_packet, " + std::string(errors));
                }

                while (true) {
                    ret = avcodec_receive_frame(codec_ctx, frame);
                    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                        break;
                    } else if (ret < 0) {
                        av_log(NULL, AV_LOG_ERROR, "Failed to avcodec_receive_frame, ret %d\n", ret);
                        throw std::runtime_error("Failed to avcodec_receive_frame");
                    }
                    {
                        // 获取锁
                        std::lock_guard<std::mutex> lock(currentPosMutex);
                        AVStream * stream = format_ctx->streams[packet->stream_index];
                        currentPos = frame->pts * av_q2d(stream->time_base);
                    }

                    // 修改采样率参数后，需要重新获取采样点的样本个数
                    uint8_t *buffer[outChannel];


                    // filter
                    if (av_buffersrc_add_frame(in_ctx, frame) < 0) {
                        av_log(NULL, AV_LOG_ERROR, "Failed to allocate filtered frame\n");
                        break;
                    }

                    while (true) {
                        ret = av_buffersink_get_frame(out_ctx, frame);
                        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                            break;
                        } else if (ret < 0) {
                            av_log(NULL, AV_LOG_ERROR, "Failed to av_buffersink_get_frame, ret %d\n", ret);
                            throw std::runtime_error("Failed to av_buffersink_get_frame, ret " + std::to_string(ret));
                        }
                        
                        // 获取每个采样点的字节大小
                        numBytes = av_get_bytes_per_sample(outFormat);
                        outNbSamples = av_rescale_rnd(frame->nb_samples, outSampleRate, codec_ctx->sample_rate, AV_ROUND_ZERO);
                        av_samples_alloc(buffer, NULL, outChannel, outNbSamples, outFormat, 0);


                        // 重采样
                        int realOutNbSamples = swr_convert(swr_ctx, buffer, outNbSamples, (const uint8_t **)frame->data, frame->nb_samples);

                        // 第一次显示
                        static int show = 1;
                        if (show == 1) {
                            av_log(NULL, AV_LOG_INFO, "numBytes: %d nb_samples: %d to outNbSamples: %d\n", numBytes, frame->nb_samples, outNbSamples);
                            show = 0;
                        }

                        uint8_t output_buffer[numBytes * realOutNbSamples * outChannel];
                        int cnt = 0;

                        // 使用 LRLRLRLRLRL（采样点为单位，采样点有几个字节，交替存储到文件，可使用pcm播放器播放）
                        for (int index = 0; index < realOutNbSamples; index++) {
                            for (int channel = 0; channel < codec_ctx->channels; channel++) {
                                for(int i = 0; i < numBytes; i++) {
                                    output_buffer[cnt++] = buffer[channel][numBytes * index + i];
                                }
                            }
                        }
                        assert(cnt == numBytes * realOutNbSamples * outChannel);
                        callback(output_buffer, realOutNbSamples);

                        av_frame_unref(frame);

                        av_freep(&buffer[0]);
                    }

                    av_packet_unref(packet);
                }
            }
        }
    }

    fclose(outfile);
}

void Parser::play()
{
    isPlaying = true;
}

void Parser::pause() {
    isPlaying = false;
}

void Parser::release() {
    // 释放回收资源
    avformat_close_input(&format_ctx);
    avfilter_graph_free(&filter_graph);
    swr_free(&swr_ctx);
    avcodec_free_context(&codec_ctx);
}