// Ecosystem Simulation Web Engine & UI Controller
// Exact replication of the C++ / PDF rules

class EcosystemSim {
    constructor() {
        this.genProcRabbits = 2;
        this.genProcFoxes = 4;
        this.genFoodFoxes = 3;
        this.nGen = 6;
        this.rows = 5;
        this.cols = 5;

        this.currentGen = 0;
        this.nextId = 1;
        this.rocks = []; // [{r, c}]
        this.rabbits = []; // [{id, r, c, procAge, alive}]
        this.foxes = []; // [{id, r, c, procAge, foodAge, alive}]

        this.history = []; // [{gen, rabbits, foxes}]
    }

    resetFromData(config, objects) {
        this.genProcRabbits = config.genProcRabbits;
        this.genProcFoxes = config.genProcFoxes;
        this.genFoodFoxes = config.genFoodFoxes;
        this.nGen = config.nGen;
        this.rows = config.rows;
        this.cols = config.cols;

        this.currentGen = 0;
        this.nextId = 1;
        this.rocks = [];
        this.rabbits = [];
        this.foxes = [];
        this.history = [];

        for (const obj of objects) {
            if (obj.type === "ROCK") {
                this.rocks.push({ r: obj.r, c: obj.c });
            } else if (obj.type === "RABBIT") {
                this.rabbits.push({ id: this.nextId++, r: obj.r, c: obj.c, procAge: 0, alive: true });
            } else if (obj.type === "FOX") {
                this.foxes.push({ id: this.nextId++, r: obj.r, c: obj.c, procAge: 0, foodAge: 0, alive: true });
            }
        }

        this.recordHistory();
    }

    recordHistory() {
        this.history.push({
            gen: this.currentGen,
            rabbits: this.rabbits.filter(b => b.alive).length,
            foxes: this.foxes.filter(f => f.alive).length,
            rocks: this.rocks.length
        });
    }

    inBounds(r, c) {
        return r >= 0 && r < this.rows && c >= 0 && c < this.cols;
    }

    step() {
        if (this.currentGen >= this.nGen) return false;

        const G = this.currentGen;
        const DIRS = [
            { dr: -1, dc: 0 }, // 0: North
            { dr: 0, dc: 1 },  // 1: East
            { dr: 1, dc: 0 },  // 2: South
            { dr: 0, dc: -1 }  // 3: West
        ];

        // 1. Grid of current occupancy
        const grid = Array.from({ length: this.rows }, () => Array(this.cols).fill("EMPTY"));
        for (const rk of this.rocks) grid[rk.r][rk.c] = "ROCK";
        for (const b of this.rabbits) if (b.alive) grid[b.r][b.c] = "RABBIT";
        for (const f of this.foxes) if (f.alive) grid[f.r][f.c] = "FOX";

        // ==========================================
        // PHASE 1 & 2: RABBITS
        // ==========================================
        const rabbitProposals = new Map(); // targetKey -> []

        for (const b of this.rabbits) {
            if (!b.alive) continue;

            const possibleEmpty = [];
            for (const d of DIRS) {
                const nr = b.r + d.dr;
                const nc = b.c + d.dc;
                if (this.inBounds(nr, nc) && grid[nr][nc] === "EMPTY") {
                    possibleEmpty.push({ r: nr, c: nc });
                }
            }

            const P = possibleEmpty.length;
            let targetR = b.r;
            let targetC = b.c;
            let moved = false;

            if (P > 0) {
                const idx = ((G + b.r + b.c) % P + P) % P;
                targetR = possibleEmpty[idx].r;
                targetC = possibleEmpty[idx].c;
                moved = true;
            }

            const key = `${targetR},${targetC}`;
            if (!rabbitProposals.has(key)) rabbitProposals.set(key, []);
            rabbitProposals.get(key).push({
                rabbit: b,
                moved,
                oldR: b.r,
                oldC: b.c,
                targetR,
                targetC
            });
        }

        const babyRabbits = [];
        for (const [key, group] of rabbitProposals.entries()) {
            // Sort by procAge descending, then id ascending
            group.sort((a, b) => {
                if (b.rabbit.procAge !== a.rabbit.procAge) return b.rabbit.procAge - a.rabbit.procAge;
                return a.rabbit.id - b.rabbit.id;
            });

            const winner = group[0];
            winner.rabbit.r = winner.targetR;
            winner.rabbit.c = winner.targetC;

            if (winner.moved && winner.rabbit.procAge >= this.genProcRabbits) {
                winner.rabbit.procAge = 0;
                babyRabbits.push({
                    id: this.nextId++,
                    r: winner.oldR,
                    c: winner.oldC,
                    procAge: 0,
                    alive: true
                });
            } else {
                winner.rabbit.procAge++;
            }

            // Conflicts: all others die
            for (let i = 1; i < group.length; ++i) {
                group[i].rabbit.alive = false;
            }
        }

        // Update grid for foxes
        for (let r = 0; r < this.rows; ++r) {
            for (let c = 0; c < this.cols; ++c) {
                if (grid[r][c] === "RABBIT") grid[r][c] = "EMPTY";
            }
        }
        for (const b of this.rabbits) if (b.alive) grid[b.r][b.c] = "RABBIT";
        for (const baby of babyRabbits) grid[baby.r][baby.c] = "RABBIT";

        // ==========================================
        // PHASE 3 & 4: FOXES
        // ==========================================
        const foxProposals = new Map();

        for (const f of this.foxes) {
            if (!f.alive) continue;

            const possibleRabbits = [];
            for (const d of DIRS) {
                const nr = f.r + d.dr;
                const nc = f.c + d.dc;
                if (this.inBounds(nr, nc) && grid[nr][nc] === "RABBIT") {
                    possibleRabbits.push({ r: nr, c: nc });
                }
            }

            if (possibleRabbits.length > 0) {
                const P = possibleRabbits.length;
                const idx = ((G + f.r + f.c) % P + P) % P;
                const targetR = possibleRabbits[idx].r;
                const targetC = possibleRabbits[idx].c;

                const key = `${targetR},${targetC}`;
                if (!foxProposals.has(key)) foxProposals.set(key, []);
                foxProposals.get(key).push({
                    fox: f,
                    moved: true,
                    ate: true,
                    oldR: f.r,
                    oldC: f.c,
                    targetR,
                    targetC
                });
            } else {
                // Starvation check
                if (f.foodAge + 1 >= this.genFoodFoxes) {
                    f.alive = false;
                    continue;
                }

                const possibleEmpty = [];
                for (const d of DIRS) {
                    const nr = f.r + d.dr;
                    const nc = f.c + d.dc;
                    if (this.inBounds(nr, nc) && grid[nr][nc] === "EMPTY") {
                        possibleEmpty.push({ r: nr, c: nc });
                    }
                }

                const P = possibleEmpty.length;
                let targetR = f.r;
                let targetC = f.c;
                let moved = false;

                if (P > 0) {
                    const idx = ((G + f.r + f.c) % P + P) % P;
                    targetR = possibleEmpty[idx].r;
                    targetC = possibleEmpty[idx].c;
                    moved = true;
                }

                const key = `${targetR},${targetC}`;
                if (!foxProposals.has(key)) foxProposals.set(key, []);
                foxProposals.get(key).push({
                    fox: f,
                    moved,
                    ate: false,
                    oldR: f.r,
                    oldC: f.c,
                    targetR,
                    targetC
                });
            }
        }

        const babyFoxes = [];
        const eatenCoords = new Set();

        for (const [key, group] of foxProposals.entries()) {
            // Sort: procAge DESC, foodAge ASC, id ASC
            group.sort((a, b) => {
                if (b.fox.procAge !== a.fox.procAge) return b.fox.procAge - a.fox.procAge;
                if (a.fox.foodAge !== b.fox.foodAge) return a.fox.foodAge - b.fox.foodAge;
                return a.fox.id - b.fox.id;
            });

            const winner = group[0];
            winner.fox.r = winner.targetR;
            winner.fox.c = winner.targetC;

            if (winner.ate) {
                winner.fox.foodAge = 0;
                eatenCoords.add(`${winner.targetR},${winner.targetC}`);
            } else {
                winner.fox.foodAge++;
            }

            if (winner.moved && winner.fox.procAge >= this.genProcFoxes) {
                winner.fox.procAge = 0;
                babyFoxes.push({
                    id: this.nextId++,
                    r: winner.oldR,
                    c: winner.oldC,
                    procAge: 0,
                    foodAge: 0,
                    alive: true
                });
            } else {
                winner.fox.procAge++;
            }

            // Conflicts: other foxes die
            for (let i = 1; i < group.length; ++i) {
                group[i].fox.alive = false;
            }
        }

        // Kill eaten rabbits
        for (const b of this.rabbits) {
            if (b.alive && eatenCoords.has(`${b.r},${b.c}`)) b.alive = false;
        }
        for (const baby of babyRabbits) {
            if (baby.alive && eatenCoords.has(`${baby.r},${baby.c}`)) baby.alive = false;
        }

        // Compaction
        this.rabbits = this.rabbits.filter(b => b.alive).concat(babyRabbits.filter(b => b.alive));
        this.foxes = this.foxes.filter(f => f.alive).concat(babyFoxes.filter(f => f.alive));

        this.currentGen++;
        this.recordHistory();
        return true;
    }
}

// UI Controller
document.addEventListener("DOMContentLoaded", () => {
    const sim = new EcosystemSim();
    let isPlaying = false;
    let playTimer = null;

    // DOM Elements
    const canvas = document.getElementById("ecosystemCanvas");
    const ctx = canvas.getContext("2d");

    const genDisplay = document.getElementById("genDisplay");
    const rabbitDisplay = document.getElementById("rabbitDisplay");
    const foxDisplay = document.getElementById("foxDisplay");
    const rockDisplay = document.getElementById("rockDisplay");
    const dimsDisplay = document.getElementById("dimsDisplay");

    const btnPlay = document.getElementById("btnPlay");
    const btnStep = document.getElementById("btnStep");
    const btnReset = document.getElementById("btnReset");
    const speedRange = document.getElementById("speedRange");
    const speedVal = document.getElementById("speedVal");

    const pProcRabbits = document.getElementById("pProcRabbits");
    const pProcFoxes = document.getElementById("pProcFoxes");
    const pFoodFoxes = document.getElementById("pFoodFoxes");
    const pNGen = document.getElementById("pNGen");
    const pRows = document.getElementById("pRows");
    const pCols = document.getElementById("pCols");

    const btnPresetProf = document.getElementById("btnPresetProf");
    const btnPresetBalance = document.getElementById("btnPresetBalance");
    const btnPresetOverpop = document.getElementById("btnPresetOverpop");

    const ioTextarea = document.getElementById("ioTextarea");
    const fileInput = document.getElementById("fileInput");
    const btnApplyText = document.getElementById("btnApplyText");
    const btnExportOutput = document.getElementById("btnExportOutput");

    // Initialize Chart.js
    const chartCtx = document.getElementById("populationChart").getContext("2d");
    const popChart = new Chart(chartCtx, {
        type: "line",
        data: {
            labels: [0],
            datasets: [
                {
                    label: "Conejos (🐰)",
                    data: [0],
                    borderColor: "#10b981",
                    backgroundColor: "rgba(16, 185, 129, 0.15)",
                    tension: 0.2,
                    fill: true
                },
                {
                    label: "Zorros (🦊)",
                    data: [0],
                    borderColor: "#f97316",
                    backgroundColor: "rgba(249, 115, 22, 0.15)",
                    tension: 0.2,
                    fill: true
                }
            ]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
                legend: { labels: { color: "#cbd5e1" } }
            },
            scales: {
                x: { ticks: { color: "#94a3b8" }, grid: { color: "#334155" } },
                y: { ticks: { color: "#94a3b8" }, grid: { color: "#334155" }, beginAtZero: true }
            }
        }
    });

    function updateChart() {
        popChart.data.labels = sim.history.map(h => h.gen);
        popChart.data.datasets[0].data = sim.history.map(h => h.rabbits);
        popChart.data.datasets[1].data = sim.history.map(h => h.foxes);
        popChart.update();
    }

    // Default Teacher Example
    const PROFESSOR_EXAMPLE = `2 4 3 6 5 5 9
ROCK 0 0
RABBIT 0 2
FOX 0 4
FOX 1 0
FOX 1 4
ROCK 2 4
RABBIT 3 0
RABBIT 4 0
FOX 4 4`;

    function loadFromText(text) {
        const trimmed = text.trim();
        if (trimmed.length < 1) return;

        // Check if loading a JSON metrics file exported by C++ engine
        if (trimmed.startsWith("{")) {
            try {
                const data = JSON.parse(trimmed);
                if (data.history && Array.isArray(data.history)) {
                    sim.history = data.history.map(h => ({
                        gen: h.generation,
                        rabbits: h.rabbit_count,
                        foxes: h.fox_count,
                        rocks: h.rock_count
                    }));
                    pause();
                    updateChart();
                    alert(`¡Métricas de simulación C++ cargadas exitosamente!\nModo: ${data.mode}\nHilos: ${data.threads}\nTiempo C++: ${data.total_time_ms.toFixed(2)} ms\nGeneraciones: ${data.history.length}`);
                    return;
                }
            } catch (err) {
                console.error("Error al analizar JSON de métricas:", err);
            }
        }

        const lines = trimmed.split("\n").map(l => l.trim()).filter(l => l.length > 0);
        if (lines.length < 1) return;

        const p = lines[0].split(/\s+/).map(Number);
        const config = {
            genProcRabbits: p[0],
            genProcFoxes: p[1],
            genFoodFoxes: p[2],
            nGen: p[3],
            rows: p[4],
            cols: p[5],
            nObjects: p[6]
        };

        const objects = [];
        for (let i = 1; i < lines.length; ++i) {
            const parts = lines[i].split(/\s+/);
            if (parts.length >= 3) {
                objects.push({
                    type: parts[0],
                    r: parseInt(parts[1], 10),
                    c: parseInt(parts[2], 10)
                });
            }
        }

        // Sync inputs
        pProcRabbits.value = config.genProcRabbits;
        pProcFoxes.value = config.genProcFoxes;
        pFoodFoxes.value = config.genFoodFoxes;
        pNGen.value = config.nGen;
        pRows.value = config.rows;
        pCols.value = config.cols;

        sim.resetFromData(config, objects);
        pause();
        updateUI();
    }

    function updateUI() {
        genDisplay.innerHTML = `Generación: <strong>${sim.currentGen} / ${sim.nGen}</strong>`;
        rabbitDisplay.innerHTML = `Conejos: <strong>${sim.rabbits.filter(b => b.alive).length}</strong>`;
        foxDisplay.innerHTML = `Zorros: <strong>${sim.foxes.filter(f => f.alive).length}</strong>`;
        rockDisplay.innerHTML = `Rocas: <strong>${sim.rocks.length}</strong>`;
        dimsDisplay.innerText = `${sim.rows} x ${sim.cols}`;

        renderCanvas();
        updateChart();
    }

    function renderCanvas() {
        const maxW = 560;
        const maxH = 560;
        const cellSize = Math.min(Math.floor(maxW / sim.cols), Math.floor(maxH / sim.rows), 64);
        
        canvas.width = sim.cols * cellSize;
        canvas.height = sim.rows * cellSize;

        // Background
        ctx.fillStyle = "#0f172a";
        ctx.fillRect(0, 0, canvas.width, canvas.height);

        // Grid lines
        ctx.strokeStyle = "#1e293b";
        ctx.lineWidth = 1;
        for (let r = 0; r <= sim.rows; ++r) {
            ctx.beginPath();
            ctx.moveTo(0, r * cellSize);
            ctx.lineTo(canvas.width, r * cellSize);
            ctx.stroke();
        }
        for (let c = 0; c <= sim.cols; ++c) {
            ctx.beginPath();
            ctx.moveTo(c * cellSize, 0);
            ctx.lineTo(c * cellSize, canvas.height);
            ctx.stroke();
        }

        // Render Rocks
        for (const rk of sim.rocks) {
            const x = rk.c * cellSize;
            const y = rk.r * cellSize;
            ctx.fillStyle = "#475569";
            ctx.fillRect(x + 2, y + 2, cellSize - 4, cellSize - 4);
            ctx.font = `${Math.floor(cellSize * 0.55)}px sans-serif`;
            ctx.textAlign = "center";
            ctx.textBaseline = "middle";
            ctx.fillText("🪨", x + cellSize / 2, y + cellSize / 2);
        }

        // Render Rabbits
        for (const b of sim.rabbits) {
            if (!b.alive) continue;
            const x = b.c * cellSize;
            const y = b.r * cellSize;
            ctx.fillStyle = "rgba(16, 185, 129, 0.2)";
            ctx.fillRect(x + 2, y + 2, cellSize - 4, cellSize - 4);
            ctx.font = `${Math.floor(cellSize * 0.58)}px sans-serif`;
            ctx.textAlign = "center";
            ctx.textBaseline = "middle";
            ctx.fillText("🐰", x + cellSize / 2, y + cellSize / 2);

            // Procreation badge
            ctx.font = `bold ${Math.max(10, Math.floor(cellSize * 0.22))}px sans-serif`;
            ctx.fillStyle = "#34d399";
            ctx.fillText(b.procAge, x + cellSize * 0.8, y + cellSize * 0.25);
        }

        // Render Foxes
        for (const f of sim.foxes) {
            if (!f.alive) continue;
            const x = f.c * cellSize;
            const y = f.r * cellSize;
            ctx.fillStyle = "rgba(249, 115, 22, 0.25)";
            ctx.fillRect(x + 2, y + 2, cellSize - 4, cellSize - 4);
            ctx.font = `${Math.floor(cellSize * 0.58)}px sans-serif`;
            ctx.textAlign = "center";
            ctx.textBaseline = "middle";
            ctx.fillText("🦊", x + cellSize / 2, y + cellSize / 2);

            // Food age / proc badge
            ctx.font = `bold ${Math.max(10, Math.floor(cellSize * 0.22))}px sans-serif`;
            ctx.fillStyle = "#fb923c";
            ctx.fillText(`${f.procAge}/${f.foodAge}`, x + cellSize * 0.75, y + cellSize * 0.25);
        }
    }

    function play() {
        if (sim.currentGen >= sim.nGen) return;
        isPlaying = true;
        btnPlay.innerText = "⏸ Pausar";
        btnPlay.classList.replace("btn-primary", "btn-warning");

        const delay = parseInt(speedRange.value, 10);
        playTimer = setInterval(() => {
            const hasMore = sim.step();
            updateUI();
            if (!hasMore || sim.currentGen >= sim.nGen) {
                pause();
            }
        }, delay);
    }

    function pause() {
        isPlaying = false;
        clearInterval(playTimer);
        btnPlay.innerText = "▶ Reproducir";
        btnPlay.classList.replace("btn-warning", "btn-primary");
    }

    btnPlay.addEventListener("click", () => {
        if (isPlaying) pause();
        else play();
    });

    btnStep.addEventListener("click", () => {
        pause();
        sim.step();
        updateUI();
    });

    btnReset.addEventListener("click", () => {
        pause();
        loadFromText(ioTextarea.value || PROFESSOR_EXAMPLE);
    });

    speedRange.addEventListener("input", () => {
        speedVal.innerText = `${speedRange.value} ms`;
        if (isPlaying) {
            pause();
            play();
        }
    });

    // Presets
    btnPresetProf.addEventListener("click", () => {
        ioTextarea.value = PROFESSOR_EXAMPLE;
        loadFromText(PROFESSOR_EXAMPLE);
    });

    btnPresetBalance.addEventListener("click", () => {
        // Generate a 20x20 balanced ecosystem
        const R = 20, C = 20;
        const total = 50;
        let txt = `3 5 4 40 ${R} ${C} ${total}\n`;
        for (let i = 0; i < 10; ++i) txt += `ROCK ${i * 2} ${i * 2}\n`;
        for (let i = 0; i < 25; ++i) txt += `RABBIT ${(i * 3 + 1) % R} ${(i * 7 + 2) % C}\n`;
        for (let i = 0; i < 15; ++i) txt += `FOX ${(i * 5 + 4) % R} ${(i * 11 + 3) % C}\n`;
        ioTextarea.value = txt;
        loadFromText(txt);
    });

    btnPresetOverpop.addEventListener("click", () => {
        const R = 15, C = 15;
        let txt = `2 6 3 30 ${R} ${C} 35\n`;
        for (let i = 0; i < 5; ++i) txt += `ROCK ${i * 3} ${i * 3}\n`;
        for (let i = 0; i < 28; ++i) txt += `RABBIT ${(i * 2) % R} ${(i * 3 + 1) % C}\n`;
        for (let i = 0; i < 2; ++i) txt += `FOX ${(i * 7 + 3) % R} ${(i * 5 + 2) % C}\n`;
        ioTextarea.value = txt;
        loadFromText(txt);
    });

    btnApplyText.addEventListener("click", () => {
        loadFromText(ioTextarea.value);
    });

    btnExportOutput.addEventListener("click", () => {
        // Output format matching professor specification
        const objs = [];
        for (const rk of sim.rocks) objs.push({ type: "ROCK", r: rk.r, c: rk.c });
        for (const b of sim.rabbits) if (b.alive) objs.push({ type: "RABBIT", r: b.r, c: b.c });
        for (const f of sim.foxes) if (f.alive) objs.push({ type: "FOX", r: f.r, c: f.c });

        objs.sort((a, b) => (a.r !== b.r ? a.r - b.r : a.c - b.c));

        let out = `${sim.genProcRabbits} ${sim.genProcFoxes} ${sim.genFoodFoxes} 0 ${sim.rows} ${sim.cols} ${objs.length}\n`;
        for (const o of objs) {
            out += `${o.type} ${o.r} ${o.c}\n`;
        }
        ioTextarea.value = out;
        alert("¡Estado del ecosistema exportado al cuadro de texto inferior en el formato oficial del profesor!");
    });

    fileInput.addEventListener("change", (e) => {
        const file = e.target.files[0];
        if (!file) return;
        const reader = new FileReader();
        reader.onload = (evt) => {
            ioTextarea.value = evt.target.result;
            loadFromText(evt.target.result);
        };
        reader.readAsText(file);
    });

    // Start with Professor Example
    ioTextarea.value = PROFESSOR_EXAMPLE;
    loadFromText(PROFESSOR_EXAMPLE);
});
