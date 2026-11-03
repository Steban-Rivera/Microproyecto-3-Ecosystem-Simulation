#include "ecosystem.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>

void Ecosystem::initialize(const Config& cfg, const std::vector<InitialObject>& objects) {
    config_ = cfg;
    current_gen_ = 0;
    next_animal_id_ = 1;

    grid_.assign(config_.rows * config_.cols, ObjectType::EMPTY);
    rabbits_.clear();
    foxes_.clear();
    rocks_.clear();
    stats_ = SimulationStatistics();

    for (const auto& obj : objects) {
        int idx = cell_index(obj.r, obj.c);
        grid_[idx] = obj.type;

        if (obj.type == ObjectType::ROCK) {
            rocks_.push_back({obj.r, obj.c});
        } else if (obj.type == ObjectType::RABBIT) {
            rabbits_.emplace_back(next_animal_id_++, ObjectType::RABBIT, obj.r, obj.c, 0, 0);
        } else if (obj.type == ObjectType::FOX) {
            foxes_.emplace_back(next_animal_id_++, ObjectType::FOX, obj.r, obj.c, 0, 0);
        }
    }

    // Record initial state at gen 0
    GenerationStats g0;
    g0.generation = 0;
    g0.rabbit_count = static_cast<int>(rabbits_.size());
    g0.fox_count = static_cast<int>(foxes_.size());
    g0.rock_count = static_cast<int>(rocks_.size());
    stats_.record_generation(g0);
}

void Ecosystem::run() {
    for (int g = 0; g < config_.n_gen; ++g) {
        step();
    }
}

std::vector<InitialObject> Ecosystem::get_current_objects() const {
    std::vector<InitialObject> objects;
    objects.reserve(rocks_.size() + rabbits_.size() + foxes_.size());

    for (const auto& rock : rocks_) {
        objects.push_back({ObjectType::ROCK, rock.first, rock.second});
    }
    for (const auto& b : rabbits_) {
        if (b.alive) {
            objects.push_back({ObjectType::RABBIT, b.r, b.c});
        }
    }
    for (const auto& f : foxes_) {
        if (f.alive) {
            objects.push_back({ObjectType::FOX, f.r, f.c});
        }
    }

    std::sort(objects.begin(), objects.end());
    return objects;
}

void Ecosystem::render_console(std::ostream& os, bool use_color) const {
    std::vector<std::string> display_grid(config_.rows, std::string(config_.cols, ' '));

    for (const auto& rock : rocks_) {
        display_grid[rock.first][rock.second] = '*';
    }
    for (const auto& b : rabbits_) {
        if (b.alive) display_grid[b.r][b.c] = 'R';
    }
    for (const auto& f : foxes_) {
        if (f.alive) display_grid[f.r][f.c] = 'F';
    }

    // ANSI colors
    const std::string RESET = use_color ? "\033[0m" : "";
    const std::string BOLD = use_color ? "\033[1m" : "";
    const std::string GREEN = use_color ? "\033[32m" : "";
    const std::string RED = use_color ? "\033[31m" : "";
    const std::string YELLOW = use_color ? "\033[33m" : "";
    const std::string CYAN = use_color ? "\033[36m" : "";

    os << BOLD << CYAN << "--- Generation " << current_gen_ << " of " << config_.n_gen
       << " | Rabbits: " << GREEN << rabbits_.size()
       << CYAN << " | Foxes: " << RED << foxes_.size()
       << CYAN << " | Rocks: " << YELLOW << rocks_.size()
       << RESET << " ---\n";

    std::string h_border = "+" + std::string(config_.cols * 2 + 1, '-') + "+";
    os << h_border << "\n";

    for (int r = 0; r < config_.rows; ++r) {
        os << "| ";
        for (int c = 0; c < config_.cols; ++c) {
            char ch = display_grid[r][c];
            if (ch == '*') {
                os << YELLOW << "*" << RESET << " ";
            } else if (ch == 'R') {
                os << GREEN << "R" << RESET << " ";
            } else if (ch == 'F') {
                os << RED << "F" << RESET << " ";
            } else {
                os << ". ";
            }
        }
        os << "|\n";
    }
    os << h_border << "\n";
}

std::string Ecosystem::export_state_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"generation\": " << current_gen_ << ",\n";
    ss << "  \"rows\": " << config_.rows << ",\n";
    ss << "  \"cols\": " << config_.cols << ",\n";
    ss << "  \"rabbits\": [\n";
    for (size_t i = 0; i < rabbits_.size(); ++i) {
        const auto& b = rabbits_[i];
        ss << "    {\"id\": " << b.id << ", \"r\": " << b.r << ", \"c\": " << b.c
           << ", \"proc_age\": " << b.proc_age << "}" << (i + 1 < rabbits_.size() ? "," : "") << "\n";
    }
    ss << "  ],\n";
    ss << "  \"foxes\": [\n";
    for (size_t i = 0; i < foxes_.size(); ++i) {
        const auto& f = foxes_[i];
        ss << "    {\"id\": " << f.id << ", \"r\": " << f.r << ", \"c\": " << f.c
           << ", \"proc_age\": " << f.proc_age << ", \"food_age\": " << f.food_age << "}"
           << (i + 1 < foxes_.size() ? "," : "") << "\n";
    }
    ss << "  ],\n";
    ss << "  \"rocks\": [\n";
    for (size_t i = 0; i < rocks_.size(); ++i) {
        ss << "    {\"r\": " << rocks_[i].first << ", \"c\": " << rocks_[i].second << "}"
           << (i + 1 < rocks_.size() ? "," : "") << "\n";
    }
    ss << "  ]\n";
    ss << "}\n";
    return ss.str();
}
