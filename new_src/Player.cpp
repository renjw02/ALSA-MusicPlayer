#include "Player.h"

#include <iostream>
#include <exception>
#include <functional>


Player::Player(int channel, unsigned int sample_rate, snd_pcm_format_t sample_format, const std::string& device_name) {
    // 分配 hw_params 空间
    int err = 0;
    if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0) {
        throw(std::runtime_error("Player error: cannot alloc hwparams, error message: " + std::string(snd_strerror(err))));
    }   
    // 打开 pcm 设备
    if ((err = snd_pcm_open(&pcm_handle, device_name.c_str(), SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        throw(std::runtime_error("Player error: cannot open pcm device, error message: " + std::string(snd_strerror(err))));
    }
    // 初始化 pcm 配置
    if ((err = snd_pcm_hw_params_any(pcm_handle, hw_params)) < 0) {
        throw(std::runtime_error("Player error: cannot init hwparams, error message: " + std::string(snd_strerror(err))));
    }
    // 设置 pcm 参数
    if ((err = snd_pcm_set_params(pcm_handle, sample_format, SND_PCM_ACCESS_RW_INTERLEAVED, channel, sample_rate, 0, 500000)) < 0) {
        throw(std::runtime_error("Player error: cannot set params, error message: " + std::string(snd_strerror(err))));
    }
    
    buffer_size = 0;
    if ((err = snd_pcm_get_params(pcm_handle, &buffer_size, &period_size)) < 0) {
        throw(std::runtime_error("AlsaPlayer error: snd_pcm_get_params, error message: " + std::string(snd_strerror(err))));
    }
}

Player::~Player() {
    snd_pcm_drain(pcm_handle);
    snd_pcm_close(pcm_handle);
    snd_pcm_hw_params_free(hw_params);
}

void Player::play_to_pcm(void *buffer, size_t out_sample_size) {
    snd_pcm_sframes_t frames = snd_pcm_writei(pcm_handle, buffer, out_sample_size);
    if (frames < 0) {
        frames = snd_pcm_recover(pcm_handle, frames, 0);
    }
    if (frames < 0) {
        throw(std::runtime_error("Player error: cannot write to pcm device, error message: " + std::string(snd_strerror(frames))));
    }

}
