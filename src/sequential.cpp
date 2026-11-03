#include "sequential.hpp"
#include <chrono>
#include <algorithm>

SequentialEcosystem::SequentialEcosystem() {
    stats_.mode = "Sequential";
    stats_.threads_used = 1;
}

void SequentialEcosystem::step() {
    auto t_start = std::chrono::high_resolution_clock::now();

    int g = current_gen_;
    GenerationStats gen_stat;
    gen_stat.generation = g + 1;
    gen_stat.rock_count = static_cast<int>(rocks_.size());

    int total_cells = config_.rows * config_.cols;
    int n_rabbits = static_cast<int>(rabbits_.size());

    // Proposal structure for rabbits
    struct RabbitProposal {
        int rabbit_idx;
        bool moved;
        int old_r, old_c;
        int target_r, target_c;
    };

    std::vector<std::vector<RabbitProposal>> rabbit_proposals(total_cells);
    std::vector<RabbitProposal> rab_props(n_rabbits);

    // ==========================================
    // PHASE 1: RABBIT MOVEMENT PROPOSALS
    // ==========================================
    for (int i = 0; i < n_rabbits; ++i) {
        auto& b = rabbits_[i];
        if (!b.alive) continue;

        std::pair<int, int> possible_empty[4];
        int p = 0;

        for (const auto& dir : CLOCKWISE_DIRS) {
            int nr = b.r + dir.dr;
            int nc = b.c + dir.dc;
            if (in_bounds(nr, nc)) {
                int n_idx = cell_index(nr, nc);
                if (grid_[n_idx] == ObjectType::EMPTY) {
                    possible_empty[p++] = {nr, nc};
                }
            }
        }

        int target_r = b.r;
        int target_c = b.c;
        bool moved = false;

        if (p > 0) {
            int chosen_idx = select_adjacent(g, b.r, b.c, p);
            target_r = possible_empty[chosen_idx].first;
            target_c = possible_empty[chosen_idx].second;
            moved = true;
        }

        rab_props[i] = {i, moved, b.r, b.c, target_r, target_c};
        int t_idx = cell_index(target_r, target_c);
        rabbit_proposals[t_idx].push_back(rab_props[i]);
    }

    // ==========================================
    // PHASE 2: RABBIT CONFLICT RESOLUTION & REPRODUCTION
    // ==========================================
    std::vector<bool> rabbit_won(n_rabbits, false);
    std::vector<bool> rabbit_procreated(n_rabbits, false);

    for (int idx = 0; idx < total_cells; ++idx) {
        auto& group = rabbit_proposals[idx];
        if (group.empty()) continue;

        std::sort(group.begin(), group.end(), [this](const RabbitProposal& a, const RabbitProposal& b) {
            const auto& ra = rabbits_[a.rabbit_idx];
            const auto& rb = rabbits_[b.rabbit_idx];
            if (ra.proc_age != rb.proc_age) return ra.proc_age > rb.proc_age;
            return ra.id < rb.id;
        });

        // Winner survives
        int winner_idx = group[0].rabbit_idx;
        rabbit_won[winner_idx] = true;

        for (size_t k = 1; k < group.size(); ++k) {
            rabbits_[group[k].rabbit_idx].alive = false;
            gen_stat.rabbit_conflicts_lost++;
        }
    }

    // Process winners in parent index order (0 to n_rabbits - 1)
    std::vector<Animal> baby_rabbits;
    for (int i = 0; i < n_rabbits; ++i) {
        if (!rabbits_[i].alive || !rabbit_won[i]) continue;

        auto& b = rabbits_[i];
        const auto& prop = rab_props[i];
        b.r = prop.target_r;
        b.c = prop.target_c;

        if (prop.moved && b.proc_age >= config_.gen_proc_rabbits) {
            b.proc_age = 0;
            baby_rabbits.emplace_back(next_animal_id_++, ObjectType::RABBIT, prop.old_r, prop.old_c, 0, 0);
            gen_stat.rabbit_births++;
        } else {
            b.proc_age++;
        }
    }

    // Update grid with new rabbit positions
    for (int idx = 0; idx < total_cells; ++idx) {
        if (grid_[idx] == ObjectType::RABBIT) {
            grid_[idx] = ObjectType::EMPTY;
        }
    }
    for (const auto& b : rabbits_) {
        if (b.alive) {
            grid_[cell_index(b.r, b.c)] = ObjectType::RABBIT;
        }
    }
    for (const auto& baby : baby_rabbits) {
        grid_[cell_index(baby.r, baby.c)] = ObjectType::RABBIT;
    }

    // ==========================================
    // PHASE 3: FOX MOVEMENT PROPOSALS & STARVATION
    // ==========================================
    int n_foxes = static_cast<int>(foxes_.size());
    struct FoxProposal {
        int fox_idx;
        bool moved;
        bool ate;
        bool starved;
        int old_r, old_c;
        int target_r, target_c;
    };

    std::vector<std::vector<FoxProposal>> fox_proposals(total_cells);
    std::vector<FoxProposal> fx_props(n_foxes);

    for (int j = 0; j < n_foxes; ++j) {
        auto& f = foxes_[j];
        if (!f.alive) continue;

        std::pair<int, int> possible_rabbits[4];
        int p_rab = 0;

        for (const auto& dir : CLOCKWISE_DIRS) {
            int nr = f.r + dir.dr;
            int nc = f.c + dir.dc;
            if (in_bounds(nr, nc)) {
                int n_idx = cell_index(nr, nc);
                if (grid_[n_idx] == ObjectType::RABBIT) {
                    possible_rabbits[p_rab++] = {nr, nc};
                }
            }
        }

        if (p_rab > 0) {
            int chosen_idx = select_adjacent(g, f.r, f.c, p_rab);
            int target_r = possible_rabbits[chosen_idx].first;
            int target_c = possible_rabbits[chosen_idx].second;

            fx_props[j] = {j, true, true, false, f.r, f.c, target_r, target_c};
            int t_idx = cell_index(target_r, target_c);
            fox_proposals[t_idx].push_back(fx_props[j]);
        } else {
            if (f.food_age + 1 >= config_.gen_food_foxes) {
                f.alive = false;
                gen_stat.fox_starvations++;
                fx_props[j] = {j, false, false, true, 0, 0, 0, 0};
                continue;
            }

            std::pair<int, int> possible_empty[4];
            int p_emp = 0;

            for (const auto& dir : CLOCKWISE_DIRS) {
                int nr = f.r + dir.dr;
                int nc = f.c + dir.dc;
                if (in_bounds(nr, nc)) {
                    int n_idx = cell_index(nr, nc);
                    if (grid_[n_idx] == ObjectType::EMPTY) {
                        possible_empty[p_emp++] = {nr, nc};
                    }
                }
            }

            int target_r = f.r;
            int target_c = f.c;
            bool moved = false;

            if (p_emp > 0) {
                int chosen_idx = select_adjacent(g, f.r, f.c, p_emp);
                target_r = possible_empty[chosen_idx].first;
                target_c = possible_empty[chosen_idx].second;
                moved = true;
            }

            fx_props[j] = {j, moved, false, false, f.r, f.c, target_r, target_c};
            int t_idx = cell_index(target_r, target_c);
            fox_proposals[t_idx].push_back(fx_props[j]);
        }
    }

    // ==========================================
    // PHASE 4: FOX CONFLICT RESOLUTION, FEEDING & REPRODUCTION
    // ==========================================
    std::vector<bool> fox_won(n_foxes, false);
    std::vector<bool> eaten_cells(total_cells, false);

    for (int idx = 0; idx < total_cells; ++idx) {
        auto& group = fox_proposals[idx];
        if (group.empty()) continue;

        std::sort(group.begin(), group.end(), [this](const FoxProposal& a, const FoxProposal& b) {
            const auto& fa = foxes_[a.fox_idx];
            const auto& fb = foxes_[b.fox_idx];
            if (fa.proc_age != fb.proc_age) return fa.proc_age > fb.proc_age;
            if (fa.food_age != fb.food_age) return fa.food_age < fb.food_age;
            return fa.id < fb.id;
        });

        int winner_idx = group[0].fox_idx;
        fox_won[winner_idx] = true;

        for (size_t k = 1; k < group.size(); ++k) {
            foxes_[group[k].fox_idx].alive = false;
            gen_stat.fox_conflicts_lost++;
        }
    }

    // Process fox winners in parent index order (0 to n_foxes - 1)
    std::vector<Animal> baby_foxes;
    for (int j = 0; j < n_foxes; ++j) {
        if (!foxes_[j].alive || !fox_won[j]) continue;

        auto& f = foxes_[j];
        const auto& prop = fx_props[j];
        f.r = prop.target_r;
        f.c = prop.target_c;

        if (prop.ate) {
            f.food_age = 0;
            eaten_cells[cell_index(f.r, f.c)] = true;
            gen_stat.rabbits_eaten++;
        } else {
            f.food_age++;
        }

        if (prop.moved && f.proc_age >= config_.gen_proc_foxes) {
            f.proc_age = 0;
            baby_foxes.emplace_back(next_animal_id_++, ObjectType::FOX, prop.old_r, prop.old_c, 0, 0);
            gen_stat.fox_births++;
        } else {
            f.proc_age++;
        }
    }

    // ==========================================
    // PHASE 5: CLEANUP & COMPACTION
    // ==========================================
    for (auto& b : rabbits_) {
        if (b.alive && eaten_cells[cell_index(b.r, b.c)]) {
            b.alive = false;
        }
    }
    for (auto& baby : baby_rabbits) {
        if (baby.alive && eaten_cells[cell_index(baby.r, baby.c)]) {
            baby.alive = false;
        }
    }

    std::vector<Animal> surviving_rabbits;
    surviving_rabbits.reserve(rabbits_.size() + baby_rabbits.size());
    for (const auto& b : rabbits_) {
        if (b.alive) surviving_rabbits.push_back(b);
    }
    for (const auto& baby : baby_rabbits) {
        if (baby.alive) surviving_rabbits.push_back(baby);
    }
    rabbits_ = std::move(surviving_rabbits);

    std::vector<Animal> surviving_foxes;
    surviving_foxes.reserve(foxes_.size() + baby_foxes.size());
    for (const auto& f : foxes_) {
        if (f.alive) surviving_foxes.push_back(f);
    }
    for (const auto& baby : baby_foxes) {
        if (baby.alive) surviving_foxes.push_back(baby);
    }
    foxes_ = std::move(surviving_foxes);

    // Rebuild grid
    std::fill(grid_.begin(), grid_.end(), ObjectType::EMPTY);
    for (const auto& rock : rocks_) {
        grid_[cell_index(rock.first, rock.second)] = ObjectType::ROCK;
    }
    for (const auto& b : rabbits_) {
        grid_[cell_index(b.r, b.c)] = ObjectType::RABBIT;
    }
    for (const auto& f : foxes_) {
        grid_[cell_index(f.r, f.c)] = ObjectType::FOX;
    }

    current_gen_++;

    auto t_end = std::chrono::high_resolution_clock::now();
    double step_time = std::chrono::duration<double, std::milli>(t_end - t_start).count();
    gen_stat.rabbit_count = static_cast<int>(rabbits_.size());
    gen_stat.fox_count = static_cast<int>(foxes_.size());
    gen_stat.step_time_ms = step_time;
    stats_.record_generation(gen_stat);
    stats_.total_time_ms += step_time;
}
