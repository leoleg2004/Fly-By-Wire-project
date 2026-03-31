# Flight Sensor Simulator — Project Instructions

## Project Context
This is an F-16 flight simulator project (6-DOF, NASA TP-1538 aero model).
Key references:
- **Stevens & Lewis** textbook — equations of motion, stability derivatives
- **NASA TP-1538** — aerodynamic coefficient tables
- **MATLAB/Simulink** implementations in `prof.Russo/F16-Model-Matlab/`

When modifying flight physics or equations of motion, always verify against these references.

## Development Environment
- **Python**: use system Python (`python3`), NOT virtual environments, unless explicitly asked
- **C++ build**: `cd build && cmake --build .` — binary: `build/FlightSim`
- **OS**: Linux (Ubuntu)
- Always verify imports work before presenting Python solutions

## Workflow Conventions

### Editing Rules
- When editing Markdown documents, make targeted section edits rather than rewriting entire files
- For large documents (> 200 lines), edit ONE section at a time to avoid token/API limits
- Prefer `Edit` tool over `Write` for existing files

### File Locations
- Save generated files to the project directory or user-specified path
- Never save to `.claude/` or auto-memory folders unless explicitly asked
- Output images (plots, charts) go to the same directory as the input data

### Code Quality
- When modifying flight physics, run dimensional consistency checks
- Cross-reference aero coefficients against F16AeroData.h5 breakpoints
- For the real-time scheduler visualizer: all statistics must be 100% CSV-derived, period/deadline from matched C source only

## Key Directories
- `build/` — CMake build directory
- `prof.Russo/F16-Model-Matlab/` — MATLAB reference implementation
- `RMSenzaDDSapp09/Brugali/src/app/parser/` — Real-time scheduler trace parser & visualizer
- `src/` — Main simulator C++ sources
