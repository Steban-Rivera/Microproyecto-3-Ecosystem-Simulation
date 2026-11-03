#include "statistics.hpp"
#include <fstream>
#include <iomanip>

void SimulationStatistics::record_generation(const GenerationStats& stats) {
    history.push_back(stats);
}

void SimulationStatistics::print_summary(std::ostream& os) const {
    os << "\n================ SIMULATION STATISTICS ================\n";
    os << "Mode: " << mode << "\n";
    os << "Threads: " << threads_used << "\n";
    os << "Generations Simulated: " << history.size() << "\n";
    os << "Total Execution Time: " << std::fixed << std::setprecision(4) << total_time_ms << " ms\n";

    if (!history.empty()) {
        const auto& init = history.front();
        const auto& last = history.back();
        os << "Initial Populations -> Rabbits: " << init.rabbit_count
           << ", Foxes: " << init.fox_count
           << ", Rocks: " << init.rock_count << "\n";
        os << "Final Populations   -> Rabbits: " << last.rabbit_count
           << ", Foxes: " << last.fox_count
           << ", Rocks: " << last.rock_count << "\n";

        long long total_rabbit_births = 0;
        long long total_fox_births = 0;
        long long total_starvations = 0;
        long long total_eaten = 0;

        for (const auto& g : history) {
            total_rabbit_births += g.rabbit_births;
            total_fox_births += g.fox_births;
            total_starvations += g.fox_starvations;
            total_eaten += g.rabbits_eaten;
        }

        os << "Total Rabbit Births: " << total_rabbit_births << "\n";
        os << "Total Fox Births:    " << total_fox_births << "\n";
        os << "Total Eaten Rabbits: " << total_eaten << "\n";
        os << "Total Fox Starvations: " << total_starvations << "\n";
    }
    os << "=======================================================\n";
}

std::string SimulationStatistics::to_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"mode\": \"" << mode << "\",\n";
    ss << "  \"threads\": " << threads_used << ",\n";
    ss << "  \"total_time_ms\": " << total_time_ms << ",\n";
    ss << "  \"history\": [\n";

    for (size_t i = 0; i < history.size(); ++i) {
        const auto& h = history[i];
        ss << "    {\n";
        ss << "      \"generation\": " << h.generation << ",\n";
        ss << "      \"rabbit_count\": " << h.rabbit_count << ",\n";
        ss << "      \"fox_count\": " << h.fox_count << ",\n";
        ss << "      \"rock_count\": " << h.rock_count << ",\n";
        ss << "      \"rabbit_births\": " << h.rabbit_births << ",\n";
        ss << "      \"fox_births\": " << h.fox_births << ",\n";
        ss << "      \"fox_starvations\": " << h.fox_starvations << ",\n";
        ss << "      \"rabbits_eaten\": " << h.rabbits_eaten << ",\n";
        ss << "      \"rabbit_conflicts_lost\": " << h.rabbit_conflicts_lost << ",\n";
        ss << "      \"fox_conflicts_lost\": " << h.fox_conflicts_lost << ",\n";
        ss << "      \"step_time_ms\": " << h.step_time_ms << "\n";
        ss << "    }" << (i + 1 < history.size() ? "," : "") << "\n";
    }

    ss << "  ]\n";
    ss << "}\n";
    return ss.str();
}

void SimulationStatistics::export_json_file(const std::string& filename) const {
    std::ofstream out(filename);
    if (out.is_open()) {
        out << to_json();
    }
}
