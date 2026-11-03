# Microproyecto 3: Simulación de Ecosistema (Zorros, Conejos y Rocas)

Implementación de alto rendimiento en C++17 de un simulador de ecosistema predator-prey en memoria compartida, utilizando **OpenMP** y una arquitectura determinista libre de bloqueos (*lock-free*). Desarrollado para la asignatura de Computación Paralela y Distribuida.

**Repositorio Oficial:** [https://github.com/Steban-Rivera/Microproyecto-3-Ecosystem-Simulation.git](https://github.com/Steban-Rivera/Microproyecto-3-Ecosystem-Simulation.git)

---

## 1. Descripción del Proyecto y Objetivo

El proyecto modela y simula la evolución biológica y espacial discreta de un ecosistema compuesto por conejos (herbívoros), zorros (carnívoros) y rocas (obstáculos infranqueables estáticos) en un retículo bidimensional de $R$ filas por $C$ columnas a lo largo de $N_{\text{GEN}}$ generaciones.

El objetivo académico es implementar y contrastar dos motores de simulación completos y funcionales:
1. **Motor Secuencial (`SequentialEcosystem`)**: Algoritmo paso a paso determinista que sirve como base de referencia canónica.
2. **Motor Paralelo (`ParallelEcosystem`)**: Implementación concurrente basada en memoria compartida con directivas OpenMP y resolución atómica de colisiones mediante CAS (*Compare-And-Swap*).

Ambas implementaciones respetan con exactitud matemática las reglas del documento del docente y garantizan **100% de equivalencia determinista** independientemente del número de hilos ejecutados (1, 2, 4, 8, etc.).

---

## 2. Reglas del Ecosistema

### 2.1 Representación del Mundo
- Matriz bidimensional de dimensiones $R \times C$ con origen en $(0, 0)$ en la esquina superior izquierda.
- Cada celda puede contener una roca (`ROCK`), un conejo (`RABBIT`), un zorro (`FOX`) o estar vacía (`EMPTY`).
- Las rocas son estáticas e infranqueables.
- Los movimientos se realizan en cuatro direcciones ortogonales (Norte, Este, Sur, Oeste). No existen movimientos diagonales.

### 2.2 Criterio de Selección de Celda Adyacente
Para un animal en coordenadas $(X, Y)$ durante la generación $G$, las celdas adyacentes candidatas se enumeran en sentido horario a partir del Norte:
- 0: Norte $(X-1, Y)$
- 1: Este $(X, Y+1)$
- 2: Sur $(X+1, Y)$
- 3: Oeste $(X, Y-1)$

Si existen $P > 0$ celdas válidas ordenadas según esta prioridad horaria, la celda elegida corresponde al índice:
$$\text{idx} = (G + X + Y) \bmod P$$
Si $P = 0$, el animal permanece en su posición.

### 2.3 Conejos (Herbívoros)
- En cada generación, se desplazan a una celda vacía adyacente según el criterio de selección.
- Si no hay celdas vacías, permanecen en su lugar.
- **Reproducción**: Se reproducen al alcanzar la edad $\text{proc\_age} \ge \text{GEN\_PROC\_RABBITS}$ **siempre y cuando realicen un movimiento efectivo**. Al moverse, dejan una nueva cría en su posición previa con $\text{proc\_age} = 0$, y la edad del progenitor se reinicia a 0.
- **Conflictos**: Si varios conejos intentan ocupar la misma celda, sobrevive aquel con mayor edad de reproducción ($\max \text{proc\_age}$). Los empates se resuelven deterministamente por identificador menor ($\min \text{id}$). Los demás mueren.

### 2.4 Zorros (Carnívoros)
- En cada generación, primero buscan conejos adyacentes para cazar. Si hay varios disponibles, eligen uno usando el criterio de selección, se desplazan a dicha celda, devoran al conejo y reinician su hambre: $\text{food\_age} = 0$.
- **Inanición (Starvation)**: Si no encuentran conejos adyacentes, acumulan una generación sin comer. Si $\text{food\_age} + 1 \ge \text{GEN\_FOOD\_FOXES}$, el zorro **muere por inanición inmediatamente**, antes de intentar moverse a celdas vacías.
- Si no muere de hambre, intenta desplazarse a una celda vacía adyacente según el criterio de selección. Si no hay celdas vacías, permanece en su celda. En ambos casos, incrementa su hambre ($\text{food\_age} = \text{food\_age} + 1$).
- **Reproducción**: Al alcanzar $\text{proc\_age} \ge \text{GEN\_PROC\_FOXES}$ y realizar un movimiento efectivo, deja una cría en su celda previa con $\text{proc\_age} = 0$ y $\text{food\_age} = 0$.
- **Conflictos**: Si varios zorros compiten por la misma celda:
  1. Sobrevive el de mayor edad reproductiva ($\max \text{proc\_age}$).
  2. En caso de empate, sobrevive el menos hambriento ($\min \text{food\_age}$).
  3. En caso de empate, desempate determinista por identificador menor ($\min \text{id}$).
  4. Los demás zorros en conflicto mueren.

---

## 3. Arquitectura y Organización del Proyecto

```
Microproyecto-3-Ecosystem-Simulation/
├── CMakeLists.txt            # Configuración de compilación (MSVC, GCC, Clang, OpenMP, CTest)
├── README.md                 # Documentación completa y manual de usuario
├── technical_notes.md        # Fundamentos matemáticos, demostración lock-free y análisis
├── .gitignore                # Reglas de exclusión de binarios, temporales y builds
│
├── src/
│   ├── main.cpp              # Punto de entrada CLI, redirección stdin/stdout y benchmarks
│   ├── animal.hpp            # Estructuras Animal, ObjectType y funciones de vecindad
│   ├── animal.cpp
│   ├── input.hpp             # Lector, validador de restricciones y formateador de salida
│   ├── input.cpp
│   ├── ecosystem.hpp         # Clase base del ecosistema, cuadrícula y renderizado terminal
│   ├── ecosystem.cpp
│   ├── sequential.hpp        # Motor secuencial canónico de referencia
│   ├── sequential.cpp
│   ├── parallel.hpp          # Motor paralelo con OpenMP y CAS lock-free
│   ├── parallel.cpp
│   ├── statistics.hpp        # Rastreo de métricas poblacionales y exportador JSON
│   └── statistics.cpp
│
├── tests/
│   ├── test_main.cpp         # Suite automatizada de 15 pruebas unitarias y de integración
│   └── test_cases/           # Archivos de prueba individuales (.txt)
│       ├── 01_teacher_example.txt
│       ├── 02_rabbit_procreation.txt
│       ├── 03_fox_starvation.txt
│       ├── 04_rabbit_conflict.txt
│       └── 05_fox_conflict.txt
│
├── examples/
│   ├── example_input.txt     # Caso de prueba oficial del docente (5x5, 9 objetos)
│   ├── large_ecosystem.txt   # Retículo 100x100 con 2,800 objetos (30 generaciones)
│   └── benchmark_heavy.txt   # Retículo 250x250 con 21,000 objetos (40 generaciones)
│
└── visualization/
    ├── index.html            # Interfaz web interactiva del ecosistema (dashboard moderno)
    ├── style.css             # Estilos oscuros responsivos
    └── app.js                # Motor de simulación en JavaScript, canvas y Chart.js
```

---

## 4. Diferencia entre la Implementación Secuencial y Paralela

| Aspecto | Implementación Secuencial | Implementación Paralela (OpenMP) |
| :--- | :--- | :--- |
| **Control de Hilos** | Un único hilo de ejecución. | Equipo de $T$ hilos mediante `#pragma omp parallel for schedule(static)`. |
| **Resolución de Conflictos** | Búferes temporales e inserciones directas por celda. | **Lock-Free CAS:** Claves atómicas de 64 bits (`std::atomic<uint64_t>`) resueltas por hardware con `compare_exchange_weak`. |
| **Asignación de IDs a Crías** | Consecutivo de $0$ a $N-1$ según el orden de padres. | Búferes privados por hilo consolidados en orden determinista de chunks parentales. |
| **Sincronización** | No requerida. | Barreras síncronas implícitas al término de cada una de las 4 subfases por generación. |
| **Garantía Determinista** | Sí. | **Sí (100% idéntico bit a bit a la versión secuencial con 1, 2, 4 u 8 hilos)**. |

---

## 5. Requisitos de Instalación y Entorno

- **Sistema Operativo:** Windows 10/11, Linux o macOS.
- **Estándar C++:** C++17.
- **Compilador C++ con soporte OpenMP:**
  - Microsoft Visual Studio 2019 / 2022 / 2026 (MSVC con OpenMP `/openmp`), o
  - GCC / MinGW-w64 10.0+ con `libgomp`, o
  - Clang con `libomp`.
- **Sistema de Construcción:** CMake 3.16 o superior.
- **Herramienta de compilación:** Ninja o Visual Studio MSBuild.
- **Navegador Web:** Cualquier navegador moderno para la interfaz web (Google Chrome, Edge, Firefox). Requiere conexión a internet únicamente para cargar Chart.js desde CDN si se usa el gráfico dinámico.

---

## 6. Instrucciones de Compilación

### Opción A: Microsoft Visual Studio (IDE en Windows)
1. Abra Visual Studio.
2. Seleccione **Archivo** > **Abrir** > **Carpeta...** y seleccione la carpeta raíz del repositorio.
3. Visual Studio detectará automáticamente `CMakeLists.txt` y configurará el proyecto.
4. En la barra superior, elija la configuración `x64-Release`.
5. Seleccione **Compilar** > **Compilar todo** (`Ctrl + Mayús + B`).

### Opción B: Línea de Comandos con CMake y Ninja / MinGW
```powershell
# 1. Configurar la compilación
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# 2. Compilar todos los ejecutables
cmake --build build
```
Los ejecutables generados se ubicarán en `build/`:
- `build/ecosystem_sim.exe`: Simulador principal.
- `build/ecosystem_tests.exe`: Suite de pruebas unitarias y de integración.

---

## 7. Instrucciones de Ejecución

### 7.1 Modo Estándar Oficial (Entrada y Salida por Tuberías)
Cumple estrictamente con la especificación del curso, leyendo de `stdin` y escribiendo únicamente el estado final en `stdout`:
```powershell
Get-Content examples/example_input.txt | ./build/ecosystem_sim.exe
```

### 7.2 Ejecución Secuencial
```powershell
./build/ecosystem_sim.exe -s -i examples/example_input.txt
```

### 7.3 Ejecución Paralela con OpenMP
Permite especificar el número de hilos con `-t`:
```powershell
# Ejecución paralela con 4 hilos
./build/ecosystem_sim.exe -p -t 4 -i examples/example_input.txt

# Ejecución paralela con 8 hilos
./build/ecosystem_sim.exe -p -t 8 -i examples/example_input.txt
```

### 7.4 Modo Benchmark y Comprobación de Equivalencia
Ejecuta la comparación de rendimiento midiendo tiempos en la versión secuencial y en la versión paralela con 1, 2, 4 y 8 hilos, reportando speedup, eficiencia y validando que los estados finales sean matemáticamente idénticos:
```powershell
./build/ecosystem_sim.exe -i examples/large_ecosystem.txt --benchmark
```

### 7.5 Visualización Interactiva en Terminal (Modo ANSI)
Muestra la matriz en consola con emojis y colores ANSI a velocidad configurable con `--speed <milisegundos>`:
```powershell
./build/ecosystem_sim.exe -i examples/example_input.txt -v --speed 200
```

### 7.6 Exportación de Trazabilidad a JSON
```powershell
./build/ecosystem_sim.exe -i examples/example_input.txt --export-json metricas.json
```

---

## 8. Ejecución de la Suite de Pruebas Automatizadas

El proyecto incluye 15 pruebas unitarias y de integración exhaustivas en `tests/test_main.cpp`:
1. `test_teacher_reference_case`: Validación estricta del caso de referencia oficial del docente.
2. `test_input_validation`: Comprobación de coordenadas fuera de rango, dimensiones y objetos duplicados.
3. `test_rock_immobility`: Inmutabilidad y permanencia de las rocas.
4. `test_rabbit_procreation`: Verificación de reproducción exacta en generaciones programadas.
5. `test_fox_starvation`: Verificación de muerte por inanición antes de desplazarse.
6. `test_fox_hunting`: Verificación de prioridad de caza sobre desplazamiento a celdas vacías.
7. `test_rabbit_conflict_resolution`: Validación de supervivencia por mayor edad reproductiva.
8. `test_fox_conflict_resolution`: Validación de supervivencia por edad reproductiva y menor hambre.
9. `test_trapped_animal`: Comportamiento de animales acorralados sin opciones de escape.
10. `test_empty_population`: Estabilidad con matrices sin animales.
11. `test_large_multithread_equivalence`: Equivalencia determinista en matrices complejas con 1, 2, 4 y 8 hilos.
12. `test_fox_procreation`: Reproducción de zorros tras alcanzar `GEN_PROC_FOXES` y moverse.
13. `test_fox_conflict_tie_hunger`: Desempate de zorros por identificador cuando coinciden edad y hambre.
14. `test_zero_generations`: Caso límite donde $N_{\text{GEN}} = 0$ (el ecosistema no cambia).
15. `test_boundaries_and_corners`: Animales en esquinas y bordes del retículo.

### Para ejecutar las pruebas directamente:
```powershell
./build/ecosystem_tests.exe
```

### Para ejecutar mediante CTest (integrado con CMake):
```powershell
ctest --test-dir build --output-on-failure
```

---

## 9. Visualización Web Interactiva

El proyecto cuenta con una interfaz web gráfica moderna ubicada en la carpeta `visualization/`:
- **Cómo abrirla:** Basta con abrir el archivo `visualization/index.html` en cualquier navegador web moderno (doble clic o `start visualization/index.html` en PowerShell).
- **Arquitectura Transparente:** La interfaz web es una *Single Page Application* en HTML5, CSS y JavaScript cliente. El motor de simulación en `app.js` replica con total fidelidad las reglas del autómata celular del documento para brindar interactividad fluida e inmediata sin requerir la ejecución de un servidor backend.
- **Interoperabilidad con C++:** Permite arrastrar o seleccionar archivos `.json` de métricas exportados directamente por el motor C++ (`--export-json`), permitiendo visualizar la evolución real del motor OpenMP.
- **Funcionalidades:**
  - Matriz 2D dinámica con emojis (🦊 zorros, 🐰 conejos, 🪨 rocas).
  - Curvas de población dinámica en tiempo real mediante Chart.js.
  - Controles de reproducción: Reproducir, Pausar, Paso a Paso, Reiniciar y ajuste de velocidad (ms).
  - Presets incluidos: Ejemplo Oficial del Profesor, Equilibrio 20x20 y Superpoblación 15x15.
  - Carga de archivos `.txt` de configuración y exportación del estado final en el formato exacto del profesor.

---

## 10. Resultados Empíricos de Rendimiento

Mediciones realizadas en Windows 11 sobre procesador Intel con 20 hilos lógicos (promedios sobre 5 repeticiones independientes):

### Escenario Mediano (`examples/large_ecosystem.txt` - Retículo 100x100, 30 generaciones, 2,800 objetos):
| Modalidad | Hilos | Tiempo Medio | Desv. Est. | Speedup ($S_p$) | Eficiencia ($E_p$) | Equivalencia |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| Secuencial | 1 | 9.79 ms | ±0.31 ms | 1.00x | 100.0% | Referencia |
| Paralelo | 1 | 5.98 ms | ±0.18 ms | 1.64x | 163.8% | **EXACTA (0 diffs)** |
| Paralelo | 2 | 9.96 ms | ±0.42 ms | 0.98x | 49.2% | **EXACTA (0 diffs)** |
| Paralelo | 4 | 15.70 ms | ±0.65 ms | 0.62x | 15.6% | **EXACTA (0 diffs)** |
| Paralelo | 8 | 17.71 ms | ±0.78 ms | 0.55x | 6.9% | **EXACTA (0 diffs)** |

### Escenario Pesado (`examples/benchmark_heavy.txt` - Retículo 250x250, 40 generaciones, 21,000 objetos):
| Modalidad | Hilos | Tiempo Medio | Desv. Est. | Speedup ($S_p$) | Eficiencia ($E_p$) | Equivalencia |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| Secuencial | 1 | 83.99 ms | ±1.45 ms | 1.00x | 100.0% | Referencia |
| Paralelo | 1 | 44.10 ms | ±0.82 ms | 1.90x | 190.4% | **EXACTA (0 diffs)** |
| Paralelo | 2 | 50.45 ms | ±1.12 ms | 1.66x | 83.2% | **EXACTA (0 diffs)** |
| Paralelo | 4 | 50.33 ms | ±1.05 ms | 1.67x | 41.7% | **EXACTA (0 diffs)** |
| Paralelo | 8 | 52.40 ms | ±1.30 ms | 1.60x | 20.0% | **EXACTA (0 diffs)** |
