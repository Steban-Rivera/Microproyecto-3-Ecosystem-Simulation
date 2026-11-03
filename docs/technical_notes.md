# Notas Técnicas y Fundamentos Algorítmicos

Este documento describe con detalle la teoría, formulación matemática y decisiones de diseño implementadas en el **Microproyecto 3: Simulación de Ecosistema**.

---

## 1. Modelo Discreto del Ecosistema

El ecosistema se modela como un autómata celular bidimensional síncrono sobre un retículo finito de dimensiones $R \times C$:
$$\Omega = \{ (r, c) \in \mathbb{N}^2 \mid 0 \le r < R, \ 0 \le c < C \}$$

Cada celda $u = (r, c) \in \Omega$ puede encontrarse en uno de cuatro estados:
$$\text{Estado}(u) \in \{ \text{EMPTY}, \text{ROCK}, \text{RABBIT}, \text{FOX} \}$$

### 1.1 Vecindad y Criterio de Selección

La vecindad está restringida a los 4 vecinos ortogonales (vecindad de von Neumann de radio 1):
$$\mathcal{N}(r, c) = \{ (r-1, c), \ (r, c+1), \ (r+1, c), \ (r, c-1) \}$$

El orden de exploración es estrictamente en sentido horario:
0. **Norte:** $(r-1, c)$
1. **Este:** $(r, c+1)$
2. **Sur:** $(r+1, c)$
3. **Oeste:** $(r, c-1)$

Para un conjunto ordenado de $P$ celdas válidas $V = [v_0, v_1, \dots, v_{P-1}]$ detectadas en sentido horario, la celda destino seleccionada en la generación $G$ desde la posición $(X, Y)$ está dada por la función de selección determinista:
$$\text{idx} = (G + X + Y) \bmod P$$
$$\text{Destino} = v_{\text{idx}}$$

Si $P = 0$, el animal no se desplaza y permanece en $(X, Y)$.

---

## 2. Dinámica de los Conejos (Herbívoros)

1. **Movimiento:**
   Un conejo evalúa sus celdas adyacentes vacías ($\text{Estado} = \text{EMPTY}$). Si $P > 0$, elige una según el criterio $(G+X+Y) \bmod P$.
2. **Reproducción:**
   - La edad reproductiva $\text{proc\_age}$ inicia en 0.
   - Si $\text{proc\_age} \ge \text{GEN\_PROC\_RABBITS}$ **y el conejo realiza un movimiento efectivo**, deja una nueva cría en su posición previa $(X_{\text{old}}, Y_{\text{old}})$.
   - Ambas edades reproductivas (progenitor y cría) se reinician a 0.
   - Si no se mueve, no procrea y su edad reproductiva se incrementa.
3. **Resolución de Conflictos:**
   Si $k \ge 2$ conejos compiten por la misma celda destino:
   $$\text{Ganador} = \arg\max_{b \in \text{Candidatos}} (\text{proc\_age}(b))$$
   En caso de empate en $\text{proc\_age}$, se utiliza el identificador único $\text{id}$ como desempate determinista ($\min \text{id}$). Los demás conejos mueren.

---

## 3. Dinámica de los Zorros (Carnívoros)

1. **Prioridad Alimenticia:**
   - Primero busca celdas adyacentes ocupadas por conejos ($\text{Estado} = \text{RABBIT}$).
   - Si hay $P_{\text{rabbits}} > 0$ conejos adyacentes, selecciona uno mediante $(G+X+Y) \bmod P_{\text{rabbits}}$, lo depreda, se desplaza a dicha celda y reinicia su hambre: $\text{food\_age} = 0$.
2. **Inanición (Muerte por Hambre):**
   - Si no hay conejos disponibles en la vecindad, el zorro acumula una generación adicional sin alimentarse.
   - Si $\text{food\_age} + 1 \ge \text{GEN\_FOOD\_FOXES}$, el zorro **muere por inanición inmediatamente**, antes de intentar desplazarse a una celda vacía.
3. **Desplazamiento a Celdas Vacías:**
   - Si no murió de hambre, busca celdas adyacentes vacías ($\text{Estado} = \text{EMPTY}$).
   - Si $P_{\text{empty}} > 0$, elige mediante $(G+X+Y) \bmod P_{\text{empty}}$ y su hambre incrementa: $\text{food\_age} = \text{food\_age} + 1$.
   - Si $P_{\text{empty}} = 0$, permanece en su posición y su hambre incrementa.
4. **Reproducción:**
   - Ocurre cuando $\text{proc\_age} \ge \text{GEN\_PROC\_FOXES}$ y el zorro realiza un movimiento.
   - Deja una nueva cría en su celda previa con $\text{proc\_age} = 0$ y $\text{food\_age} = 0$.
5. **Resolución de Conflictos:**
   Si $k \ge 2$ zorros intentan ocupar la misma celda:
   1. Mayor edad de reproducción ($\max \text{proc\_age}$).
   2. Menor hambre ($\min \text{food\_age}$).
   3. Menor identificador ($\min \text{id}$).

---

## 4. Estrategia de Paralelismo y Memoria Compartida (OpenMP)

### 4.1 Fases Desacopladas con Barreras Síncronas

Para respetar la simultaneidad de los movimientos y evitar condiciones de carrera, cada generación se divide en fases ordenadas:

```
[ Generación G ]
       │
       ▼
┌──────────────────────────────────────────────┐
│ Fase 1: Propuestas de Movimiento de Conejos  │  (Paralelo OMP)
└──────────────────────────────────────────────┘
       │ [Barrera Implícita OMP]
       ▼
┌──────────────────────────────────────────────┐
│ Fase 2: Resolución Lock-Free de Conflictos   │  (Paralelo OMP + CAS)
│         y Nacimiento de Conejos              │
└──────────────────────────────────────────────┘
       │ [Sincronización de Retículo]
       ▼
┌──────────────────────────────────────────────┐
│ Fase 3: Propuestas de Zorros, Caza e Inanición│ (Paralelo OMP)
└──────────────────────────────────────────────┘
       │ [Barrera Implícita OMP]
       ▼
┌──────────────────────────────────────────────┐
│ Fase 4: Resolución Lock-Free de Zorros,      │  (Paralelo OMP + CAS)
│         Alimentación y Nacimiento            │
└──────────────────────────────────────────────┘
       │
       ▼
┌──────────────────────────────────────────────┐
│ Fase 5: Compactación Determinista            │  (O(N) Compactación)
└──────────────────────────────────────────────┘
```

### 4.2 Resolución de Conflictos Sin Bloqueos (Lock-Free con CAS)

En lugar de utilizar secciones críticas (`#pragma omp critical`) o cerrojos (`omp_lock_t`), que destruyen el rendimiento al generar cuellos de botella por contención, se implementó un mecanismo **Lock-Free** basado en `std::atomic<uint64_t>` y la instrucción hardware `compare_exchange_weak` (CAS).

#### Empaquetamiento de Clave para Conejos (64 bits):
$$\text{Key}_{\text{Rabbit}} = (\text{proc\_age} \ll 32) \mid (2^{32} - 1 - \text{id})$$
- Al maximizar este entero de 64 bits sin signo, el valor más grande corresponde estrictamente al conejo con mayor edad de reproducción y, en caso de empate, al de menor ID.

#### Empaquetamiento de Clave para Zorros (64 bits):
$$\text{Key}_{\text{Fox}} = (\text{proc\_age} \ll 48) \mid ((2^{16} - 1 - \text{food\_age}) \ll 32) \mid (2^{32} - 1 - \text{id})$$
- Campo 1 (Bits 63..48): `proc_age` (mayor es mejor).
- Campo 2 (Bits 47..32): Complemento de `food_age` (menor hambre produce un valor numérico mayor).
- Campo 3 (Bits 31..0): Complemento de `id` (desempate determinista).

#### Bucle de Actualización Atómica:
```cpp
uint64_t current = target_best[cell_idx].load(std::memory_order_relaxed);
while (new_key > current && 
       !target_best[cell_idx].compare_exchange_weak(
           current, new_key, std::memory_order_release, std::memory_order_relaxed)) {
    // Reintento sin bloqueo hasta converger al máximo matemático global
}
```

**Propiedades formales:**
1. **Libre de interbloqueos (Deadlock-Free):** Ningún hilo espera recursos bloqueados.
2. **Determinismo Absoluto:** Al existir un orden total estricto entre candidatos, el estado final de cada celda converge siempre al mismo ganador, independientemente del orden de ejecución de los hilos.
3. **Escalabilidad Óptima:** Múltiples hilos que escriben en celdas distintas no compiten en absoluto en la memoria caché.

---

## 5. Análisis de Sobrecarga y Ley de Amdahl

- En simulaciones pequeñas (ej. $5 \times 5$ con 9 objetos), el tiempo total de cómputo por generación es del orden de microsegundos ($\approx 5\ \mu\text{s}$). La creación y sincronización del equipo de hilos OpenMP introduce una sobrecarga que puede superar el costo del cómputo secuencial, resultando en un speedup menor a 1.
- En ecosistemas densos y medianos a grandes (ej. $250 \times 250$ con más de 20,000 animales), la carga aritmética y de acceso a memoria domina ampliamente, alcanzando un paralelismo real con aceleración sustancial.

---

## 6. Arquitectura de la Interfaz Web y Conexión con el Motor C++

La interfaz web situada en `visualization/index.html` fue diseñada como una herramienta visual y pedagógica para explorar el comportamiento del ecosistema.

### 6.1 Desacoplamiento Arquitectónico (Frontend en Cliente)
- **Motor en JavaScript**: Para garantizar portabilidad total sin requerir la instalación ni configuración de servidores web (como Node.js, Python o servidores HTTP compilados en C++), la lógica del autómata celular fue replicada en JavaScript (`visualization/app.js`).
- **No ejecución directa de OpenMP**: La interfaz web en el navegador ejecuta código JavaScript monohilo en el motor V8/SpiderMonkey del cliente. **No invoca directamente hilos OpenMP ni el binario `ecosystem_sim.exe` en tiempo de ejecución web**, a menos que se cargue una traza previamente generada.

### 6.2 Interoperabilidad y Validación Cruzada
- **Consistencia Matemática**: El motor JavaScript implementa de forma bit a bit las mismas reglas de vecindad de von Neumann, prioridad horaria, inanición antes de movimiento y orden de conflictos que el motor C++.
- **Importación de Trazas C++**: El binario C++ soporta el flag `--export-json <ruta>`, que serializa las estadísticas de población y tiempos de cómputo de cada generación. El visualizador web permite cargar dicho archivo JSON para graficar de manera fiel la evolución calculada por el motor OpenMP.
