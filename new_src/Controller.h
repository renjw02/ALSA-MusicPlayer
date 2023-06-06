#pragma once

#include <vector>
#include <string>
#include <thread>
#include <iostream>
#include <fstream>
#include <algorithm>

#include "Parser.h"
#include "Player.h"

class Controller {

private:
    std::vector<std::string> song_list;
    int current_index;

    Parser *parser;
    Player *player;

    std::thread play_thread;
    
public:

    Controller();

    ~Controller();
    
    inline int get_current_index() const { 
        return current_index; 
    }

    int get_index_of_song(const std::string& song_name);

    void create_song_list();
    void print_song_list();
    void pause();
    void play();
    void set_tempo(double tempo);
    void jump(double jumpTarget);
    void get_time(double &current_time, double &total_time);
    void change_song(int index);
    
    void play_worker();
};