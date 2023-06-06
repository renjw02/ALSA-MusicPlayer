#pragma once

#include <vector>
#include <string>
#include <thread>

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
    
    inline int get_current_select_index() const { 
        return current_index; 
    }
    inline void set_current_select_index(int index) { 
        current_index = index; 
    }
    // inline std::string get_current_select_song_name() const { 
    //     return current_select_index == -1 ?  "No Song" : song_list[current_select_index]; 
    // }
    inline void delete_list_index(int index) {
        if(index >= current_index) {
            current_index -= 1;
        }
        song_list.erase(song_list.begin() + index);
    }

    inline void add_song(std::string song_name) {
        song_list.push_back(song_name);
    }

    //void pause();
    void play();
    void set_tempo(double tempo);
    void jump(double jumpTarget);
    void get_time(double &current_time, double &total_time);
    void change_song(int index);
    
    void play_worker();
};