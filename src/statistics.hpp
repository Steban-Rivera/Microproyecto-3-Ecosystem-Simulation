#pragma once

#include <vector>
#include <string>
#include <iostream>

struct GenerationStats {
    int generation = 0;
    int rabbit_count = 0;
    int fox_count = 0;
    int rock_count = 0;
    int rabbit_births = 0;
    int fox_births = 0;
    int fox_starvations = 0;
    int rabbits_eaten = 0;
    int rabbit_conflicts_lost = 0;
    int fox_conflicts_lost = 0;
    double step_time_ms = 0.0;
};

class SimulationStatistics {
public:
    std::string mode = "Sequential";
    int threads_used = 1;
    double total_time_ms = 0.0;
    std::vector<GenerationStats> history;

    void record_generation(const GenerationStats& stats);
    void print_summary(std::ostream& os) const;
    std::string to_json() const;
    void export_json_file(const std::string& filename) const;
};
