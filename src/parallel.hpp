#pragma once

#include "ecosystem.hpp"

class ParallelEcosystem : public Ecosystem {
private:
    int num_threads_ = 0; // 0 means default OpenMP threads

public:
    ParallelEcosystem(int num_threads = 0);
    void set_num_threads(int num_threads);
    int get_num_threads() const { return num_threads_; }
    void step() override;
};
