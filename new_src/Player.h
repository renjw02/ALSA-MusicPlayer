#pragma once

#include <string>

extern "C" {
    #include <alsa/asoundlib.h>
}


class Player {
private:
    snd_pcm_hw_params_t *hw_params;
    snd_pcm_t *pcm_handle;
    int periods = 2;
    snd_pcm_uframes_t period_size;
    snd_pcm_uframes_t buffer_size;
    
public:
    Player(int channel, unsigned int sample_rate, snd_pcm_format_t sample_format, const std::string& device_name = "default");
    ~Player();

    void play_to_pcm(void *buffer, size_t out_sample_size);
};