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
Controller controller;

void* keyboard_thread(void* arg);

int main(int argc, char *argv[]) {
    // 创建键盘输入线程
    if (pthread_create(&input_thread, NULL, keyboard_thread, NULL) != 0) {
        printf("Failed to create keyboard thread\n");
        return 1;
    }

    controller.add_song(argv[1]);
    controller.change_song(0);
}


void* keyboard_thread(void* arg) {
    double ctime;
    double ttime;
    double tempos[4] = {0.5,1.0,1.5,2.0};
    int ctempo = 1;
    while (1) {
        // 读取用户输入的字符
        char c = getchar();
        //std::cout << "input: " << c << std::endl;
        if(c=='d'){
            controller.get_time(ctime,ttime);
            if(ctime+10>ttime){
                ctime = ttime;
            }
            else ctime += 10;
            controller.jump(ctime);
            std::cout <<  ctime << " : " << ttime << std::endl;
        }
        else if(c=='a'){
            controller.get_time(ctime,ttime);
            controller.jump(ctime-10);
            if(ctime<10){
                ctime = 0;
            }
            else ctime -= 10; 
            std::cout <<  ctime << " : " << ttime << std::endl;
        }
        else if(c=='x'){
            if(ctempo==0){
                std::cout <<  tempos[ctempo] << " 已是最低倍速 "  << std::endl;
                continue;
            }
            ctempo--;
            controller.set_tempo(tempos[ctempo]);
            std::cout << " 当前倍速 " << tempos[ctempo]  << std::endl;
        }
        else if(c=='c'){
            if(ctempo==3){
                std::cout <<  tempos[ctempo] << " 已是最高倍速 "  << std::endl;
                continue;
            }
            ctempo++;
            controller.set_tempo(tempos[ctempo]);
            std::cout << " 当前倍速 " << tempos[ctempo]  << std::endl;
        }
    }
    return NULL;
}