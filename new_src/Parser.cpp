#include "Parser.h"

#include <thread>

Parser::Parser() {
    av_register_all();          //旧版本需要注册
    avfilter_register_all();    //旧版本需要注册
    packet = av_packet_alloc();
    frame = av_frame_alloc();
}

Parser::~Parser() {
    release();
    av_frame_free(&frame);
    av_packet_free(&packet);
}

void Parser::openFile(char const file_path[]) {
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

    std::cerr << "输入音乐,格式" << codec->name << " 通道数: " << codec_ctx->channels << " 采样率: " << codec_ctx->sample_rate 
        << " 通道布局: " << av_get_default_channel_layout(codec_ctx->channels) << " 采样格式: " 
        << av_get_sample_fmt_name(codec_ctx->sample_fmt) << std::endl << std::flush;

    std::cerr << "输出格式, 通道数: " << outChannel << " 采样率: " << outSampleRate 
        << " 通道布局: " << av_get_default_channel_layout(outChannel) 
        << " 采样格式: " << av_get_sample_fmt_name(outFormat) << std::endl << std::flush;

    // 获取音频转码器并设置采样参数初始化
    swr_ctx = swr_alloc_set_opts(0,av_get_default_channel_layout(outChannel),outFormat,outSampleRate,
        av_get_default_channel_layout(codec_ctx->channels),codec_ctx->sample_fmt,codec_ctx->sample_rate,0,0);
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
    // std::cerr << "start" << std::endl << std::flush;
    std::string sampleRate = std::to_string(codec_ctx->sample_rate);
    std::string sampleFmt = std::string(av_get_sample_fmt_name(codec_ctx->sample_fmt));
    // std::cerr << "asd" << std::endl << std::flush;
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
    // if(graph!=NULL)
    // std::cerr << "asd1" << std::endl << std::flush;
    // accept abuffer for receving input
    AVFilter *abuffer = avfilter_get_by_name("abuffer");
    //avfilter_register(abuffer);
    // if(abuffer!=NULL)
    // std::cerr << "asd2" << std::endl << std::flush;
    AVFilterContext *abuffer_ctx = avfilter_graph_alloc_filter(graph, abuffer, "abuffer");
    // if(abuffer_ctx!=NULL)
    // std::cerr << "asd3" << std::endl << std::flush;
    // set parameter: 匹配原始音频采样率sample rate，数据格式sample_fmt， channel_layout声道
    if (avfilter_init_str(abuffer_ctx, abufferString.c_str()) < 0) {
        throw std::runtime_error( "error init abuffer filter\n");
    } 
    // std::cerr << "asd" << std::endl << std::flush;
    // init atempo filter
    AVFilter *atempo = avfilter_get_by_name("atempo");
    //avfilter_register(atempo);
    AVFilterContext *atempo_ctx = avfilter_graph_alloc_filter(graph, atempo, "atempo");
    // std::cerr << "asd" << std::endl << std::flush;
    // 这里采用av_dict_set设置参数
    AVDictionary *args = NULL;
    av_dict_set(&args, "tempo", value, 0);//这里传入外部参数，可以动态修改
    if (avfilter_init_dict(atempo_ctx, &args) < 0) {
        throw std::runtime_error("error init atempo filter\n");
    }

    AVFilter *aformat = avfilter_get_by_name("aformat");
    //avfilter_register(aformat);
    AVFilterContext *aformat_ctx = avfilter_graph_alloc_filter(graph, aformat, "aformat");
    if (avfilter_init_str(aformat_ctx, aformatString.c_str()) < 0) {
        throw std::runtime_error("error init aformat filter");
    }
    // std::cerr << "asd" << std::endl << std::flush;
    // 初始化sink用于输出
    AVFilter *sink = avfilter_get_by_name("abuffersink");
    // if(sink==NULL)std::cerr << "asd null" << std::endl << std::flush;
    // std::cerr << "asd" << std::endl << std::flush;
    avfilter_register(sink);
    // std::cerr << "asd" << std::endl << std::flush;
    AVFilterContext *sink_ctx = avfilter_graph_alloc_filter(graph, sink, "sink");
    // if(sink_ctx==NULL)std::cerr << "asd null" << std::endl << std::flush;
    if (avfilter_init_str(sink_ctx, "") < 0) {//无需参数
        throw std::runtime_error("error init sink filter\n");
    }
    // std::cerr << "asd" << std::endl << std::flush;
    // 链接各个filter上下文
    if (avfilter_link(abuffer_ctx, 0, atempo_ctx, 0) != 0) {
        throw std::runtime_error("error link to atempo filter\n");
    }
    // std::cerr << "asd" << std::endl << std::flush;
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
            // 进入等待状态，直到 isFileOpened 为 true
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

        // 处理倍速
        std::lock_guard<std::mutex> lock1(tempoMutex);
        if (tempoSignal) {
            avfilter_graph_free(&filter_graph);
            if (init_atempo_filter(&filter_graph, &in_ctx, &out_ctx, std::to_string(targetTempo).c_str()) != 0) {
                av_log(NULL, AV_LOG_ERROR, "Codec not init audio filter!");
                throw(std::runtime_error("Codec not init audio filter!"));
            }
            tempoSignal = false;

            avcodec_flush_buffers( codec_ctx );
        }


        // 如果有跳转 处理跳转 
        // 获取锁
        std::lock_guard<std::mutex> lock2(jumpMutex);
        if(jumpSignal) {
            AVStream * stream = format_ctx->streams[packet->stream_index];
            int64_t target = jumpTarget / av_q2d(stream->time_base);
            ret = av_seek_frame(format_ctx, packet->stream_index, target, AVSEEK_FLAG_ANY);
            if (ret < 0) {
                av_strerror(ret, errors, ERROR_STR_SIZE);
                av_log(NULL, AV_LOG_ERROR, "Failed to av_seek_frame, %d(%s)\n", ret, errors);
                throw std::runtime_error("Failed to av_seek_framem, " + std::string(errors));
            }
            jumpSignal = false;
            
            avcodec_flush_buffers( codec_ctx );
        }


        std::lock_guard<std::mutex> lock3(openFileMutex);
        
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
                    std::lock_guard<std::mutex> lock(currentTimeMutex);
                    AVStream * stream = format_ctx->streams[packet->stream_index];
                    currentTime = frame->pts * av_q2d(stream->time_base);
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

                    // 存储到文件，可使用pcm播放器播放
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

    fclose(outfile);
}

void Parser::play()
{
    isPlaying = true;
}

void Parser::pause() {
    isPlaying = false;
}

double Parser::getCurrentTime() {
    std::lock_guard<std::mutex> lock(currentTimeMutex);
    return currentTime;
}

double Parser::getTotalTime() {
    return double(format_ctx->duration) / AV_TIME_BASE;
}

bool Parser::jump(double jumpTarget) {
    std::lock_guard<std::mutex> lock(jumpMutex);
    if(jumpTarget < 0){
        jumpTarget=0;
    }
    else if (jumpTarget > format_ctx->duration / AV_TIME_BASE) {
        jumpTarget = format_ctx->duration / AV_TIME_BASE;
    }
    this->jumpTarget = jumpTarget;
    jumpSignal = true;
    return true;
}

bool Parser::setTempo(double tempo) {
    std::lock_guard<std::mutex> lock(tempoMutex);
    if (tempo < 0.5 || tempo > 2) {
        std::cerr << "Tempo out of range, should be in [0.5,2]. Your value is " << tempo << std::endl << std::flush;
        return false;
    }
    this->targetTempo = tempo;
    tempoSignal = true;
    return true;
}

void Parser::release() {
    // 释放回收资源
    avformat_close_input(&format_ctx);
    avfilter_graph_free(&filter_graph);
    swr_free(&swr_ctx);
    avcodec_free_context(&codec_ctx);
}