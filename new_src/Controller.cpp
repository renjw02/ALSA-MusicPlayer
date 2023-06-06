#include "Controller.h"

Controller::Controller() {
    current_index = -1;
    parser = new Parser();
    player = new Player(2, 44100, SND_PCM_FORMAT_S16_LE);

    play_thread = std::thread(&Controller::play_worker, this);
}

void Controller::play_worker() {
    std::cout << "handler thread start:" << std::endl;
    parser->parse([&](void *buffer, int outSamplesize) {
        player->play_to_pcm(buffer, outSamplesize);
    });
}

void Controller::create_song_list() {
    std::ifstream playlist("playList.txt");
    if (!playlist.is_open()) {
        std::cout << "Cannot open playList.txt" << std::endl;
        exit(1);
    }

    std::string song_name;
    while (std::getline(playlist, song_name)) {
        // 移除换行符
        song_name.erase(std::remove(song_name.begin(), song_name.end(), '\r'), song_name.end());
        song_name.erase(std::remove(song_name.begin(), song_name.end(), '\n'), song_name.end());
        song_list.push_back(song_name);
    }
    // printf("total song number:%d\n", songCount);
    playlist.close();
}

void Controller::print_song_list() {
    std::cout << "********************************" << std::endl;
    std::cout << "song list: " << std::endl;
    for (int i = 0; i < song_list.size(); i++) {
        std::cout << i << ": " << song_list[i] << std::endl;
    }
    std::cout << "********************************" << std::endl;
}

void Controller::change_song(int index) {
    int count =  song_list.size();
    if (index >= count) {
        index = 0;
    } else if (index < 0) {
        index = song_list.size() - 1;
    }

    current_index = index;
    parser->pause();
    parser->openFile(song_list[index].c_str());
    parser->play();
}

int Controller::get_index_of_song(const std::string& song_name) {
    auto iter = std::find(song_list.begin(), song_list.end(), song_name);
    if (iter != song_list.end()) {
        std::vector<std::string>::difference_type index = std::distance(song_list.begin(), iter);
        int intIndex = static_cast<int>(index);
        return intIndex;

    } else {
        std::cout << "Song '" << song_name << "' not found in the list." << std::endl;
        return -1;
    }
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