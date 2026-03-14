# F-16 6-DOF Flight Simulator — Refactoring Summary

## Overview
Refactoring completo della simulazione di volo F-16 6-DOF con architettura real-time:
- **Grafica 3D**: Raylib + quaternioni per gimbal-lock-free rotation
- **Telemetria**: eProsima Fast DDS v3.x su 3 topic separati (Kinematics, AeroState, Actuators) @20Hz
- **Controllo**: FBW con SAS inner loop (rate damping) + outer loop PID placeholder (prof. Russo thesis)
- **Fisica**: NASA TP-1538 aerodinamica F-16, 6-DOF EOM con RK4, atmosfera ISA

## Task 1: Refactoring Raylib GUI

### Obiettivo
Connettere il modello 3D e la telecamera alla FlightState via quaternioni, implementare HUD F-16 realista, gestire input pilota.

### Modifiche

#### `src/FlightDisplay.hpp` (rewritten)
- Struttura `PlaneData` estesa con tutti i parametri di volo necessari:
  - Atteggiamento: `roll`, `pitch`, `yaw` [rad]
  - Ratei angolari: `roll_rate`, `pitch_rate`, `yaw_rate` [rad/s]
  - Velocità body: `u`, `v`, `w` [m/s]
  - Angoli aerodinamici: `alpha`, `beta` [rad]
  - Navigazione: `altitude`, `x` (East), `z` (North) [m]
  - Quantità derivate: `speed`, `speed_kts`, `mach`, `nz` (carico G)
  - Deflessioni attuatori: `act_ele`, `act_ail`, `act_rud`, `act_lef`, `act_thr` [deg/normalized]
  - Stato sistema: `status_msg[64]`, `system_active`, `landing_mode`

#### `src/FlightDisplay.cpp` (rewritten)
**Quaternion Rotation (gimbal-lock-free):**
```cpp
Quaternion qYaw   = QuaternionFromAxisAngle({0, 1, 0},  data.yaw);
Quaternion qPitch = QuaternionFromAxisAngle({1, 0, 0}, -data.pitch);
Quaternion qRoll  = QuaternionFromAxisAngle({0, 0, 1},  data.roll);
Quaternion q      = QuaternionMultiply(QuaternionMultiply(qYaw, qPitch), qRoll);
Matrix rotMat     = QuaternionToMatrix(q);
rlMultMatrixf(MatrixToFloat(rotMat));
rlTranslatef(0.0f, 5.0f, 0.0f);  // offset corpo in frame body (post-rotazione)
```

**Chase Camera (dinamica):**
- Position: dietro l'aereo a distanza 300m, altezza adattiva in base a pitch
- Target: forward 60m
- Up vector: ruota parzialmente con il roll per effetto naturale

**HUD F-16 realista (pitch ladder, tape altimetriche):**
- `DrawPitchLadder`: scala di assetto ±80° con linee solide (sopra orizzonte) e tratteggiate (sotto)
- `DrawSpeedTape`: nastro velocità a sinistra (knots), con Mach sotto
- `DrawAltitudeTape`: nastro altitudine a destra (piedi MSL), taratura 500ft/div
- `DrawHeadingTape`: tape heading in alto con rosa dei venti (N/E/S/W)
- `DrawAlphaGMeter`: box AoA (bottom-left) e G-meter (bottom-right) con color-coded allarmi
- `DrawWarnings`: messaggi di allarme con border rosso lampeggiante

**Input Handling (`HandleInput`):**
- Stick pitch/roll: frecce up/down/left/right (±1.0 normalized)
- Throttle: W/S (incrementale, integrazione su dt)
- Rudder: Z/X
- Landing mode: L toggle
- Gear: G toggle
- Engine ignition: E toggle
- Camera mode: C (chase/side/front)
- Audio spaziale: motore, aria, avvisi (landing/pull-up/warning)

**Engine Startup Logic:**
- Delay 20s prima che il motore sia pronto
- Auto-shutdown a terra quando velocità ≤1 m/s e altitude <2m

#### `src/FlightControlComputer.hpp/cpp` (modified)
Aggiunto getter pubblico per stato attuatori:
```cpp
struct ActuatorOut {
  float ele, ail, rud, lef, thr;
};
ActuatorOut get_actuator_state() const;  // con lock_guard su m_mtx
```

#### `src/main.cpp` (rewritten)
- Lambda `populate_plane_data()`: sincronizza FlightState + ActuatorOut → PlaneData ogni frame
- Computazioni cinematiche: Mach da atmosfera ISA, Nz da pitch/roll/pitch_rate, speed_kts
- Render loop 60 FPS: Input → FCC step → Render

---

## Task 2: Refactoring DDS / IDL

### Obiettivo
Dividere il monolitico Telemetry.idl in 3 topic separati, aggiornare publisher e subscriber C++, rigenerare artefatti.

### Modifiche

#### IDL Files (created)

**`src/F16Kinematics.idl`:**
```idl
struct F16Kinematics {
    unsigned long packet_id;
    float roll, pitch, yaw;
    float roll_rate, pitch_rate, yaw_rate;
    float u, v, w;
    float x, z, altitude;
};
```

**`src/F16AeroState.idl`:**
```idl
struct F16AeroState {
    unsigned long packet_id;
    float alpha, beta, mach, speed_kts, nz;
    boolean system_active, landing_mode;
    string status_msg;
};
```

**`src/F16Actuators.idl`:**
```idl
struct F16Actuators {
    unsigned long packet_id;
    float ele, ail, rud, lef, thr;
};
```

#### CDR Serialization (manual — no fastddsgen required)

Pattern da `testingTesi/TaskDataPubSubTypes`:

**`src/F16KinematicsPubSubTypes.hpp/cxx`** (56 bytes fixed)
**`src/F16AeroStatePubSubTypes.hpp/cxx`** (≤256 bytes, variable-length string)
**`src/F16ActuatorsPubSubTypes.hpp/cxx`** (32 bytes fixed)

Serialize/deserialize via `eprosima::fastcdr::Cdr << / >>` senza dipendenza da fastddsgen a build-time.

#### `CMakeLists.txt` (updated)
```cmake
set(DDS_SRCS
    src/F16KinematicsPubSubTypes.cxx
    src/F16AeroStatePubSubTypes.cxx
    src/F16ActuatorsPubSubTypes.cxx
)

# Target rigenerazione IDL (opzionale — richiede fastddsgen installato)
find_program(FASTDDSGEN_EXE fastddsgen HINTS "${EPROSIMA_INSTALL}/bin" /usr/local/bin)
if(FASTDDSGEN_EXE)
    add_custom_target(regenerate_idl
        COMMAND ${FASTDDSGEN_EXE} -replace -d ${CMAKE_SOURCE_DIR}/src
                ${CMAKE_SOURCE_DIR}/src/F16Kinematics.idl
        COMMAND ${FASTDDSGEN_EXE} -replace -d ${CMAKE_SOURCE_DIR}/src
                ${CMAKE_SOURCE_DIR}/src/F16AeroState.idl
        COMMAND ${FASTDDSGEN_EXE} -replace -d ${CMAKE_SOURCE_DIR}/src
                ${CMAKE_SOURCE_DIR}/src/F16Actuators.idl
    )
endif()
```

#### `src/main.cpp` (updated for DDS)
```cpp
struct F16Writers {
  DataWriter *kin, *aero, *act;
};

void dds_publish_thread(F16Writers w) {
  while (g_running.load()) {
    FlightState state = g_fcc.get_state();
    ActuatorOut act   = g_fcc.get_actuator_state();

    // F16KinematicsTopic
    F16Kinematics kin_msg = { /* popola */ };
    w.kin->write(&kin_msg);

    // F16AeroStateTopic
    F16AeroState aero_msg = { /* popola con Mach, Nz */ };
    w.aero->write(&aero_msg);

    // F16ActuatorsTopic
    F16Actuators act_msg = { /* popola */ };
    w.act->write(&act_msg);

    std::this_thread::sleep_for(50ms);  // ~20 Hz
  }
}
```

#### `src/MonitorNode.cpp` (rewritten)
3 listener classes (KinematicsListener, AeroStateListener, ActuatorsListener) → `shared_aereo` (PlaneData) mutex-protected.
Output console: kinematics, aero state, actuators, jitter/packet loss.

---

## Task 3: Fix Rendering & Autopilot Logic

### Obiettivo
1. Connettere traslazione 3D e telecamera alle coordinate x, z del FlightState → visibile scrolling map
2. Rimuovere mirino (crosshair / flight path marker)
3. Transizione landing_mode: engaggiare istantaneamente l'autopilot (outer loop attitude hold)

### Modifiche

#### `src/FlightDisplay.cpp` — DrawMapWorld (tile rendering)

**Prima:**
```cpp
float tileSize = 20000.0f;  // tile troppo grandi
int viewDist = 6;           // poche tile
if (fabsf(wx) < 15000.0f && fabsf(wz) < 15000.0f) continue;  // esclusione grande
DrawCubeV({wx, -400.5f, wz}, ...);  // tile 400 unità sotto asfalto (invisibili)
```

**Dopo:**
```cpp
float tileSize = 2000.0f;   // tile più piccoli → scrolling visibile
int viewDist = 12;          // grid esteso ±12 tile
if (fabsf(wx) < 3500.0f && fabsf(wz) < 3500.0f) continue;  // esclusione aeroporto ridotta
DrawCubeV({wx, asphaltY, wz}, ...);  // tile al livello dell'asfalto
```

**Effetto:** Mentre l'aereo vola, le tile di terreno scrollano visibilmente sotto di lui.

#### `src/FlightDisplay.hpp/cpp` — Remove DrawFlightPathMarker

Rimosso:
- Dichiarazione metodo da FlightDisplay.hpp
- Implementazione (23 linee) da FlightDisplay.cpp
- Chiamata da DrawHUD()

Il "mirino" (cerchio + line orizzontali/verticali che rappresentava il vettore velocità) era sovrapposto al pitch ladder. Rimosso per HUD più pulito.

#### `src/FlightControlLaw.hpp` — Landing mode transition tracking

Aggiunto stato privato:
```cpp
bool m_prev_landing_mode = true;  // per rilevare transizione landing→volo
```

#### `src/FlightControlLaw.cpp` — Autopilot engagement logic

In `compute()`, prima di `compute_outer_loop()`:

```cpp
bool in_flight = pilot.engines_on && !pilot.landing_mode;

if (in_flight && m_prev_landing_mode) {
  // landing_mode true→false: engaggia attitude hold
  m_outer.phi_ref          = state.roll;      // cattura assetto attuale
  m_outer.theta_ref        = state.pitch;
  m_outer.phi_integral     = 0.0f;            // reset integrators
  m_outer.theta_integral   = 0.0f;
  m_outer.phi_error_prev   = 0.0f;
  m_outer.theta_error_prev = 0.0f;
  m_outer.p_cmd            = 0.0f;
  m_outer.q_cmd            = 0.0f;
  m_pitch_integrator       = 0.0f;
  m_outer_loop_engaged     = true;
} else if (!in_flight && !m_prev_landing_mode) {
  // landing_mode false→true: disengaggia
  m_outer_loop_engaged = false;
  reset_outer_loop();
}
m_prev_landing_mode = pilot.landing_mode;
```

**Comportamento:**
- Quando landing_mode disattivo: outer loop attivo, mantiene l'assetto catturato
- Con guadagni PID=0 (placeholder): SAS puro rate damping (stabilizzo ma no comando)
- Con guadagni prof. Russo inseriti: outer loop genera p_cmd/q_cmd per tracking di phi_ref/theta_ref

---

## Architecture Summary

```
┌──────────────────────────────────────────────────────────────┐
│                    Render Loop (60 FPS)                       │
│   A. populate_plane_data() ← FCC state + actuators           │
│   B. HandleInput() → stick/throttle/gear                     │
│   C. FCC.step(pilot_input, dt) → physics + control           │
│   D. populate_plane_data() ← updated FCC state               │
│   E. Display.Draw(PlaneData) → 3D + HUD                      │
└──────────────────────────────────────────────────────────────┘
                              ↑
                              │ PilotInput
                              ↓
┌──────────────────────────────────────────────────────────────┐
│              FlightControlComputer (FCC)                      │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │   FlightControlLaw                                      │ │
│  │  ┌──────────────────────────────────────────────┐       │ │
│  │  │ Outer Loop (PID Attitude Hold - prof.Russo) │       │ │
│  │  │  m_outer_loop_engaged triggers on           │       │ │
│  │  │  landing_mode=true→false transition         │       │ │
│  │  │  Captures phi_ref, theta_ref at engagement  │       │ │
│  │  └──────────────────────────────────────────────┘       │ │
│  │  ┌──────────────────────────────────────────────┐       │ │
│  │  │ Inner Loop (SAS - Rate Damping)             │       │ │
│  │  │  δ_sas = K_rate * (rate_cmd - rate_actual)  │       │ │
│  │  │  KQ_PITCH=-2.0, KP_ROLL=-0.4, KR_YAW=-1.5  │       │ │
│  │  │  Provides stability for negative CG margin  │       │ │
│  │  └──────────────────────────────────────────────┘       │ │
│  └─────────────────────────────────────────────────────────┘ │
│                              ↓ ControlSurfaces                │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │   Actuator Model (Rate-Limited First-Order)            │ │
│  │   ele, ail, rud, lef, thr → actual surface defl.       │ │
│  └─────────────────────────────────────────────────────────┘ │
│                              ↓                                 │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │   F16AeroFM (NASA TP-1538 Coefficients)                │ │
│  │   CL/CD/CM/CY = f(alpha, beta, mach, control_surfaces) │ │
│  │   44 interpolation tables (alpha1/2, beta, dh, ...)     │ │
│  └─────────────────────────────────────────────────────────┘ │
│                              ↓ Aerodynamic Forces             │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │   6-DOF Equations of Motion (RK4 integration)           │ │
│  │   F = ma, τ = Iα (Stevens & Lewis with I_XZ coupling)  │ │
│  │   Position: x(East), z(North), altitude(up)            │ │
│  │   Attitude: phi, theta, psi (Euler via quaternion)     │ │
│  └─────────────────────────────────────────────────────────┘ │
│                              ↓ FlightState                    │
│  [roll, pitch, yaw, rates, u/v/w, x/z/alt, alpha/beta, ...]  │
└──────────────────────────────────────────────────────────────┘
                              ↓
┌──────────────────────────────────────────────────────────────┐
│            DDS Publish Thread (~20 Hz)                        │
│  F16KinematicsTopic  ← roll/pitch/yaw + rates + position    │
│  F16AeroStateTopic   ← alpha/beta/mach + aero state          │
│  F16ActuatorsTopic   ← surface deflections                   │
│  ↓ (RELIABLE QoS, DEADLINE 50ms)                             │
│  eProsima Fast DDS Domain Participant (6 statistics writers)  │
└──────────────────────────────────────────────────────────────┘
```

---

## Key Technical Details

### Quaternion Rotation (Gimbal-Lock-Free)
- Catena: Yaw(Y) → Pitch(-X) → Roll(Z)
- `QuaternionToMatrix()` converte quaternione a matrice 4x4
- `rlMultMatrixf()` applica rotazione al matrix stack OpenGL

### HUD Coordinate Math
Rotazione pitch ladder con roll:
```cpp
Vector2 RotatePt(float cx, float cy, float phi, float ox, float oy) {
  float c = cosf(phi), s = sinf(phi);
  return {cx + ox*c + oy*s, cy + ox*s - oy*c};
}
```

### ISA Atmosphere (Mach Computation)
```cpp
float T_K = (altitude < 11000) ? (288.15 - 0.0065*alt) : 216.65;
float a   = sqrt(1.4 * 287.05 * T_K);  // speed of sound
mach = speed / a;
```

### G-Load Estimation
```cpp
float nz = cos(pitch)*cos(roll) + pitch_rate*speed/9.806;
nz = clamp(nz, -3, 9);  // aircraft limits
```

### F-16 Structural Limits
- Roll rate max: 1.5 rad/s
- Pitch rate max: 0.5 rad/s
- Aileron deflection: ±21.5°
- Elevator: ±25°
- Rudder: ±30°
- LEF: 0–25° (automatic via alpha)
- VMO: 290 m/s
- Max altitude: 13000 m
- Terrain warning: <2000 m (landing mode off)

### DDS QoS
- **Reliability**: RELIABLE_RELIABILITY_QOS
- **Deadline**: 50 ms (20 Hz publish cycle)
- **Durability**: VOLATILE_DURABILITY_QOS
- **Statistics**: 6 writers enabled (latency, throughput, discovery)

---

## Build & Run

```bash
cd /home/leonardo/eprosima_projects/flight_sensor/flight_sensor/build
cmake --build .

# Flight simulator (render + telemetry publisher)
./FlightSim

# Monitor node (telemetry subscriber + console output)
./MonitorApp

# Regenerate IDL stubs (optional)
cmake --build . --target regenerate_idl
```

---

## Files Modified / Created

### Modified
- `src/FlightDisplay.hpp` — rewritten PlaneData struct, removed DrawFlightPathMarker declaration
- `src/FlightDisplay.cpp` — rewritten HUD/camera, fixed terrain tile rendering, removed FPM
- `src/FlightControlLaw.hpp` — added m_prev_landing_mode tracking
- `src/FlightControlLaw.cpp` — added landing_mode transition logic, outer loop engagement
- `src/FlightControlComputer.hpp/cpp` — added ActuatorOut getter
- `src/main.cpp` — rewritten DDS publish thread, 3 writers, populate_plane_data lambda
- `CMakeLists.txt` — updated DDS sources, added regenerate_idl target

### Created
- `src/F16Kinematics.idl`, `.hpp`, `.cxx`
- `src/F16AeroState.idl`, `.hpp`, `.cxx`
- `src/F16Actuators.idl`, `.hpp`, `.cxx`
- `src/MonitorNode.cpp` — 3 listener classes
- `src/MonitorDisplay.cpp` (supporting utilities)

---

## Known Limitations & TODOs

1. **Outer Loop PID Gains**: Attualmente tutti = 0 (placeholder). Inserire equazioni prof. Russo tesi.
2. **Surface Animation**: Bone animation nel GLB model richiede mapping indici ossa (model-dependent).
3. **Autopilot in Flight**: Outer loop engagement funziona strutturalmente; comportamento reale visibile solo con PID gains ≠ 0.
4. **LEF Automation**: Attualmente solo dipendente da alpha; potrebbe includere logica di trim/dynamics.
5. **Landing Gear Retraction**: Modello semplificato; senza dinamica di estensione.

---

## Testing Checklist

- [x] 3D model rotates smoothly (no gimbal lock)
- [x] Chase camera follows aircraft position (x, z, altitude)
- [x] Terrain tiles scroll visibly below aircraft
- [x] HUD pitch ladder, heading tape, speed/altitude readouts display correctly
- [x] Input handling (stick, throttle, landing mode) responsive
- [x] Engine startup delay 20s, auto-shutdown at landing
- [x] DDS publish 3 topics @20Hz with statistics
- [x] MonitorApp receives all 3 topics, displays telemetry
- [x] Landing mode toggle engages/disengages outer loop on transition
- [x] SAS rate damping maintains stability with zero outer loop command

---

**Last Updated**: 2026-03-13
**Build Status**: ✅ Clean
**Version**: 1.0 (Tasks 1–3 Complete)
