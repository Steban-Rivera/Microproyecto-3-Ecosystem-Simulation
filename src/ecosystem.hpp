#pragma once

#include "animal.hpp"
#include "input.hpp"
#include "statistics.hpp"
#include <vector>
#include <string>
#include <iostream>

class Ecosystem {
protected:
    Config config_;
    int current_gen_ = 0;
    int next_animal_id_ = 1;

    std::vector<ObjectType> grid_; // size rows * cols
    std::vector<Animal> rabbits_;
    std::vector<Animal> foxes_;
    std::vector<std::pair<int, int>> rocks_;

    SimulationStatistics stats_;

    inline int cell_index(int r, int c) const {
        return r * config_.cols + c;
    }

    inline bool in_bounds(int r, int c) const {
        return r >= 0 && r < config_.rows && c >= 0 && c < config_.cols;
    }

public:
    virtual ~Ecosystem() = default;

    void initialize(const Config& cfg, const std::vector<InitialObject>& objects);

    virtual void step() = 0;
    void run();

    int get_current_gen() const { return current_gen_; }
    const Config& get_config() const { return config_; }
    const std::vector<Animal>& get_rabbits() const { return rabbits_; }
    const std::vector<Animal>& get_foxes() const { return foxes_; }
    const std::vector<std::pair<int, int>>& get_rocks() const { return rocks_; }
    const SimulationStatistics& get_statistics() const { return stats_; }

    std::vector<InitialObject> get_current_objects() const;
    void render_console(std::ostream& os, bool use_color = true) const;
    std::string export_state_json() const;
};
