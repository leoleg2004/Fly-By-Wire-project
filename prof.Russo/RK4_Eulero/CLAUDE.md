# F-16 RK4_Eulero Project — Claude Code Guidelines

## Project Overview

This is a **Python F-16 6-DOF flight simulator** demonstrating numerical integration methods (Eulero Esplicito, Eulero Implicito, RK2, RK4) applied to nonlinear aerospace dynamics with inertial coupling.

**Key references:**
- Stevens & Lewis, "Aircraft Control and Simulation", 3rd ed.
- NASA TP-1538 (F-16 aerodynamic model)
- MATLAB reference implementations in `prof.Russo/F16-Model-Matlab/`

The simulator is verified against MATLAB trajectories and integrated with **Eclipse/PyDev** for interactive visualization via **vpython**.

---

## Python Environment

### Interpreter
- **Primary**: System Python (`/usr/bin/python3`, v3.12)
- **For Eclipse**: Configure PyDev to use "Default" interpreter (system Python)
- **Do NOT assume venv is active** — code must work with system Python

### Dependencies
All required packages are installed system-wide:
```bash
pip install numpy scipy vpython "setuptools<81"
```

To validate, run:
```bash
python3 scripts/validate_env.py
```

### Import Guards
All optional imports **must** have try/except guards to prevent PyDev test discovery from crashing:

```python
try:
    from vpython import canvas, graph, gcurve, color, rate
except ImportError:
    # Fallback for test discovery
    canvas = graph = gcurve = color = rate = None
```

---

## File Structure and Paths

```
RK4_Eulero/
├── simulazione/
│   └── py/
│       └── Simulazione.py          ← Main script (Eclipse Run entry point)
├── scripts/
│   ├── validate_env.py             ← Environment checker
│   └── setup_project.sh            ← Bash setup (if needed)
├── .claude/
│   ├── settings.json               ← Claude Code hooks & config
│   └── skills/
│       └── expand-section/SKILL.md ← Custom skill template
├── .project                        ← Eclipse project metadata
├── .pydevproject                   ← PyDev interpreter config
└── CLAUDE.md                       ← This file
```

### Path Conventions
- **Always use absolute paths** or paths relative to `${PROJECT_ROOT}`
- **No hardcoded `/home/leonardo/` paths** — use relative paths
- **Before writing any new file**, state the exact path and wait for user confirmation
- **Never write to `.claude/memory/` or auto-memory folders** — that's reserved for persistent session notes

---

## Eclipse/PyDev Configuration

### Running from Eclipse
1. Right-click `simulazione/py/Simulazione.py`
2. Click "Run As" → "Python Run"
3. vpython opens in browser automatically

### Interpreter Setup
- **Project → Properties → PyDev - Interpreter/Grammar**
- Select **"Default"** (system `/usr/bin/python3`)
- Do NOT use venv unless explicitly activated in bash

### Known Issues & Solutions
| Issue | Cause | Fix |
|-------|-------|-----|
| `ModuleNotFoundError: scipy` | PyDev using different Python | Run `python3 scripts/validate_env.py` to check interpreter |
| vpython import fails in test discovery | Missing import guard | Wrap `from vpython import ...` in try/except |
| File not found in Eclipse | Relative path wrong | Use absolute path from project root |

---

## Code Standards

### Numerical Integration
The system integrates **6 state equations** (u, v, w, p, q, r) with 4 methods:
- **Eulero Esplicito**: `state += dt * f(state)`
- **Eulero Implicito**: Fixed-point iteration with try/except for scipy optional dependency
- **RK2 (Heun)**: `(k1, k2)` two-stage
- **RK4 Classico**: `(k1, k2, k3, k4)` four-stage, Butcher tableau

**Aero frozen**: Forces/moments computed once per step (not updated within substages)

### State Equations (Stevens & Lewis)
With zero external forces (Fx=Fy=Fz=L=M=N=0):

$$\dot{u} = rv - qw$$
$$\dot{v} = pw - ru$$
$$\dot{w} = qu - pv$$
$$\dot{p} = \frac{I_{zz}L_{eff} + I_{xz}N_{eff}}{\Gamma}$$
$$\dot{q} = \frac{M - (I_{xx}-I_{zz})pr - I_{xz}(p^2-r^2)}{I_{yy}}$$
$$\dot{r} = \frac{I_{xx}N_{eff} + I_{xz}L_{eff}}{\Gamma}$$

where $\Gamma = I_{xx}I_{zz} - I_{xz}^2$ and $L_{eff}$, $N_{eff}$ contain Coriolis/gyroscopic coupling.

### Reference Solution
- **Method**: RK4 with dt = dt_nominal / 1000 (ultra-high precision)
- **Output**: Black curve on all state plots for visual comparison
- **Exact solution NOT analytical** — system is nonlinear and chaotic

### Visualization
- **3D Model**: F-16 rendered via vpython, rotated by quaternion from (p, q, r)
- **Graphs**: 6 state plots (u, v, w, p, q, r) + 1 KE drift plot
- **Colors**:
  - Black = Reference (exact)
  - Red = Eulero Esplicito
  - Orange = Eulero Implicito
  - Green = RK2
  - Cyan = RK4

---

## Documentation

### Markdown Files
- **Expand one section at a time** to avoid API limits
- Use **Edit tool** for incremental changes, not full file rewrites
- Include Mermaid diagrams for architecture/flowcharts
- Cross-reference **Stevens & Lewis**, **NASA TP-1538**, **MATLAB**

### Custom Skill: `/expand-section`
When expanding a doc section, provide:
```
Section: "RK4 Integration"
Content: "Add detailed derivation, Butcher tableau, and local truncation error analysis."
```

---

## Testing & Validation

### Quick Validation
```bash
python3 scripts/validate_env.py
```

### Run Simulator
```bash
python3 simulazione/py/Simulazione.py
```

Premi "Run" nel browser per avviare l'integrazione numerica.

---

## Common Tasks

### Task: Add a new method or modify equations
1. Edit `derivatives()` function in `Simulazione.py`
2. Verify against Stevens & Lewis equations
3. Run `python3 Simulazione.py` to test visually
4. Confirm reference (black) curve changes accordingly

### Task: Change initial conditions
Edit `IC` array in `Simulazione.py`:
```python
IC = np.array([500.0, 0.0, 0.0,    # u, v, w  [ft/s]
               2.0,   0.5, 0.0])   # p, q, r  [rad/s]
```

### Task: Adjust simulation time
Edit `T_END` in `Simulazione.py`:
```python
T_END = 55.0  # seconds
```

---

## API Limits & Efficiency

**Problem**: Half your prior sessions failed due to API rate limits on large file writes.

**Solution**:
- Edit **one section at a time** using the Edit tool
- Keep each prompt under 200 lines of target content
- Use the `/expand-section` custom skill for docs
- When stuck on API limits, space out requests (wait 5-10 min)

---

## References

- **Simulator**: `simulazione/py/Simulazione.py`
- **MATLAB Source**: `../F16-Model-Matlab/`
- **Memory/Notes**: `.claude/projects/-home-leonardo-eprosima-projects-flight-sensor-flight-sensor/memory/`
- **Validation Script**: `scripts/validate_env.py`
- **Settings**: `.claude/settings.json`

---

**Last Updated**: 2026-03-15
**Python**: 3.12.3 | **vpython**: 7.6.5 | **scipy**: 1.17.1
