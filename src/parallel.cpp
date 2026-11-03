#include "parallel.hpp"
#include <chrono>
#include <atomic>
#include <algorithm>

#ifdef _OPENMP
#include <omp.h>
#endif

ParallelEcosystem::ParallelEcosystem(int num_threads)
    : num_threads_(num_threads) {
    stats_.mode = "Parallel (OpenMP)";
    int actual_threads = 1;
#ifdef _OPENMP
    if (num_threads_ > 0) {
        actual_threads = num_threads_;
    } else {
        actual_threads = omp_get_max_threads();
    }
#endif
    stats_.threads_used = actual_threads;
}

void ParallelEcosystem::set_num_threads(int num_threads) {
    num_threads_ = num_threads;
    stats_.threads_used = (num_threads_ > 0) ? num_threads_ : 1;
#ifdef _OPENMP
    if (num_threads_ <= 0) {
        stats_.threads_used = omp_get_max_threads();
    }
#endif
}

// Key packing for Rabbits:
// Higher proc_age is better.
// In case of tie, lower id is better.
inline uint64_t make_rabbit_key(int proc_age, int id) {
    uint64_t p = static_cast<uint64_t>(proc_age) & 0xFFFFFFFFULL;
    uint64_t inv_id = 0xFFFFFFFFULL - static_cast<uint64_t>(id);
    return (p << 32) | inv_id;
}

// Key packing for Foxes:
// 1) Higher proc_age is better (16 bits)
// 2) Lower food_age is better (16 bits)
// 3) Lower id is better (32 bits)
inline uint64_t make_fox_key(int proc_age, int food_age, int id) {
    uint64_t p = static_cast<uint64_t>(proc_age) & 0xFFFFULL;
    uint64_t inv_food = 0xFFFFULL - (static_cast<uint64_t>(food_age) & 0xFFFFULL);
    uint64_t inv_id = 0xFFFFFFFFULL - static_cast<uint64_t>(id);
    return (p << 48) | (inv_food << 32) | inv_id;
}

void ParallelEcosystem::step() {
    auto t_start = std::chrono::high_resolution_clock::now();

    int g = current_gen_;
    GenerationStats gen_stat;
    gen_stat.generation = g + 1;
    gen_stat.rock_count = static_cast<int>(rocks_.size());

    int n_threads = stats_.threads_used;
#ifdef _OPENMP
    if (num_threads_ > 0) {
        omp_set_num_threads(num_threads_);
        n_threads = num_threads_;
    } else {
        n_threads = omp_get_max_threads();
    }
#endif
    stats_.threads_used = n_threads;

    int total_cells = config_.rows * config_.cols;
    int n_rabbits = static_cast<int>(rabbits_.size());

    // Proposal storage for rabbits
    struct RabbitMoveProposal {
        int target_r;
        int target_c;
        int old_r;
        int old_c;
        bool moved;
        uint64_t key;
    };
    std::vector<RabbitMoveProposal> rabbit_proposals(n_rabbits);

    // Target cells atomic maximum for rabbits
    std::vector<std::atomic<uint64_t>> rabbit_target_best(total_cells);
    for (int i = 0; i < total_cells; ++i) {
        rabbit_target_best[i].store(0, std::memory_order_relaxed);
    }

    // ==========================================
    // PHASE 1: PARALLEL RABBIT MOVEMENT PROPOSALS
    // ==========================================
#ifdef _OPENMP
    #pragma omp parallel for num_threads(n_threads) schedule(static)
#endif
    for (int i = 0; i < n_rabbits; ++i) {
        auto& b = rabbits_[i];
        if (!b.alive) {
            rabbit_proposals[i] = {0, 0, 0, 0, false, 0};
            continue;
        }

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

        uint64_t key = make_rabbit_key(b.proc_age, b.id);
        rabbit_proposals[i] = {target_r, target_c, b.r, b.c, moved, key};

        int t_idx = cell_index(target_r, target_c);
        // Lock-free atomic maximum CAS
        uint64_t current = rabbit_target_best[t_idx].load(std::memory_order_relaxed);
        while (key > current && !rabbit_target_best[t_idx].compare_exchange_weak(
                   current, key, std::memory_order_release, std::memory_order_relaxed)) {
            // retry until either current is larger or CAS succeeds
        }
    }

    // ==========================================
    // PHASE 2: PARALLEL RABBIT CONFLICT RESOLUTION & REPRODUCTION
    // ==========================================
    std::vector<std::vector<Animal>> thread_baby_rabbits(n_threads);
    std::vector<int> thread_rabbit_births(n_threads, 0);
    std::vector<int> thread_rabbit_conflicts(n_threads, 0);

#ifdef _OPENMP
    #pragma omp parallel num_threads(n_threads)
#endif
    {
        int tid = 0;
#ifdef _OPENMP
        tid = omp_get_thread_num();
#endif
#ifdef _OPENMP
        #pragma omp for schedule(static)
#endif
        for (int i = 0; i < n_rabbits; ++i) {
            auto& b = rabbits_[i];
            if (!b.alive) continue;

            const auto& prop = rabbit_proposals[i];
            int t_idx = cell_index(prop.target_r, prop.target_c);
            uint64_t best_key = rabbit_target_best[t_idx].load(std::memory_order_acquire);

            if (prop.key == best_key) {
                // Winner survives!
                b.r = prop.target_r;
                b.c = prop.target_c;

                if (prop.moved && b.proc_age >= config_.gen_proc_rabbits) {
                    b.proc_age = 0;
                    // Temporary ID will be assigned sequentially for strict determinism
                    thread_baby_rabbits[tid].emplace_back(0, ObjectType::RABBIT, prop.old_r, prop.old_c, 0, 0);
                    thread_rabbit_births[tid]++;
                } else {
                    b.proc_age++;
                }
            } else {
                // Conflict lost
                b.alive = false;
                thread_rabbit_conflicts[tid]++;
            }
        }
    }

    // Merge rabbit statistics and baby rabbits deterministically
    for (int tid = 0; tid < n_threads; ++tid) {
        gen_stat.rabbit_births += thread_rabbit_births[tid];
        gen_stat.rabbit_conflicts_lost += thread_rabbit_conflicts[tid];
    }

    std::vector<Animal> baby_rabbits;
    baby_rabbits.reserve(gen_stat.rabbit_births);
    for (int tid = 0; tid < n_threads; ++tid) {
        for (auto& baby : thread_baby_rabbits[tid]) {
            baby.id = next_animal_id_++;
            baby_rabbits.push_back(baby);
        }
    }

    // ==========================================
    // UPDATE GRID FOR FOX PHASE
    // ==========================================
#ifdef _OPENMP
    #pragma omp parallel for num_threads(n_threads) schedule(static)
#endif
    for (int idx = 0; idx < total_cells; ++idx) {
        if (grid_[idx] == ObjectType::RABBIT) {
            grid_[idx] = ObjectType::EMPTY;
        }
    }

    // Mark surviving rabbits and babies
    for (const auto& b : rabbits_) {
        if (b.alive) {
            grid_[cell_index(b.r, b.c)] = ObjectType::RABBIT;
        }
    }
    for (const auto& baby : baby_rabbits) {
        grid_[cell_index(baby.r, baby.c)] = ObjectType::RABBIT;
    }

    // ==========================================
    // PHASE 3: PARALLEL FOX MOVEMENT PROPOSALS & STARVATION
    // ==========================================
    int n_foxes = static_cast<int>(foxes_.size());
    struct FoxMoveProposal {
        int target_r;
        int target_c;
        int old_r;
        int old_c;
        bool moved;
        bool ate;
        bool starved;
        uint64_t key;
    };
    std::vector<FoxMoveProposal> fox_proposals(n_foxes);

    std::vector<std::atomic<uint64_t>> fox_target_best(total_cells);
    for (int i = 0; i < total_cells; ++i) {
        fox_target_best[i].store(0, std::memory_order_relaxed);
    }

    std::vector<int> thread_fox_starvations(n_threads, 0);

#ifdef _OPENMP
    #pragma omp parallel num_threads(n_threads)
#endif
    {
        int tid = 0;
#ifdef _OPENMP
        tid = omp_get_thread_num();
#endif
#ifdef _OPENMP
        #pragma omp for schedule(static)
#endif
        for (int j = 0; j < n_foxes; ++j) {
            auto& f = foxes_[j];
            if (!f.alive) {
                fox_proposals[j] = {0, 0, 0, 0, false, false, false, 0};
                continue;
            }

            // Check adjacent rabbits
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
                // Hunts a rabbit
                int chosen_idx = select_adjacent(g, f.r, f.c, p_rab);
                int target_r = possible_rabbits[chosen_idx].first;
                int target_c = possible_rabbits[chosen_idx].second;
                uint64_t key = make_fox_key(f.proc_age, f.food_age, f.id);

                fox_proposals[j] = {target_r, target_c, f.r, f.c, true, true, false, key};

                int t_idx = cell_index(target_r, target_c);
                uint64_t current = fox_target_best[t_idx].load(std::memory_order_relaxed);
                while (key > current && !fox_target_best[t_idx].compare_exchange_weak(
                           current, key, std::memory_order_release, std::memory_order_relaxed)) {
                }
            } else {
                // Starvation check
                if (f.food_age + 1 >= config_.gen_food_foxes) {
                    f.alive = false;
                    thread_fox_starvations[tid]++;
                    fox_proposals[j] = {0, 0, 0, 0, false, false, true, 0};
                    continue;
                }

                // Check empty adjacent cells
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

                uint64_t key = make_fox_key(f.proc_age, f.food_age, f.id);
                fox_proposals[j] = {target_r, target_c, f.r, f.c, moved, false, false, key};

                int t_idx = cell_index(target_r, target_c);
                uint64_t current = fox_target_best[t_idx].load(std::memory_order_relaxed);
                while (key > current && !fox_target_best[t_idx].compare_exchange_weak(
                           current, key, std::memory_order_release, std::memory_order_relaxed)) {
                }
            }
        }
    }

    for (int tid = 0; tid < n_threads; ++tid) {
        gen_stat.fox_starvations += thread_fox_starvations[tid];
    }

    // ==========================================
    // PHASE 4: PARALLEL FOX RESOLUTION, FEEDING & REPRODUCTION
    // ==========================================
    std::vector<std::vector<Animal>> thread_baby_foxes(n_threads);
    std::vector<int> thread_fox_births(n_threads, 0);
    std::vector<int> thread_fox_conflicts(n_threads, 0);
    std::vector<int> thread_rabbits_eaten(n_threads, 0);

    // Eaten flag per cell for O(1) lock-free marked eaten
    std::vector<std::atomic<uint8_t>> eaten_flags(total_cells);
    for (int i = 0; i < total_cells; ++i) {
        eaten_flags[i].store(0, std::memory_order_relaxed);
    }

#ifdef _OPENMP
    #pragma omp parallel num_threads(n_threads)
#endif
    {
        int tid = 0;
#ifdef _OPENMP
        tid = omp_get_thread_num();
#endif
#ifdef _OPENMP
        #pragma omp for schedule(static)
#endif
        for (int j = 0; j < n_foxes; ++j) {
            auto& f = foxes_[j];
            if (!f.alive) continue;

            const auto& prop = fox_proposals[j];
            if (prop.starved) continue;

            int t_idx = cell_index(prop.target_r, prop.target_c);
            uint64_t best_key = fox_target_best[t_idx].load(std::memory_order_acquire);

            if (prop.key == best_key) {
                // Winner survives!
                f.r = prop.target_r;
                f.c = prop.target_c;

                if (prop.ate) {
                    f.food_age = 0;
                    eaten_flags[t_idx].store(1, std::memory_order_release);
                    thread_rabbits_eaten[tid]++;
                } else {
                    f.food_age++;
                }

                if (prop.moved && f.proc_age >= config_.gen_proc_foxes) {
                    f.proc_age = 0;
                    thread_baby_foxes[tid].emplace_back(0, ObjectType::FOX, prop.old_r, prop.old_c, 0, 0);
                    thread_fox_births[tid]++;
                } else {
                    f.proc_age++;
                }
            } else {
                // Lost conflict
                f.alive = false;
                thread_fox_conflicts[tid]++;
            }
        }
    }

    // Merge fox stats
    for (int tid = 0; tid < n_threads; ++tid) {
        gen_stat.fox_births += thread_fox_births[tid];
        gen_stat.fox_conflicts_lost += thread_fox_conflicts[tid];
        gen_stat.rabbits_eaten += thread_rabbits_eaten[tid];
    }

    std::vector<Animal> baby_foxes;
    baby_foxes.reserve(gen_stat.fox_births);
    for (int tid = 0; tid < n_threads; ++tid) {
        for (auto& baby : thread_baby_foxes[tid]) {
            baby.id = next_animal_id_++;
            baby_foxes.push_back(baby);
        }
    }

    // ==========================================
    // PHASE 5: CLEANUP & COMPACTION
    // ==========================================
    // Mark eaten rabbits
#ifdef _OPENMP
    #pragma omp parallel for num_threads(n_threads) schedule(static)
#endif
    for (int i = 0; i < static_cast<int>(rabbits_.size()); ++i) {
        auto& b = rabbits_[i];
        if (b.alive && eaten_flags[cell_index(b.r, b.c)].load(std::memory_order_relaxed)) {
            b.alive = false;
        }
    }
#ifdef _OPENMP
    #pragma omp parallel for num_threads(n_threads) schedule(static)
#endif
    for (int i = 0; i < static_cast<int>(baby_rabbits.size()); ++i) {
        auto& baby = baby_rabbits[i];
        if (baby.alive && eaten_flags[cell_index(baby.r, baby.c)].load(std::memory_order_relaxed)) {
            baby.alive = false;
        }
    }

    // Sequential compaction for exact deterministic vector order
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
