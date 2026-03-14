# Changelog: Integrazione Modello MATLAB — Cambiamento Variabili

**Data:** Marzo 2026
**Commit di integrazione:** da09395 (feat: Initialize F-16 6-DOF aerodynamic data and constants)
**Commit struttura:** 884d1dd (refactor: Update FBW_Types for F-16 6-DOF model)

---

## 1. Struttura `PilotInput` — Input del Pilota

### PRIMA (Modello Semplificato)
```cpp
struct PilotInput {
  float stick_roll;      // [-1.0, +1.0]: comanda roll rate
  float stick_pitch;     // [-1.0, +1.0]: comanda pitch rate
  float rudder;          // [-1.0, +1.0]: pedale timone
  float speed_delta;     // [KPH/frame] — Comando velocità scalare
  bool engines_on;
  bool engine_ready;
  bool landing_mode;
  bool gear_deploy;
};
```

### DOPO (Integrazione MATLAB / NASA TP-1538)
```cpp
struct PilotInput {
  float stick_roll;       // [-1.0, +1.0]: input cloche asse rollio (comando p)
  float stick_pitch;      // [-1.0, +1.0]: input cloche asse beccheggio (comando q)
  float rudder;           // [-1.0, +1.0]: pedale timone (comando imbardata r)
  float throttle_input;   // [0.0, 1.0]: comando manetta FISICA (spinta) [NASA TP-1538]
  bool engines_on;
  bool engine_ready;
  bool landing_mode;
  bool gear_deploy;
};
```

**Cambiamenti principali:**
- ❌ Rimosso: `speed_delta` (velocità scalare non-fisico)
- ✅ Aggiunto: `throttle_input` (comando fisico di spinta, convertito a Newton dal modello motore)
- 📝 Commenti aggiornati per riferimento NASA TP-1538

---

## 2. Struttura `ControlSurfaces` — Superfici di Controllo F-16

### PRIMA (Generico)
```cpp
struct ControlSurfaces {
  float aileron_deflection;   // [deg] ±25°
  float elevator_deflection;  // [deg] ±30°
  float rudder_deflection;    // [deg] ±30°
  float thrust_normalized;    // [0.0, 1.0]
};
```

### DOPO (Specifico F-16 NASA TP-1538)
```cpp
struct ControlSurfaces {
  float stabilator_deflection;  // [deg] δh — Stabilatore orizzontale intero
  float flaperon_deflection;    // [deg] δa — Flaperoni (alettone + flap)
  float rudder_deflection;      // [deg] δr — Timone verticale
  float leading_edge_flap;      // [deg] δlef — Flap di bordo d'attacco
  float thrust_normalized;      // [0.0, 1.0]
};
```

**Cambiamenti principali:**
- ❌ Rinominato: `aileron_deflection` → `flaperon_deflection` (F-16 ha alettone-flap integrato)
- ❌ Rinominato: `elevator_deflection` → `stabilator_deflection` (F-16 ha stabilatore, non ascensore separato)
- ✅ Aggiunto: `leading_edge_flap` (comando LEF, specifico F-16)
- 📝 Nomi ora corrispondono alla nomenclatura NASA TP-1538

---

## 3. Struttura `FlightState` — Stato di Volo

### PRIMA (Minimalista)
```cpp
struct FlightState {
  // Assetto
  float roll, pitch, yaw;          // [rad]

  // Tassi angolari
  float roll_rate, pitch_rate, yaw_rate;  // [rad/s]

  // Navigazione
  float x, z;                 // [m]
  float altitude;             // [m]
  float speed;                // [KPH] — Scalare generico

  // Stato sistema
  FBWMode mode;
  ProtectionStatus protections;
  bool system_active;
  bool landing_mode;
};
```

### DOPO (6-DOF Completo / NASA TP-1538)
```cpp
struct FlightState {
  // Atteggiamento (Euler, rad)
  float roll;    // Φ (Phi)
  float pitch;   // Θ (Theta)
  float yaw;     // Ψ (Psi)

  // Tassi angolari (rad/s) nel body frame
  float roll_rate;   // p
  float pitch_rate;  // q
  float yaw_rate;    // r

  // Velocità lineari nel body frame (m/s) — 6-DOF!
  float u;  // asse X (avanti)
  float v;  // asse Y (destra)
  float w;  // asse Z (basso)

  // Angoli aerodinamici (rad) — calcolati da u, v, w
  float alpha;  // Angolo di Attacco (α)
  float beta;   // Angolo di Derapata (β)

  // Navigazione
  float x, z;      // [m] posizione earth frame
  float altitude;  // [m]

  // Stato sistema
  FBWMode mode;
  ProtectionStatus protections;
  bool system_active;
  bool landing_mode;
};
```

**Cambiamenti principali:**

| Variabile     | Prima | Dopo |  Motivo |
|-----------    |-------|------|--------        |
| `speed` (KPH) | ✅    | ❌   | Rimosso — sostituito da u, v, w 6-DOF |
| `u, v, w`     | ❌    | ✅   | **AGGIUNTO** — Velocità nel body frame per 6-DOF |
| `alpha, beta` | ❌    | ✅   | *AGGIUNTO** — Angoli aerodinamici NASA TP-1538 |

| Commenti | Generici | Specifici F-16 | Allineamento MATLAB/NASA |

---

## 4. Nuove Strutture AGGIUNTE

### 4.1 Struttura Interna: Actuator Dynamics
```cpp
// NEW in FlightControlComputer.hpp (dopo integrazione MATLAB)
struct ActuatorState {
  float ele;   // [deg] stabilatore attuale
  float ail;   // [deg] flaperon attuale
  float rud;   // [deg] timone attuale
  float lef;   // [deg] LEF attuale
  float thr;   // [0,1] throttle attuale
};
```
**Perché:** Modellare la dinamica degli attuatori (rate-limited first-order lag) come nel MATLAB

---

### 4.2 Costanti F-16 (da load_F16_params.m)
```cpp
// NEW in FlightControlComputer.hpp (dopo integrazione MATLAB)
static constexpr float MASS_KG = 636.94f * SLUG2KG;          // 9298.6 kg
static constexpr float I_XX = 9496.0f  * SLUG_FT2_TO_KG_M2;  // 12874 kg*m^2
static constexpr float I_YY = 55814.0f * SLUG_FT2_TO_KG_M2;  // 75674 kg*m^2
static constexpr float I_ZZ = 63100.0f * SLUG_FT2_TO_KG_M2;  // 85552 kg*m^2
static constexpr float I_XZ = 982.0f   * SLUG_FT2_TO_KG_M2;  // 1331  kg*m^2 ← IMPORTANTE!
static constexpr float S_WING = 300.0f * FT2M * FT2M;        // 27.87 m^2
static constexpr float B_SPAN = 30.0f * FT2M;                // 9.144 m
static constexpr float C_BAR  = 11.32f * FT2M;               // 3.45 m
```

**Perché:** Necessarie per le equazioni del moto 6-DOF (Newton-Eulero)

---

## 5. Nuove Costanti Rate Limits (da trim_and_linearize.m)
```cpp
// NEW in FlightControlComputer.hpp
static constexpr float ACT_RATE_ELE = 60.0f;   // [deg/s]
static constexpr float ACT_RATE_AIL = 80.0f;   // [deg/s]
static constexpr float ACT_RATE_RUD = 120.0f;  // [deg/s]
static constexpr float ACT_RATE_LEF = 25.0f;   // [deg/s]
static constexpr float ACT_RATE_THR = 0.556f;  // [1/s]
```

**Provenienza:** MATLAB `trim_and_linearize.m` — Rispetto attuatori realistici F-16

---

## 6. Nuovi File Aggiunti al Progetto

| File | Provenienza | Funzione |
|------|-----------|----------|
| `F16AeroData.hpp` | Auto-generato da F16AeroData.h5 | 44 lookup tables + interpolatori 1D/2D/3D |
| `prof.Russo/F16-Model-Matlab/` | MATLAB originale prof. Russo | Reference implementation + parametri |

---

## 7. Riepilogo Impatto su Strutture Dati

### Dimensione Struct `FlightState`

**PRIMA:**
```
roll, pitch, yaw (3 × 4 byte)       = 12 byte
roll_rate, pitch_rate, yaw_rate     = 12 byte
x, z, altitude, speed               = 16 byte
mode (1 byte), protections (4 byte) = 5 byte
system_active, landing_mode         = 2 byte
TOTALE ≈ 47 byte (+ padding)
```

**DOPO:**
```
roll, pitch, yaw (3 × 4 byte)                    = 12 byte
roll_rate, pitch_rate, yaw_rate                  = 12 byte
u, v, w                                          = 12 byte ← NEW
alpha, beta                                      = 8 byte  ← NEW
x, z, altitude                                   = 12 byte (rimosso speed)
mode (1 byte), protections (4 byte)              = 5 byte
system_active, landing_mode                      = 2 byte
TOTALE ≈ 63 byte (+ padding)
```

**Crescita:** +16 byte (34% più grande) — accettabile per una simulazione fisica completa

---

## 8. Compatibilità DDS / Telemetria

**PRIMA:**
- Telemetria: `speed` [KPH], assetto [rad], tassi angolari [rad/s]

**DOPO:**
- Telemetria: `u, v, w` [m/s], `alpha, beta` [rad], assetto [rad], tassi angolari [rad/s]
- **Impatto:** DDS `Telemetry.hpp` deve essere aggiornato per includere i nuovi campi

---

## 9. Implicazioni sul Codice Principale

### FlightControlComputer.cpp
- ✅ **Lines 264–315:** Nuove equazioni 6-DOF (Stevens & Lewis) gestiscono (u, v, w) e (p, q, r)
- ✅ **Lines 220–221:** Actuator model ora usa rate limits specifici F-16
- ✅ **Lines 76–144:** F16AeroFM completo con lookup tables MATLAB

### FlightControlLaw.cpp/hpp
- ⚠️ **Necessario update:** SAS inner loop deve rispondere a (p, q, r) — OK, già fatto
- ⚠️ **Necessario update:** Outer PID loop deve usare assetto Eulero — struttura ready, valori ancora placeholder

### FlightDisplay.cpp
- ⚠️ **Necessario update:** HUD deve visualizzare alpha/beta e velocità componenti — non ancora fatto

---

## 10. Conclusione

**Impatto di integrazione MATLAB:**
- ✅ Modello fisico da **3-DOF scalare** → **6-DOF vettoriale**
- ✅ Parametri da **arbitrari** → **Esatti dalla NASA TP-1538**
- ✅ Superfici controllo da **generiche** → **Specifiche F-16**
- ⚠️ Structs e costanti aggiornati; alcuni moduli (HUD, telemetry) necessitano sincronizzazione

**File corrisponden affidabili:**
- `FBW_Types.hpp` — 2026-03, commit 884d1dd ✓
- `FlightControlComputer.hpp/cpp` — 2026-03, commit da09395 ✓
- `F16AeroData.hpp` — auto-generato da .h5 ✓
- `FlightControlLaw.hpp/cpp` — già aggiornato, commit 8ff110f ✓
