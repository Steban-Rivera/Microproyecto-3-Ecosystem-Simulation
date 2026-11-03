#include "input.hpp"
#include "sequential.hpp"
#include "parallel.hpp"
#include <iostream>
#include <fstream>
#include <memory>
#include <cstring>
#include <thread>
#include <chrono>
#include <algorithm>

#ifdef _OPENMP
#include <omp.h>
#endif

void print_help(const char* prog_name) {
    std::cerr << "Uso: " << prog_name << " [OPCIONES] < entrada.txt\n"
              << "Simulador de Ecosistema (Zorros, Conejos y Rocas) - Microproyecto 3\n\n"
              << "Opciones:\n"
              << "  -s, --sequential        Ejecutar versión secuencial.\n"
              << "  -p, --parallel          Ejecutar versión paralela con OpenMP (por defecto).\n"
              << "  -t, --threads <N>       Número de hilos OpenMP a utilizar.\n"
              << "  -i, --input <archivo>   Leer entrada desde archivo en lugar de stdin.\n"
              << "  --benchmark             Comparar secuencial vs paralelo (1, 2, 4, 8 hilos).\n"
              << "  -v, --visualize         Visualización interactiva en consola (ANSI).\n"
              << "  --speed <ms>            Retardo por generación en visualización (defecto: 150ms).\n"
              << "  --stats                 Imprimir estadísticas detalladas por stderr.\n"
              << "  --export-json <archivo> Exportar trazabilidad completa a formato JSON.\n"
              << "  -h, --help              Mostrar este mensaje de ayuda.\n";
}

int main(int argc, char* argv[]) {
    bool use_parallel = true;
    int num_threads = 0;
    bool benchmark_mode = false;
    bool visualize_mode = false;
    bool print_stats = false;
    int delay_ms = 150;
    std::string input_file = "";
    std::string export_json_file = "";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-s" || arg == "--sequential") {
            use_parallel = false;
        } else if (arg == "-p" || arg == "--parallel") {
            use_parallel = true;
        } else if (arg == "-t" || arg == "--threads") {
            if (i + 1 < argc) {
                num_threads = std::stoi(argv[++i]);
            } else {
                std::cerr << "Error: Se requiere un número de hilos tras " << arg << "\n";
                return 1;
            }
        } else if (arg == "-i" || arg == "--input") {
            if (i + 1 < argc) {
                input_file = argv[++i];
            } else {
                std::cerr << "Error: Se requiere la ruta del archivo tras " << arg << "\n";
                return 1;
            }
        } else if (arg == "--benchmark") {
            benchmark_mode = true;
        } else if (arg == "-v" || arg == "--visualize") {
            visualize_mode = true;
        } else if (arg == "--speed") {
            if (i + 1 < argc) {
                delay_ms = std::stoi(argv[++i]);
            }
        } else if (arg == "--stats") {
            print_stats = true;
        } else if (arg == "--export-json") {
            if (i + 1 < argc) {
                export_json_file = argv[++i];
            }
        } else if (arg == "-h" || arg == "--help") {
            print_help(argv[0]);
            return 0;
        }
    }

    // Input stream handling
    std::ifstream file_stream;
    std::istream* input_stream = &std::cin;

    if (!input_file.empty()) {
        file_stream.open(input_file);
        if (!file_stream.is_open()) {
            std::cerr << "Error: No se pudo abrir el archivo de entrada: " << input_file << "\n";
            return 1;
        }
        input_stream = &file_stream;
    }

    Config config;
    std::vector<InitialObject> initial_objects;
    std::string error_msg;

    if (!InputManager::parse_input(*input_stream, config, initial_objects, error_msg)) {
        std::cerr << "Error de entrada: " << error_msg << "\n";
        return 1;
    }

    // Benchmark mode
    if (benchmark_mode) {
        std::cerr << "\n================ BENCHMARK: SECUENCIAL VS PARALELO ================\n";
        std::cerr << "Config: " << config.rows << "x" << config.cols << " grid, "
                  << config.n_gen << " generaciones, " << initial_objects.size() << " objetos iniciales.\n";

        // 1. Sequential Run
        SequentialEcosystem seq_sim;
        seq_sim.initialize(config, initial_objects);
        auto t0_seq = std::chrono::high_resolution_clock::now();
        seq_sim.run();
        auto t1_seq = std::chrono::high_resolution_clock::now();
        double seq_time = std::chrono::duration<double, std::milli>(t1_seq - t0_seq).count();

        std::vector<InitialObject> seq_res = seq_sim.get_current_objects();
        std::cerr << "Secuencial: " << seq_time << " ms\n";

        // 2. Parallel Runs with different thread counts
        std::vector<int> test_threads = {1, 2, 4, 8};
#ifdef _OPENMP
        int max_th = omp_get_max_threads();
        if (std::find(test_threads.begin(), test_threads.end(), max_th) == test_threads.end()) {
            test_threads.push_back(max_th);
        }
#endif
        std::sort(test_threads.begin(), test_threads.end());

        for (int th : test_threads) {
            ParallelEcosystem par_sim(th);
            par_sim.initialize(config, initial_objects);
            auto t0_par = std::chrono::high_resolution_clock::now();
            par_sim.run();
            auto t1_par = std::chrono::high_resolution_clock::now();
            double par_time = std::chrono::duration<double, std::milli>(t1_par - t0_par).count();

            std::vector<InitialObject> par_res = par_sim.get_current_objects();

            bool equal = (seq_res.size() == par_res.size());
            if (equal) {
                for (size_t k = 0; k < seq_res.size(); ++k) {
                    if (seq_res[k].type != par_res[k].type ||
                        seq_res[k].r != par_res[k].r ||
                        seq_res[k].c != par_res[k].c) {
                        equal = false;
                        break;
                    }
                }
            }

            double speedup = seq_time / (par_time > 0.0001 ? par_time : 0.0001);
            double efficiency = (speedup / th) * 100.0;

            std::cerr << "Paralelo (" << th << " hilos): "
                      << par_time << " ms | Speedup: " << speedup << "x"
                      << " | Eficiencia: " << efficiency << "%"
                      << " | Equivalencia: " << (equal ? "EXACTA (OK)" : "FALLÓ") << "\n";
        }
        std::cerr << "===================================================================\n\n";

        // Still output the final state of the simulation to stdout
        InputManager::write_output(std::cout, config, seq_res);
        return 0;
    }

    // Normal Execution Mode
    std::unique_ptr<Ecosystem> sim;
    if (use_parallel) {
        sim = std::make_unique<ParallelEcosystem>(num_threads);
    } else {
        sim = std::make_unique<SequentialEcosystem>();
    }

    sim->initialize(config, initial_objects);

    if (visualize_mode) {
        std::cerr << "\nIniciando visualización en consola del ecosistema...\n";
        sim->render_console(std::cerr, true);
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms * 2));

        for (int g = 0; g < config.n_gen; ++g) {
            sim->step();
            sim->render_console(std::cerr, true);
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        }
    } else {
        sim->run();
    }

    // Mandatory final ecosystem state output to STDOUT
    InputManager::write_output(std::cout, config, sim->get_current_objects());

    if (print_stats) {
        sim->get_statistics().print_summary(std::cerr);
    }

    if (!export_json_file.empty()) {
        sim->get_statistics().export_json_file(export_json_file);
        std::cerr << "Estadísticas exportadas a: " << export_json_file << "\n";
    }

    return 0;
}
