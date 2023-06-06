#include "Controller.h"

Controller::Controller() {
    current_index = -1;
    parser = new Parser();
    player = new Player(2, 44100, SND_PCM_FORMAT_S16_LE);

    // FILE *playlist = fopen("playList.txt", "r");
    // if (playlist == NULL) {
    //     printf("无法打开歌曲列表文件\n");
    //     return 1;
    // }
    // char song_name[256];
    // while (fgets(song_name, sizeof(song_name), playlist) != NULL) {
    //     song_name[strcspn(song_name, "\r")] = '\0';
    //     song_name[strcspn(song_name, "\n")] = '\0';  // 移除换行符
    //     strcpy(songList[songCount], song_name);  // 存储歌曲名字到歌曲列表
    //     songCount++;
    // }
    // printf("total song number:%d\n", songCount);
    // fclose(playlist);

    play_thread = std::thread(&Controller::play_worker, this);
}

void Controller::play_worker() {
    std::cout << "handler thread start:" << std::endl;
    parser->parse([&](void *buffer, int outSamplesize) {
        player->play_to_pcm(buffer, outSamplesize);
    });
}


void Controller::change_song(int index) {
    if(index == -1) {
        current_index = -1;
        return;
    }
    // Index out of range
    if(index < 0 || index >= song_list.size()) {
        std::cerr << "index out of range" << std::endl;
        return;
    }
    current_index = index;
    parser->pause();
    parser->openFile(song_list[index].c_str());
    parser->play();
}

void Controller::pause() {
    parser->pause();
}

void Controller::set_tempo(double tempo) {
    parser->setTempo(tempo);
}

void Controller::jump(double jumpTarget) {
    parser->jump(jumpTarget);
}

void Controller::play() {
    if(current_index == -1) {
        std::cerr << "No song selected" << std::endl;
        return;
    }
    parser->play();
}

void Controller::get_time(double &current_time, double &total_time) {
    current_time = parser->getCurrentTime();
    total_time = parser->getTotalTime();
}

Controller::~Controller() {
    play_thread.join();
    delete parser;
    delete player;
}