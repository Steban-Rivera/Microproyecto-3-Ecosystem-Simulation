#include "input.hpp"
#include "sequential.hpp"
#include "parallel.hpp"
#include <iostream>
#include <sstream>
#include <cassert>
#include <vector>
#include <string>
#include <set>

int g_total_tests = 0;
int g_passed_tests = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  [FAIL] Line " << __LINE__ << ": " << msg << "\n"; \
            return false; \
        } \
    } while (0)

#define RUN_TEST(fn) \
    do { \
        g_total_tests++; \
        std::cout << "[TEST " << g_total_tests << "] Running " << #fn << "...\n"; \
        if (fn()) { \
            g_passed_tests++; \
            std::cout << "  -> PASSED\n"; \
        } else { \
            std::cout << "  -> FAILED\n"; \
        } \
    } while (0)

// Helper to compare object lists
bool objects_equal(const std::vector<InitialObject>& a, const std::vector<InitialObject>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].type != b[i].type || a[i].r != b[i].r || a[i].c != b[i].c) {
            return false;
        }
    }
    return true;
}

// 1. Mandatory Teacher Reference Case
bool test_teacher_reference_case() {
    std::string input = 
        "2 4 3 6 5 5 9\n"
        "ROCK 0 0\n"
        "RABBIT 0 2\n"
        "FOX 0 4\n"
        "FOX 1 0\n"
        "FOX 1 4\n"
        "ROCK 2 4\n"
        "RABBIT 3 0\n"
        "RABBIT 4 0\n"
        "FOX 4 4\n";

    Config cfg;
    std::vector<InitialObject> objs;
    std::string err;
    std::istringstream iss(input);
    TEST_ASSERT(InputManager::parse_input(iss, cfg, objs, err), "Failed to parse teacher input");

    std::vector<InitialObject> expected = {
        {ObjectType::ROCK, 0, 0},
        {ObjectType::ROCK, 2, 4},
        {ObjectType::RABBIT, 3, 0},
        {ObjectType::FOX, 4, 0},
        {ObjectType::FOX, 4, 3}
    };

    // Sequential test
    SequentialEcosystem seq;
    seq.initialize(cfg, objs);
    seq.run();
    std::vector<InitialObject> seq_res = seq.get_current_objects();
    TEST_ASSERT(objects_equal(seq_res, expected), "Sequential result does not match teacher reference");

    // Parallel test with 1, 2, 4, 8 threads
    for (int th : {1, 2, 4, 8}) {
        ParallelEcosystem par(th);
        par.initialize(cfg, objs);
        par.run();
        std::vector<InitialObject> par_res = par.get_current_objects();
        TEST_ASSERT(objects_equal(par_res, expected), "Parallel (" + std::to_string(th) + " threads) does not match teacher reference");
    }

    return true;
}

// 2. Input validation: coordinates and dimensions
bool test_input_validation() {
    Config cfg;
    std::vector<InitialObject> objs;
    std::string err;

    // Out-of-bounds coordinate
    std::string bad_coord =
        "2 4 3 1 5 5 1\n"
        "RABBIT 5 5\n"; // 5x5 grid has max index 4
    std::istringstream iss1(bad_coord);
    TEST_ASSERT(!InputManager::parse_input(iss1, cfg, objs, err), "Should fail on out-of-bounds coordinate");

    // Invalid negative dimensions
    std::string bad_dim =
        "2 4 3 1 -5 5 0\n";
    std::istringstream iss2(bad_dim);
    TEST_ASSERT(!InputManager::parse_input(iss2, cfg, objs, err), "Should fail on negative grid dimensions");

    // Duplicate object on same cell
    std::string duplicate_cell =
        "2 4 3 1 5 5 2\n"
        "ROCK 1 1\n"
        "FOX 1 1\n";
    std::istringstream iss3(duplicate_cell);
    TEST_ASSERT(!InputManager::parse_input(iss3, cfg, objs, err), "Should fail on duplicate coordinate");

    // Unknown object type
    std::string bad_type =
        "2 4 3 1 5 5 1\n"
        "WOLF 2 2\n";
    std::istringstream iss4(bad_type);
    TEST_ASSERT(!InputManager::parse_input(iss4, cfg, objs, err), "Should fail on unknown object type");

    return true;
}

// 3. Rock Immobility
bool test_rock_immobility() {
    std::string input =
        "2 4 3 10 4 4 3\n"
        "ROCK 0 0\n"
        "ROCK 1 2\n"
        "ROCK 3 3\n";

    Config cfg;
    std::vector<InitialObject> objs;
    std::string err;
    std::istringstream iss(input);
    InputManager::parse_input(iss, cfg, objs, err);

    SequentialEcosystem seq;
    seq.initialize(cfg, objs);
    seq.run();
    TEST_ASSERT(objects_equal(seq.get_current_objects(), objs), "Rocks moved or vanished in sequential run");

    ParallelEcosystem par(4);
    par.initialize(cfg, objs);
    par.run();
    TEST_ASSERT(objects_equal(par.get_current_objects(), objs), "Rocks moved or vanished in parallel run");

    return true;
}

// 4. Rabbit Procreation
bool test_rabbit_procreation() {
    std::string input =
        "2 4 3 3 5 5 1\n"
        "RABBIT 2 2\n";

    Config cfg;
    std::vector<InitialObject> objs;
    std::string err;
    std::istringstream iss(input);
    InputManager::parse_input(iss, cfg, objs, err);

    SequentialEcosystem seq;
    seq.initialize(cfg, objs);
    seq.run();
    TEST_ASSERT(seq.get_rabbits().size() == 2, "Rabbit did not procreate as expected in sequential run");

    ParallelEcosystem par(2);
    par.initialize(cfg, objs);
    par.run();
    TEST_ASSERT(par.get_rabbits().size() == 2, "Rabbit did not procreate as expected in parallel run");
    TEST_ASSERT(objects_equal(seq.get_current_objects(), par.get_current_objects()), "Sequential and parallel disagree on procreation");

    return true;
}

// 5. Fox Starvation
bool test_fox_starvation() {
    std::string input =
        "2 4 2 2 3 3 1\n"
        "FOX 1 1\n";

    Config cfg;
    std::vector<InitialObject> objs;
    std::string err;
    std::istringstream iss(input);
    InputManager::parse_input(iss, cfg, objs, err);

    SequentialEcosystem seq;
    seq.initialize(cfg, objs);
    seq.run();
    TEST_ASSERT(seq.get_foxes().empty(), "Fox should have starved and died in sequential run");

    ParallelEcosystem par(4);
    par.initialize(cfg, objs);
    par.run();
    TEST_ASSERT(par.get_foxes().empty(), "Fox should have starved and died in parallel run");

    return true;
}

// 6. Fox Hunting and Eating
bool test_fox_hunting() {
    // Rabbit at (1, 1), enclosed by rocks so it cannot flee.
    // Fox at (1, 0) detects rabbit at (1, 1) and moves to eat it.
    std::string input =
        "4 4 3 1 3 3 5\n"
        "ROCK 0 1\n"
        "ROCK 1 2\n"
        "ROCK 2 1\n"
        "FOX 1 0\n"
        "RABBIT 1 1\n";

    Config cfg;
    std::vector<InitialObject> objs;
    std::string err;
    std::istringstream iss(input);
    TEST_ASSERT(InputManager::parse_input(iss, cfg, objs, err), "Failed to parse test_fox_hunting input: " + err);

    SequentialEcosystem seq;
    seq.initialize(cfg, objs);
    seq.step();
    TEST_ASSERT(seq.get_rabbits().empty(), "Rabbit was not eaten by hunting fox in sequential run");
    TEST_ASSERT(seq.get_foxes().size() == 1, "Fox died unexpectedly");
    TEST_ASSERT(seq.get_foxes()[0].food_age == 0, "Fox food_age not reset after eating");
    TEST_ASSERT(seq.get_foxes()[0].r == 1 && seq.get_foxes()[0].c == 1, "Fox should be at (1, 1) after eating");

    ParallelEcosystem par(2);
    par.initialize(cfg, objs);
    par.step();
    TEST_ASSERT(par.get_rabbits().empty(), "Rabbit was not eaten in parallel run");
    TEST_ASSERT(par.get_foxes().size() == 1, "Fox died unexpectedly in parallel run");
    TEST_ASSERT(objects_equal(seq.get_current_objects(), par.get_current_objects()), "Sequential and parallel disagree on hunting");

    return true;
}

// 7. Rabbit Conflict Resolution (Older procreation age wins)
bool test_rabbit_conflict_resolution() {
    std::string input =
        "5 5 5 1 3 3 6\n"
        "ROCK 0 0\n"
        "ROCK 0 2\n"
        "ROCK 2 0\n"
        "ROCK 2 2\n"
        "RABBIT 0 1\n"
        "RABBIT 2 1\n";

    Config cfg;
    std::vector<InitialObject> objs;
    std::string err;
    std::istringstream iss(input);
    InputManager::parse_input(iss, cfg, objs, err);

    SequentialEcosystem seq;
    seq.initialize(cfg, objs);
    seq.step();
    TEST_ASSERT(seq.get_rabbits().size() == 1, "Expected exactly 1 surviving rabbit after conflict");
    TEST_ASSERT(seq.get_rabbits()[0].r == 1 && seq.get_rabbits()[0].c == 1, "Winner should be at (1, 1)");

    ParallelEcosystem par(4);
    par.initialize(cfg, objs);
    par.step();
    TEST_ASSERT(par.get_rabbits().size() == 1, "Expected exactly 1 surviving rabbit in parallel");
    TEST_ASSERT(objects_equal(seq.get_current_objects(), par.get_current_objects()), "Sequential and parallel disagree on rabbit conflict");

    return true;
}

// 8. Fox Conflict Resolution (Older procreation age, then least hungry)
bool test_fox_conflict_resolution() {
    std::string input =
        "5 5 5 1 3 3 6\n"
        "ROCK 0 0\n"
        "ROCK 0 2\n"
        "ROCK 2 0\n"
        "ROCK 2 2\n"
        "FOX 0 1\n"
        "FOX 2 1\n";

    Config cfg;
    std::vector<InitialObject> objs;
    std::string err;
    std::istringstream iss(input);
    InputManager::parse_input(iss, cfg, objs, err);

    SequentialEcosystem seq;
    seq.initialize(cfg, objs);
    seq.step();
    TEST_ASSERT(seq.get_foxes().size() == 1, "Expected exactly 1 surviving fox after conflict");
    TEST_ASSERT(seq.get_foxes()[0].r == 1 && seq.get_foxes()[0].c == 1, "Winner should be at (1, 1)");

    ParallelEcosystem par(4);
    par.initialize(cfg, objs);
    par.step();
    TEST_ASSERT(par.get_foxes().size() == 1, "Expected exactly 1 surviving fox in parallel");
    TEST_ASSERT(objects_equal(seq.get_current_objects(), par.get_current_objects()), "Sequential and parallel disagree on fox conflict");

    return true;
}

// 9. Animal Trapped (No available movements)
bool test_trapped_animal() {
    std::string input =
        "2 4 3 5 3 3 3\n"
        "RABBIT 0 0\n"
        "ROCK 0 1\n"
        "ROCK 1 0\n";

    Config cfg;
    std::vector<InitialObject> objs;
    std::string err;
    std::istringstream iss(input);
    InputManager::parse_input(iss, cfg, objs, err);

    SequentialEcosystem seq;
    seq.initialize(cfg, objs);
    seq.run();
    TEST_ASSERT(seq.get_rabbits().size() == 1, "Trapped rabbit should stay alive");
    TEST_ASSERT(seq.get_rabbits()[0].r == 0 && seq.get_rabbits()[0].c == 0, "Trapped rabbit must remain at (0, 0)");
    TEST_ASSERT(seq.get_rabbits().size() == 1, "Trapped rabbit should not leave baby without moving");

    ParallelEcosystem par(4);
    par.initialize(cfg, objs);
    par.run();
    TEST_ASSERT(objects_equal(seq.get_current_objects(), par.get_current_objects()), "Sequential and parallel disagree on trapped rabbit");

    return true;
}

// 10. Empty Population Case
bool test_empty_population() {
    std::string input =
        "2 4 3 5 4 4 1\n"
        "ROCK 2 2\n";

    Config cfg;
    std::vector<InitialObject> objs;
    std::string err;
    std::istringstream iss(input);
    InputManager::parse_input(iss, cfg, objs, err);

    SequentialEcosystem seq;
    seq.initialize(cfg, objs);
    seq.run();
    TEST_ASSERT(seq.get_rabbits().empty() && seq.get_foxes().empty() && seq.get_rocks().size() == 1, "Failed empty population");

    ParallelEcosystem par(4);
    par.initialize(cfg, objs);
    par.run();
    TEST_ASSERT(objects_equal(seq.get_current_objects(), par.get_current_objects()), "Parallel failed empty population");

    return true;
}

// 11. Large Complex Grid Multi-Thread Equivalence
bool test_large_multithread_equivalence() {
    std::set<std::pair<int, int>> used;
    std::vector<InitialObject> generated_objs;

    for (int r = 0; r < 15; ++r) {
        generated_objs.push_back({ObjectType::ROCK, r, r});
        used.insert({r, r});
    }

    int r = 0, c = 1;
    while (generated_objs.size() < 45) {
        if (!used.count({r, c})) {
            generated_objs.push_back({ObjectType::RABBIT, r, c});
            used.insert({r, c});
        }
        c = (c + 3) % 20;
        if (c == 0) r = (r + 1) % 20;
    }

    while (generated_objs.size() < 65) {
        if (!used.count({r, c})) {
            generated_objs.push_back({ObjectType::FOX, r, c});
            used.insert({r, c});
        }
        c = (c + 7) % 20;
        if (c == 0) r = (r + 1) % 20;
    }

    std::ostringstream ss;
    ss << "3 5 4 15 20 20 " << generated_objs.size() << "\n";
    for (const auto& obj : generated_objs) {
        ss << object_type_to_string(obj.type) << " " << obj.r << " " << obj.c << "\n";
    }

    Config cfg;
    std::vector<InitialObject> objs;
    std::string err;
    std::istringstream iss(ss.str());
    TEST_ASSERT(InputManager::parse_input(iss, cfg, objs, err), "Failed to parse generated complex grid: " + err);

    SequentialEcosystem seq;
    seq.initialize(cfg, objs);
    seq.run();
    std::vector<InitialObject> seq_res = seq.get_current_objects();

    for (int th : {1, 2, 4, 8}) {
        ParallelEcosystem par(th);
        par.initialize(cfg, objs);
        par.run();
        std::vector<InitialObject> par_res = par.get_current_objects();
        TEST_ASSERT(objects_equal(seq_res, par_res), "Parallel with " + std::to_string(th) + " threads differs from sequential");
    }

    return true;
}

// 12. Fox Procreation Unit Test
bool test_fox_procreation() {
    // GEN_PROC_FOXES = 2, GEN_FOOD_FOXES = 5, N_GEN = 3.
    // 1 fox in a 5x5 grid with no nearby rabbits, so it moves to empty cells.
    // Gen 0: fox moves, proc_age becomes 1, food_age becomes 1.
    // Gen 1: fox moves, proc_age becomes 2, food_age becomes 2.
    // Gen 2: starts with proc_age = 2 >= GEN_PROC_FOXES.
    // Fox moves and procreates: leaves baby fox at previous cell with proc_age=0, food_age=0.
    // Total foxes should be 2.
    std::string input =
        "5 2 5 3 5 5 1\n"
        "FOX 2 2\n";

    Config cfg;
    std::vector<InitialObject> objs;
    std::string err;
    std::istringstream iss(input);
    TEST_ASSERT(InputManager::parse_input(iss, cfg, objs, err), "Failed to parse test_fox_procreation input");

    SequentialEcosystem seq;
    seq.initialize(cfg, objs);
    seq.run();
    TEST_ASSERT(seq.get_foxes().size() == 2, "Fox did not procreate in sequential run");

    for (int th : {1, 2, 4, 8}) {
        ParallelEcosystem par(th);
        par.initialize(cfg, objs);
        par.run();
        TEST_ASSERT(par.get_foxes().size() == 2, "Fox did not procreate in parallel run (" + std::to_string(th) + " threads)");
        TEST_ASSERT(objects_equal(seq.get_current_objects(), par.get_current_objects()), "Sequential and parallel disagree on fox procreation");
    }

    return true;
}

// 13. Fox Conflict: Same proc_age, Different Hunger (Least hungry wins)
bool test_fox_conflict_tie_hunger() {
    // Grid 3x3:
    // Cell (1, 1) is the center.
    // Surround other directions with rocks so (1, 1) is the only move.
    // Fox 1 at (0, 1), Fox 2 at (2, 1).
    // We want Fox 1 to be less hungry than Fox 2.
    // To give Fox 2 higher hunger naturally, we let Fox 2 starve for 1 generation first!
    // Or we test through stepping:
    // Let's create an ecosystem where Fox 2 was active longer or we verify tie-breaker logic.
    // In our direct test:
    // Fox A at (0, 1), Fox B at (2, 1).
    // If they start at Gen 0 with food_age=0, they have equal hunger.
    // What if a rabbit was adjacent to Fox A at Gen 0 so Fox A ate at Gen 0 (food_age resets to 0),
    // while Fox B did not eat at Gen 0 (food_age becomes 1)?
    // Then at Gen 1, both move to (1, 1) and have the SAME proc_age=1, but Fox A has food_age=0 and Fox B has food_age=1!
    // Fox A (least hungry) MUST win!
    std::string input =
        "5 5 4 2 3 3 7\n"
        "ROCK 0 0\n"
        "ROCK 0 2\n"
        "ROCK 2 0\n"
        "ROCK 2 2\n"
        "RABBIT 1 0\n"  // Fox A at (0, 1) will eat this rabbit at Gen 0!
        "FOX 0 1\n"     // Fox A
        "FOX 2 1\n";    // Fox B (cannot reach rabbit, moves to empty or stays)

    Config cfg;
    std::vector<InitialObject> objs;
    std::string err;
    std::istringstream iss(input);
    TEST_ASSERT(InputManager::parse_input(iss, cfg, objs, err), "Failed to parse tie hunger input");

    SequentialEcosystem seq;
    seq.initialize(cfg, objs);
    seq.run();

    for (int th : {1, 2, 4, 8}) {
        ParallelEcosystem par(th);
        par.initialize(cfg, objs);
        par.run();
        TEST_ASSERT(objects_equal(seq.get_current_objects(), par.get_current_objects()), "Tie hunger conflict differs between seq and par");
    }

    return true;
}

// 14. Zero Generations (N_GEN = 0)
bool test_zero_generations() {
    std::string input =
        "2 4 3 0 5 5 3\n"
        "ROCK 1 1\n"
        "RABBIT 2 2\n"
        "FOX 3 3\n";

    Config cfg;
    std::vector<InitialObject> objs;
    std::string err;
    std::istringstream iss(input);
    TEST_ASSERT(InputManager::parse_input(iss, cfg, objs, err), "Failed to parse N_GEN=0 input");

    SequentialEcosystem seq;
    seq.initialize(cfg, objs);
    seq.run();
    std::vector<InitialObject> seq_res = seq.get_current_objects();
    TEST_ASSERT(objects_equal(seq_res, objs), "State changed with N_GEN=0 in sequential");

    for (int th : {1, 2, 4, 8}) {
        ParallelEcosystem par(th);
        par.initialize(cfg, objs);
        par.run();
        std::vector<InitialObject> par_res = par.get_current_objects();
        TEST_ASSERT(objects_equal(par_res, objs), "State changed with N_GEN=0 in parallel");
    }

    return true;
}

// 15. Boundaries and All Four Corners
bool test_boundaries_and_corners() {
    // 5x5 grid with animals on the 4 corners: (0, 0), (0, 4), (4, 0), (4, 4)
    std::string input =
        "3 5 4 4 5 5 4\n"
        "RABBIT 0 0\n"
        "RABBIT 0 4\n"
        "FOX 4 0\n"
        "FOX 4 4\n";

    Config cfg;
    std::vector<InitialObject> objs;
    std::string err;
    std::istringstream iss(input);
    TEST_ASSERT(InputManager::parse_input(iss, cfg, objs, err), "Failed to parse corner input");

    SequentialEcosystem seq;
    seq.initialize(cfg, objs);
    seq.run();
    std::vector<InitialObject> seq_res = seq.get_current_objects();

    for (int th : {1, 2, 4, 8}) {
        ParallelEcosystem par(th);
        par.initialize(cfg, objs);
        par.run();
        std::vector<InitialObject> par_res = par.get_current_objects();
        TEST_ASSERT(objects_equal(seq_res, par_res), "Corners test differs between seq and par (" + std::to_string(th) + " threads)");
    }

    return true;
}

int main() {
    std::cout << "\n======================================================\n";
    std::cout << "  MICROPROYECTO 3: ECOSYSTEM SIMULATOR - TEST SUITE   \n";
    std::cout << "======================================================\n";

    RUN_TEST(test_teacher_reference_case);
    RUN_TEST(test_input_validation);
    RUN_TEST(test_rock_immobility);
    RUN_TEST(test_rabbit_procreation);
    RUN_TEST(test_fox_starvation);
    RUN_TEST(test_fox_hunting);
    RUN_TEST(test_rabbit_conflict_resolution);
    RUN_TEST(test_fox_conflict_resolution);
    RUN_TEST(test_trapped_animal);
    RUN_TEST(test_empty_population);
    RUN_TEST(test_large_multithread_equivalence);
    RUN_TEST(test_fox_procreation);
    RUN_TEST(test_fox_conflict_tie_hunger);
    RUN_TEST(test_zero_generations);
    RUN_TEST(test_boundaries_and_corners);

    std::cout << "\n------------------------------------------------------\n";
    std::cout << "Tests Summary: " << g_passed_tests << " / " << g_total_tests << " Passed.\n";
    std::cout << "======================================================\n\n";

    return (g_passed_tests == g_total_tests) ? 0 : 1;
}
