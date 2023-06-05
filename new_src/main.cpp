#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

extern "C" {
    #include <stdio.h>
    #include <stdlib.h>
    #include <unistd.h>
    #include <stdint.h>
    #include <string.h>
    #include <stdbool.h>
    #include <sys/stat.h>
    #include <alsa/asoundlib.h>
    #include <pthread.h>
}

#include "Player.h"
#include "Parser.h"
#include "Controller.h"

// 全局变量
pthread_t input_thread;  // 键盘输入线程
int playback_status = 1;  // 播放状态，0表示未播放，1表示正在播放
double playback_rate = 1.0;  // 播放速度
double current_rate = 1.0;
int next = 0;   // 0切换下一首，1切换上一首
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void* keyboard_thread(void* arg)

int main(int argc, char *argv[]) {
    // 创建键盘输入线程
    if (pthread_create(&input_thread, NULL, keyboard_thread, NULL) != 0) {
        printf("Failed to create keyboard thread\n");
        return 1;
    }

    Controller controller;
    controller.add_song(argv[1]);
    controller.change_song(0);
}


void* keyboard_thread(void* arg) {
    while (1) {
        // 读取用户输入的字符
        char c = getchar();
        std::cout << "input: " << c << std::endl;
    }
    return NULL;
}