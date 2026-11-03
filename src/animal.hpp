#pragma once

#include <string>
#include <vector>
#include <utility>
#include <cstdint>

enum class ObjectType : uint8_t {
    EMPTY = 0,
    ROCK = 1,
    RABBIT = 2,
    FOX = 3
};

inline const char* object_type_to_string(ObjectType type) {
    switch (type) {
        case ObjectType::ROCK:   return "ROCK";
        case ObjectType::RABBIT: return "RABBIT";
        case ObjectType::FOX:    return "FOX";
        default:                 return "EMPTY";
    }
}

inline ObjectType string_to_object_type(const std::string& str) {
    if (str == "ROCK")   return ObjectType::ROCK;
    if (str == "RABBIT") return ObjectType::RABBIT;
    if (str == "FOX")    return ObjectType::FOX;
    return ObjectType::EMPTY;
}

// Direction offsets in clockwise order:
// 0: North, 1: East, 2: South, 3: West
struct Direction {
    int dr;
    int dc;
};

inline constexpr Direction CLOCKWISE_DIRS[4] = {
    {-1,  0}, // 0: North
    { 0,  1}, // 1: East
    { 1,  0}, // 2: South
    { 0, -1}  // 3: West
};

// Selection criteria: (G + X + Y) mod P
inline int select_adjacent(int g, int r, int c, int p) {
    if (p <= 0) return 0;
    long long sum = static_cast<long long>(g) + r + c;
    long long rem = sum % p;
    if (rem < 0) rem += p;
    return static_cast<int>(rem);
}

// Animal structure representing either a Rabbit or Fox
struct Animal {
    int id = 0;              // Unique identifier (useful for deterministic tie-breaking)
    ObjectType type = ObjectType::EMPTY;
    int r = 0;               // Row coordinate
    int c = 0;               // Column coordinate
    int proc_age = 0;        // Generations since birth or last procreation
    int food_age = 0;        // Generations since birth or last meal (only applicable for foxes)
    bool alive = true;       // Survival flag

    Animal() = default;
    Animal(int id_, ObjectType type_, int r_, int c_, int proc_ = 0, int food_ = 0)
        : id(id_), type(type_), r(r_), c(c_), proc_age(proc_), food_age(food_), alive(true) {}
};
