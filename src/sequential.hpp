#pragma once

#include "ecosystem.hpp"

class SequentialEcosystem : public Ecosystem {
public:
    SequentialEcosystem();
    void step() override;
};
