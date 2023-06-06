#pragma once

#include <string>

extern "C" {
    #include <alsa/asoundlib.h>
}


class Player {
public:

    // device_name: 设备名("default")
    Player(int channel, unsigned int sample_rate, snd_pcm_format_t sample_format, const std::string& device_name = "default");

    ~Player();
    // 将 buffer 中 out_sample_size 个字节的数据写入 pcm 设备
    void play_to_pcm(void *buffer, size_t out_sample_size);
private:
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_t *pcm_handle;
    int periods = 2;
    snd_pcm_uframes_t period_size;
    snd_pcm_uframes_t buffer_size;
};