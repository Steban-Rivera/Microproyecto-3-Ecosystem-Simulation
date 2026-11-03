#pragma once

#include "animal.hpp"
#include <iostream>
#include <string>
#include <vector>

struct Config {
    int gen_proc_rabbits = 0;
    int gen_proc_foxes = 0;
    int gen_food_foxes = 0;
    int n_gen = 0;
    int rows = 0;
    int cols = 0;
    int n_objects = 0;
};

struct InitialObject {
    ObjectType type = ObjectType::EMPTY;
    int r = 0;
    int c = 0;

    bool operator<(const InitialObject& other) const {
        if (r != other.r) return r < other.r;
        return c < other.c;
    }
};

class InputManager {
public:
    static bool parse_input(std::istream& is, Config& config, std::vector<InitialObject>& objects, std::string& error_msg);
    static void write_output(std::ostream& os, const Config& config, const std::vector<InitialObject>& objects);
};
