#pragma once

#include <string>

extern "C" {
    #include <alsa/asoundlib.h>
}


class Player {
public:
    // 构造函数
    // channel: 声道数
    // sample_rate: 采样率
    // sample_format: 采样格式
    // device_name: 设备名("default")
    Player(int channel, unsigned int sample_rate, snd_pcm_format_t sample_format, const std::string& device_name = "default");
    // 关闭 pcm 设备 析构函数
    ~Player();
    // 将 buffer 中 out_sample_size 个字节的数据写入 pcm 设备
    void play_to_pcm(void *buffer, size_t out_sample_size);
private:
    // 定义用于PCM流和硬件的 
    snd_pcm_hw_params_t *hw_params;
    // PCM设备的句柄  想要操作PCM设备必须定义
    snd_pcm_t *pcm_handle;
    // 周期数
    int periods = 2;
    // 一个周期的大小，这里虽然是设置的字节大小，但是在有时候需要将此大小转换为帧，所以在用的时候要换算成帧数大小才可以
    snd_pcm_uframes_t period_size;
    snd_pcm_uframes_t buffer_size;
};