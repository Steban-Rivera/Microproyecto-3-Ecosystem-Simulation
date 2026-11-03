#include "input.hpp"
#include <sstream>
#include <set>
#include <algorithm>

bool InputManager::parse_input(std::istream& is, Config& config, std::vector<InitialObject>& objects, std::string& error_msg) {
    objects.clear();

    if (!(is >> config.gen_proc_rabbits >> config.gen_proc_foxes >> config.gen_food_foxes
             >> config.n_gen >> config.rows >> config.cols >> config.n_objects)) {
        error_msg = "Error reading simulation configuration line.";
        return false;
    }

    if (config.gen_proc_rabbits <= 0) {
        error_msg = "GEN_PROC_RABBITS must be a positive integer.";
        return false;
    }
    if (config.gen_proc_foxes <= 0) {
        error_msg = "GEN_PROC_FOXES must be a positive integer.";
        return false;
    }
    if (config.gen_food_foxes <= 0) {
        error_msg = "GEN_FOOD_FOXES must be a positive integer.";
        return false;
    }
    if (config.n_gen < 0) {
        error_msg = "N_GEN cannot be negative.";
        return false;
    }
    if (config.rows <= 0 || config.cols <= 0) {
        error_msg = "Grid dimensions R and C must be positive integers.";
        return false;
    }
    if (config.n_objects < 0) {
        error_msg = "N objects cannot be negative.";
        return false;
    }
    if (config.n_objects > config.rows * config.cols) {
        error_msg = "N objects exceeds total grid cell capacity.";
        return false;
    }

    std::set<std::pair<int, int>> occupied_coords;
    objects.reserve(config.n_objects);

    for (int i = 0; i < config.n_objects; ++i) {
        std::string type_str;
        int r = 0, c = 0;
        if (!(is >> type_str >> r >> c)) {
            error_msg = "Error reading object at line " + std::to_string(i + 2) + ". Expected format: OBJECT X Y";
            return false;
        }

        ObjectType type = string_to_object_type(type_str);
        if (type == ObjectType::EMPTY) {
            error_msg = "Unknown object type '" + type_str + "' at line " + std::to_string(i + 2) + ". Must be ROCK, RABBIT, or FOX.";
            return false;
        }

        if (r < 0 || r >= config.rows || c < 0 || c >= config.cols) {
            error_msg = "Object coordinates (" + std::to_string(r) + ", " + std::to_string(c) + ") out of bounds for grid " +
                        std::to_string(config.rows) + "x" + std::to_string(config.cols) + ".";
            return false;
        }

        if (occupied_coords.count({r, c})) {
            error_msg = "Duplicate object at coordinate (" + std::to_string(r) + ", " + std::to_string(c) + "). Each cell can only hold one object.";
            return false;
        }
        occupied_coords.insert({r, c});

        objects.push_back({type, r, c});
    }

    return true;
}

void InputManager::write_output(std::ostream& os, const Config& config, const std::vector<InitialObject>& objects) {
    // Expected output format:
    // GEN_PROC_RABBITS GEN_PROC_FOXES GEN_FOOD_FOXES 0 R C N_FINAL
    // followed by N_FINAL lines with OBJECT X Y sorted by (X, Y)
    std::vector<InitialObject> sorted_objs = objects;
    std::sort(sorted_objs.begin(), sorted_objs.end());

    os << config.gen_proc_rabbits << " "
       << config.gen_proc_foxes << " "
       << config.gen_food_foxes << " "
       << 0 << " "
       << config.rows << " "
       << config.cols << " "
       << sorted_objs.size() << "\n";

    for (const auto& obj : sorted_objs) {
        os << object_type_to_string(obj.type) << " " << obj.r << " " << obj.c << "\n";
    }
}
