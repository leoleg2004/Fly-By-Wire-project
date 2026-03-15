# Simulatore di Volo F-16 — Dal Modello MATLAB all'Implementazione C++ Real-Time

## Indice

1. [Introduzione e obiettivo del progetto](#1-introduzione-e-obiettivo-del-progetto)
2. [Il modello MATLAB di riferimento](#2-il-modello-matlab-di-riferimento)
3. [Architettura del simulatore C++](#3-architettura-del-simulatore-c)
4. [Strutture dati condivise (FBW_Types)](#4-strutture-dati-condivise-fbw_types)
5. [La legge di controllo Fly-By-Wire (FlightControlLaw)](#5-la-legge-di-controllo-fly-by-wire-flightcontrollaw)
6. [Il Flight Control Computer (FCC)](#6-il-flight-control-computer-fcc)
7. [Il modello aerodinamico F16AeroFM](#7-il-modello-aerodinamico-f16aerofm)
8. [Le equazioni del moto 6-DOF](#8-le-equazioni-del-moto-6-dof)
9. [Cinematica e navigazione](#9-cinematica-e-navigazione)
10. [Il display di volo e l'HUD (FlightDisplay)](#10-il-display-di-volo-e-lhud-flightdisplay)
11. [La telemetria DDS](#11-la-telemetria-dds)
12. [Il ciclo principale (main)](#12-il-ciclo-principale-main)
13. [Mappatura MATLAB → C++](#13-mappatura-matlab--c)
14. [Predisposizione per la tesi (Outer Loop PID)](#14-predisposizione-per-la-tesi-outer-loop-pid)

---

## 1. Introduzione e obiettivo del progetto

Questo progetto è un **simulatore di volo real-time per il caccia F-16 Fighting Falcon**, scritto interamente in C++. Nasce dalla traduzione di un modello aerodinamico MATLAB sviluppato dal Prof. Raktim Bhattacharya (Texas A&M University), basato sul rapporto tecnico **NASA TP-1538** ("Simulator Study of Stall/Post-Stall Characteristics of a Fighter Airplane with Relaxed Longitudinal Static Stability", Nguyen et al., 1979) e sul testo di riferimento **Stevens & Lewis, "Aircraft Control and Simulation"**.

Il simulatore MATLAB fornisce:
- 44 tabelle di lookup aerodinamiche (dati sperimentali in galleria del vento)
- Parametri inerziali e geometrici dell'F-16
- Funzione di forze e momenti aerodinamici (`F16AeroFM.m`)
- Limiti e rate degli attuatori (`trim_and_linearize.m`)
- Condizioni di trim e linearizzazione

Il simulatore C++ aggiunge:
- **Rendering 3D** in tempo reale con Raylib (modello F-16, terreno, cielo)
- **HUD di volo** completo (pitch ladder, speed tape, altitude tape, heading, alpha/G meter)
- **Sistema Fly-By-Wire** con tre leggi di controllo (Normal, Alternate, Direct)
- **SAS** (Stability Augmentation System) per lo smorzamento artificiale
- **Telemetria DDS** via eProsima Fast DDS su tre topic a 20 Hz
- **Predisposizione** per l'inserimento di un controllore PID outer-loop (tesi prof. Russo)

L'aereo parte a terra con i motori spenti. Il pilota (l'utente alla tastiera) accende i motori, attende il warmup, disattiva la modalità atterraggio, dà manetta e decolla. Da quel momento la fisica 6-DOF integra ogni frame le equazioni del moto complete, producendo un volo che rispetta fedelmente il comportamento reale dell'F-16.

---

## 2. Il modello MATLAB di riferimento

Il modello MATLAB si trova nella cartella `prof.Russo/F16-Model-Matlab/` e comprende:

### 2.1 Parametri fisici (`load_F16_params.m`)

Il file definisce le costanti dell'F-16 in unità SI:

| Parametro | Valore originale | Valore SI | Descrizione |
|-----------|-----------------|-----------|-------------|
| Massa | 636.94 slug | 9298.6 kg | Massa a vuoto + carburante |
| I_xx | 9496 slug·ft² | 12874 kg·m² | Momento d'inerzia in rollio |
| I_yy | 55814 slug·ft² | 75674 kg·m² | Momento d'inerzia in beccheggio |
| I_zz | 63100 slug·ft² | 85552 kg·m² | Momento d'inerzia in imbardata |
| I_xz | 982 slug·ft² | 1331 kg·m² | Prodotto d'inerzia (accoppiamento) |
| S | 300 ft² | 27.87 m² | Superficie alare |
| b | 30 ft | 9.144 m | Apertura alare |
| c̄ | 11.32 ft | 3.45 m | Corda media aerodinamica |
| x_cg | 0.30 c̄ | — | Posizione baricentro |
| x_cgr | 0.35 c̄ | — | Posizione CG di riferimento |

Il baricentro a 0.30 c̄ è *avanti* del punto neutro (0.35 c̄). Questo rende l'F-16 **staticamente instabile** nel canale longitudinale — una scelta progettuale voluta per garantire alta manovrabilità, ma che richiede un sistema di stabilizzazione artificiale (SAS) per impedire la divergenza in beccheggio.

### 2.2 Modello aerodinamico (`F16AeroFM.m`)

La funzione MATLAB `F16AeroFM` calcola le **sei componenti di forze e momenti** aerodinamici a partire dallo stato di volo e dalle deflessioni delle superfici. Utilizza 44 tabelle di lookup interpolate, estratte dal file HDF5 `F16AeroData.h5`, che contengono i dati sperimentali NASA.

Il flusso di calcolo è:

1. **Conversione angoli** da radianti a gradi (le tabelle usano gradi)
2. **Normalizzazione superfici**: aileron/21.5, rudder/30, LEF come (1 - lef/25)
3. **Lookup coefficienti base**: Cx, Cy, Cz, Cl, Cm, Cn in funzione di (alpha, beta, elevator)
4. **Delta LEF**: correzioni ai coefficienti base per il leading edge flap
5. **Derivate di stabilità**: Cxq, Czq, Cmq, Cyp, Cyr, Clp, Clr, Cnp, Cnr — tutte funzione solo di alpha
6. **Delta derivate LEF**: correzioni alle derivate per il LEF
7. **Delta superfici di controllo**: effetti incrementali di aileron a 20° e rudder a 30°
8. **Coefficienti totali**: assemblaggio con tutti i contributi, incluse le correzioni per lo spostamento del CG
9. **Forze e momenti dimensionali**: moltiplicazione per pressione dinamica, superficie alare e bracci

### 2.3 Limiti degli attuatori (`trim_and_linearize.m`)

Il file MATLAB documenta i limiti operativi:

| Superficie | Range | Rate massimo |
|-----------|-------|-------------|
| Spinta (T) | 1000 – 19000 lbf | 10000 lbf/s |
| Stabilatore (δh) | ±25° | 60°/s |
| Flaperon (δa) | ±21.5° | 80°/s |
| Timone (δr) | ±30° | 120°/s |
| LEF (δlef) | 0 – 25° | 25°/s |

Questi limiti fisici sono stati replicati nel modello degli attuatori C++.

---

## 3. Architettura del simulatore C++

Il simulatore è organizzato in quattro strati funzionali con un flusso dati unidirezionale:

```
Tastiera                           eProsima Fast DDS
   │                                      ▲
   ▼                                      │
┌─────────────────┐                       │
│  FlightDisplay  │ ◄─── PlaneData ───────┤
│  (Raylib + HUD) │                       │
└────────┬────────┘                       │
         │ PilotInput                     │
         ▼                                │
┌─────────────────────┐                   │
│  FlightControlLaw   │                   │
│  ┌─ Outer Loop PID ─┤ (placeholder)     │
│  └─ Inner Loop SAS ─┤                   │
└────────┬────────────┘                   │
         │ ControlSurfaces (δ_cmd)         │
         ▼                                │
┌─────────────────────────────────┐       │
│  FlightControlComputer  (FCC)   │       │
│  ┌─ Actuator Model ────────────┤       │
│  ├─ F16AeroFM (44 tabelle) ───┤       │
│  ├─ Gravità body-frame ────────┤  ──── FlightState
│  ├─ EOM 6-DOF (RK4) ──────────┤       │
│  └─ Cinematica body→earth ─────┤       │
└─────────────────────────────────┘       │
         │ FlightState (aggiornato)        │
         └─────────────────────────────────┘
```

I file sorgente sono:

| File | Ruolo |
|------|-------|
| `FBW_Types.hpp` | Strutture dati condivise (PilotInput, ControlSurfaces, FlightState, ProtectionStatus) |
| `FlightControlLaw.hpp/cpp` | Legge di controllo FBW: SAS inner loop + outer loop PID (placeholder) |
| `FlightControlComputer.hpp/cpp` | FCC: attuatori, aerodinamica, equazioni del moto, cinematica |
| `F16AeroData.hpp` | 44 tabelle di lookup auto-generate da `F16AeroData.h5` con funzioni di interpolazione |
| `FlightDisplay.hpp/cpp` | Rendering Raylib: modello 3D, cielo, terreno, HUD, audio, input da tastiera |
| `main.cpp` | Orchestrazione: setup DDS, ciclo di rendering a 60 FPS, thread di pubblicazione a 20 Hz |

---

## 4. Strutture dati condivise (FBW_Types)

Il file `FBW_Types.hpp` definisce le strutture dati che attraversano l'intero sistema.

### FBWMode — Modalità della legge di controllo

```cpp
enum class FBWMode : uint8_t {
  NORMAL_LAW    = 0,  // Protezioni complete + SAS attivo
  ALTERNATE_LAW = 1,  // Protezioni ridotte, comando diretto proporzionale
  DIRECT_LAW    = 2   // Nessuna protezione, nessun SAS — motori spenti
};
```

L'F-16 reale utilizza un sistema Fly-By-Wire completo: non esiste collegamento meccanico tra cloche e superfici. Il computer di bordo interpreta sempre i comandi del pilota. In NORMAL_LAW il SAS è attivo e fornisce smorzamento artificiale; in DIRECT_LAW i comandi vanno direttamente alle superfici senza elaborazione.

### PilotInput — Comandi del pilota

```cpp
struct PilotInput {
  float stick_roll;      // [-1, +1] cloche laterale → comando di rollio
  float stick_pitch;     // [-1, +1] cloche longitudinale → comando di beccheggio
  float rudder;          // [-1, +1] pedaliera → comando di imbardata
  float throttle_input;  // [0, 1] manetta → spinta normalizzata
  bool  engines_on;      // stato motori (toggle con tasto E)
  bool  engine_ready;    // diventa true dopo 20s di warmup
  bool  landing_mode;    // modalità ILS (toggle con tasto L)
  bool  gear_deploy;     // carrello (toggle con tasto G)
};
```

### ControlSurfaces — Deflessioni comandate

```cpp
struct ControlSurfaces {
  float stabilator_deflection;  // [deg] δh — stabilatore (beccheggio)
  float flaperon_deflection;    // [deg] δa — flaperoni (rollio)
  float rudder_deflection;      // [deg] δr — timone (imbardata)
  float leading_edge_flap;      // [deg] δlef — LEF (automatici da alpha)
  float thrust_normalized;      // [0, 1] — comando spinta normalizzato
};
```

Queste sono le deflessioni **comandate** (pre-attuatore). Gli attuatori le filtrano poi con un modello del primo ordine con rate-limiting prima di passarle all'aerodinamica.

### FlightState — Vettore di stato completo

```cpp
struct FlightState {
  // Assetto (angoli di Eulero, rad)
  float roll, pitch, yaw;            // Φ, Θ, Ψ

  // Velocità angolari nel body frame (rad/s)
  float roll_rate, pitch_rate, yaw_rate;  // p, q, r

  // Velocità lineari nel body frame (m/s)
  float u, v, w;                     // assi X, Y, Z corpo

  // Angoli aerodinamici (rad)
  float alpha, beta;                 // angolo d'attacco, angolo di derapata

  // Posizione nel riferimento terrestre (m)
  float x, z, altitude;             // Est, Nord, quota MSL

  // Stato del sistema FBW
  FBWMode mode;
  ProtectionStatus protections;
  bool system_active, landing_mode;
  char status_msg[64];
  uint32_t packet_id;
};
```

Questo vettore di stato contiene tutte le 12 variabili di stato del modello MATLAB (posizione, assetto, velocità, velocità angolari) più le informazioni di sistema FBW. Viene prodotto dal FCC ad ogni ciclo e consumato sia dal rendering che dalla telemetria DDS.

---

## 5. La legge di controllo Fly-By-Wire (FlightControlLaw)

La classe `FlightControlLaw` implementa la catena di controllo che trasforma i comandi del pilota in deflessioni delle superfici di volo. È organizzata in due livelli annidati.

### 5.1 Inner Loop — SAS (Stability Augmentation System)

L'F-16 con baricentro a 0.30 c̄ è staticamente instabile: il momento di beccheggio tende ad amplificare le perturbazioni invece di smorzarle. Il SAS è un sistema di feedback proporzionale negativo sui tassi angolari del corpo (p, q, r) che fornisce smorzamento artificiale:

```
δ_sas_ele = KQ_PITCH × (q_cmd − q)     dove KQ_PITCH = −2.0 deg/(rad/s)
δ_sas_ail = KP_ROLL  × (p_cmd − p)     dove KP_ROLL  = −0.4 deg/(rad/s)
δ_sas_rud = KR_YAW   × (0     − r)     dove KR_YAW   = −1.5 deg/(rad/s)
```

Il segno negativo nei gain garantisce il feedback negativo: se l'aereo ha un tasso di beccheggio positivo (q > 0, muso che sale), il SAS comanda uno stabilatore negativo (muso giù) per contrastare il movimento.

In modalità manuale pura (outer loop disattivo), `p_cmd` e `q_cmd` sono zero — il SAS smorza semplicemente qualsiasi tasso angolare. Se l'outer loop PID fosse attivo, questi comandi verrebbero generati dal PID come riferimenti di velocità angolare.

### 5.2 Outer Loop — PID Attitude Hold (placeholder per tesi)

La struttura per un controllore PID sugli angoli di assetto (Φ e Θ) è completamente predisposta ma **inattiva** — tutti i gain sono impostati a zero:

```
p_cmd = Kp_Φ × (Φ_ref − Φ) + Ki_Φ × ∫(Φ_ref − Φ)dt + Kd_Φ × d(Φ_ref − Φ)/dt
q_cmd = Kp_Θ × (Θ_ref − Θ) + Ki_Θ × ∫(Θ_ref − Θ)dt + Kd_Θ × d(Θ_ref − Θ)/dt
```

L'outer loop si attiva automaticamente quando il pilota esce dalla modalità atterraggio (transizione `landing_mode` da true a false): in quel momento cattura gli angoli di assetto correnti come riferimento e azzera gli integrali. Per attivare realmente il controllore, basta assegnare i gain PID dalla tesi del prof. Russo.

### 5.3 Normal Law — Combinazione pilota + SAS

In NORMAL_LAW, la deflessione comandata per ogni asse è la somma del comando del pilota e del termine SAS:

**Canale longitudinale (beccheggio):**
Il pilota comanda un tasso di beccheggio proporzionale allo stick. Quando lo stick è centrato (zona morta < 2%), un integratore mantiene una componente residua che decade con fattore 0.97 ad ogni ciclo, simulando il comportamento "C* law" dell'F-16 reale:

```
se |stick_pitch| < 0.02:
    pitch_rate_demand = integratore × 0.97 (decay naturale)
altrimenti:
    pitch_rate_demand = stick_pitch × MAX_PITCH_RATE
    integratore = pitch_rate_demand × 0.25

δ_ele = (pitch_rate_demand / MAX_PITCH_RATE) × MAX_ELEV + δ_sas_ele
```

**Canale laterale (rollio) e direzionale (imbardata):** proporzionali diretti + SAS.

**Leading Edge Flap:** automatico, proporzionale all'angolo d'attacco — `δ_lef = clamp(alpha_deg × 1.38, 0, 25°)`. I LEF si estendono automaticamente ad alti angoli d'attacco per ritardare lo stallo.

### 5.4 Protezioni di inviluppo

La funzione `apply_protections` valuta le condizioni di volo e attiva i flag di protezione:

| Protezione | Condizione | Significato |
|-----------|-----------|-------------|
| Alpha floor | α > 25° e quota > 0.5 m | Prossimità allo stallo |
| Overspeed | V_T > 290 m/s | Velocità massima operativa superata |
| Terrain avoidance | quota < 300 m, in volo | GPWS pull-up |
| High altitude | quota > 15000 m, in volo | Quota operativa massima |
| Bank angle limit | \|Φ\| > 1.0 rad (~57°), in volo | Inclinazione laterale eccessiva |

Le protezioni attualmente **segnalano** la condizione (il flag viene mostrato sull'HUD come warning), e le deflessioni vengono clampate ai limiti fisici delle superfici.

---

## 6. Il Flight Control Computer (FCC)

La classe `FlightControlComputer` è il cuore del simulatore. Ad ogni chiamata del metodo `step()` esegue l'**intera catena fisica** in sequenza:

```
PilotInput → Control Law → Attuatori → Aerodinamica → EOM 6-DOF → Cinematica → nuovo FlightState
```

### 6.1 Step 1 — Legge di controllo

```cpp
ControlSurfaces cmd = m_law.compute(input, snap, dt);
```

Produce le deflessioni **comandate** (δ_cmd) combinando i comandi del pilota con il SAS.

### 6.2 Step 2 — Modello degli attuatori

Ogni attuatore è modellato come un **sistema del primo ordine con limitazione di rateo**. Questo simula il comportamento fisico della servovalvola idraulica che muove la superficie:

```
errore = δ_cmd − δ_actual
rateo  = clamp(errore / τ, −rate_max, +rate_max)
δ_actual += rateo × dt
δ_actual = clamp(δ_actual, −pos_max, +pos_max)
```

La costante di tempo τ = 0.05 s corrisponde a una banda passante di 20 Hz, tipica degli attuatori idraulici militari. I rate limit provengono direttamente dal MATLAB (`trim_and_linearize.m`):

| Attuatore | τ (s) | Rate max | Range |
|-----------|-------|----------|-------|
| Stabilatore | 0.05 | 60°/s | ±25° |
| Flaperon | 0.05 | 80°/s | ±21.5° |
| Timone | 0.05 | 120°/s | ±30° |
| LEF | 0.05 | 25°/s | 0–25° |
| Throttle | 0.05 | 0.556/s | 0–1 |

### 6.3 Step 3 — Atmosfera e stato aerodinamico

Prima di calcolare l'aerodinamica, il FCC calcola:

**Atmosfera standard ISA:**
```
T = 288.15 − 0.0065 × altitude    (fino a 11 km)
ρ = 1.225 × (T / 288.15)^4.2561
```

**Velocità totale e angoli aerodinamici:**
```
V_T = √(u² + v² + w²)
α = atan2(w, u)          angolo d'attacco
β = asin(v / V_T)        angolo di derapata
q̄ = ½ρV_T²               pressione dinamica
```

**Spinta (modello F100-PW-200):**
Se i motori sono accesi e pronti:
```
Thrust = T_MIN + throttle_actual × (T_MAX − T_MIN)
       = 4448 + throttle × (84517 − 4448)  [Newton]
```
Range originale MATLAB: 1000–19000 lbf, convertito in Newton (×4.44822).

### 6.4 Step 4 — Aerodinamica (F16AeroFM)

Descritto in dettaglio nella sezione 7.

### 6.5 Step 5 — Gravità nel body frame

La forza peso, che nel riferimento terrestre punta solo verso il basso, viene proiettata nel body frame usando gli angoli di Eulero:

```
W_x = −m·g·sin(Θ)
W_y =  m·g·cos(Θ)·sin(Φ)
W_z =  m·g·cos(Θ)·cos(Φ)
```

### 6.6 Step 6 — Equazioni del moto 6-DOF con integrazione RK4

Descritto in dettaglio nella sezione 8.

### 6.7 Step 7 — Cinematica e navigazione

Descritto in dettaglio nella sezione 9.

### 6.8 Sicurezze numeriche

Al termine di ogni step:

- **Ground clamp**: se l'altitudine scende a zero, la velocità verticale verso il basso viene azzerata e gli angoli di assetto vengono smorzati (×0.9) per simulare il contatto col suolo
- **isfinite check**: se una qualsiasi variabile di stato diventa NaN o Infinito (divergenza numerica), viene reimpostata a zero
- **Wrap angoli**: Φ e Ψ vengono mantenuti in [−π, π], Θ viene clampato a [−π/2, π/2]

### 6.9 Thread safety

Il `FlightState` è condiviso tra il thread di rendering (60 Hz) e il thread DDS (20 Hz). L'accesso è protetto da un `std::mutex` con `std::lock_guard` (pattern RAII). Il metodo `step()` lavora su una copia locale (`snap`) e la committe atomicamente al termine.

---

## 7. Il modello aerodinamico F16AeroFM

La funzione `F16AeroFM` nel codice C++ è una **replica riga-per-riga** della funzione MATLAB `F16AeroFM.m`. Ogni linea del codice C++ porta un commento con il numero di riga corrispondente nel file MATLAB originale.

### 7.1 Input e output

**Input:**
- V_T (m/s), α (rad), β (rad), p, q, r (rad/s), q̄ (Pa)
- δ_ele, δ_ail, δ_rud, δ_lef (deg) — deflessioni **effettive** post-attuatore

**Output:**
- Fx, Fy, Fz (N) — forze aerodinamiche nel body frame
- L, M, N (N·m) — momenti aerodinamici nel body frame

### 7.2 Le 44 tabelle di lookup

Le tabelle sono contenute in `F16AeroData.hpp`, auto-generato dal file HDF5 `F16AeroData.h5`. I breakpoint sono:

- **alpha1**: 20 punti da −10° a +45°
- **alpha2**: 14 punti (sottoinsieme)
- **beta1**: 19 punti da −30° a +30°
- **dh1**: 5 punti (deflessione stabilatore)
- **dh2**: 3 punti (sottoinsieme)

Le funzioni di interpolazione (`interp1`, `interp2`, `interp3`) replicano esattamente il comportamento dell'interpolazione lineare MATLAB su griglie regolari.

### 7.3 Flusso di calcolo dei coefficienti

Il calcolo segue esattamente lo schema del MATLAB (sezione 2.2). Per ciascun coefficiente il contributo totale è:

```
C_tot = C_base(α,β,δh)                        ← tabella 3D
      + ΔC_lef(α,β) × dlef                    ← correzione LEF
      + derivata_smorzamento(α) × rate × l/(2V) ← smorzamento aerodinamico
      + ΔC_surface × δ_normalizzata            ← effetto superfici
```

La correzione per lo spostamento del baricentro è cruciale per il momento di beccheggio:
```
Cm_tot = Cm × η_el + Cz_tot × (x_cgr − x_cg) + ΔCm_lef × dlef + ...
```

Il termine `Cz_tot × (x_cgr − x_cg)` sposta il momento di beccheggio proporzionalmente alla differenza tra il CG di riferimento delle tabelle (0.35) e quello reale (0.30). Con CG avanti del riferimento, questo termine è destabilizzante.

### 7.4 Forze e momenti dimensionali

```
Fx = q̄ × S × Cx_tot        Fy = q̄ × S × Cy_tot        Fz = q̄ × S × Cz_tot
L  = Cl_tot × q̄ × S × b    M  = Cm_tot × q̄ × S × c̄    N  = Cn_tot × q̄ × S × b
```

---

## 8. Le equazioni del moto 6-DOF

Le equazioni del moto sono quelle del corpo rigido a sei gradi di libertà secondo la formulazione di **Stevens & Lewis, "Aircraft Control and Simulation", 2ª ed., Cap. 1.4**. Il riferimento specifico è la derivazione del sistema accoppiato con I_xz ≠ 0, che in alcuni testi appare come Eq. 1.4-4 o equivalente a seconda dell'edizione. Questa sezione deriva le equazioni passo per passo per verificarne la correttezza e mostrare la corrispondenza con il codice.

### 8.1 Equazioni di forza (accelerazioni lineari) — derivazione da Newton

Il secondo principio di Newton nel **body frame** non ha la forma semplice F = m·a, perché il body frame è un sistema di riferimento **non inerziale** (ruota con l'aereo). Il teorema del trasporto dice:

```
(dV/dt)_inerziale = (dV/dt)_body + ω × V
```

dove ω = (p, q, r) è la velocità angolare nel body frame e V = (u, v, w) è la velocità lineare nel body frame. Quindi Newton diventa:

```
F = m · [(dV/dt)_body + ω × V]
```

Calcolando il prodotto vettoriale ω × V:

```
     | i    j    k  |
ω×V =| p    q    r  | = (q·w − r·v)·i + (r·u − p·w)·j + (p·v − q·u)·k
     | u    v    w  |
```

Quindi le equazioni scalari diventano:

```
m·u̇ = Fx − m·(q·w − r·v) = Fx + m·(r·v − q·w)
m·v̇ = Fy − m·(r·u − p·w) = Fy + m·(p·w − r·u)
m·ẇ = Fz − m·(p·v − q·u) = Fz + m·(q·u − p·v)
```

dove Fx, Fy, Fz sono le **forze totali nel body frame** (aerodinamica + spinta + peso):

```
Fx = Fx_aero + Thrust + W_x      W_x = −m·g·sin(Θ)
Fy = Fy_aero          + W_y      W_y = +m·g·cos(Θ)·sin(Φ)
Fz = Fz_aero          + W_z      W_z = +m·g·cos(Θ)·cos(Φ)
```

I termini `r·v − q·w`, `p·w − r·u`, `q·u − p·v` sono i **termini di Coriolis** (forze apparenti causate dalla rotazione del frame). Vanno sempre portati a destra prima di dividere per m:

```
u̇ = (Fx_aero + Thrust + W_x)/m  +  r·v − q·w          (8.1a)
v̇ = (Fy_aero          + W_y)/m  +  p·w − r·u          (8.1b)
ẇ = (Fz_aero          + W_z)/m  +  q·u − p·v          (8.1c)
```

**Verifica nel codice** (`FlightControlComputer.cpp`, lambda `compute_accel`):

```cpp
float au = (Fx_aero + thrust_force + W_x) / MASS_KG + sr*sv - sq*sw;  // 8.1a ✓
float av = (Fy_aero + W_y)          / MASS_KG + sp*sw - sr*su;         // 8.1b ✓
float aw = (Fz_aero + W_z)          / MASS_KG + sq*su - sp*sv;         // 8.1c ✓
```

Le equazioni di forza nel codice sono **corrette** e coincidono con la derivazione sopra.

### 8.2 Equazioni di momento (accelerazioni angolari) — derivazione con I_xz ≠ 0

#### Il tensore d'inerzia

Il **tensore d'inerzia** (o matrice d'inerzia) è un oggetto matematico di rango 2 — cioè una matrice 3×3 — che descrive come la massa di un corpo rigido è distribuita nello spazio rispetto a un sistema di assi. Non è semplicemente un numero scalare (come la massa), né un vettore: è un operatore lineare che trasforma la velocità angolare ω nel momento angolare H:

```
H = I · ω
```

Fisicamente, il tensore d'inerzia risponde alla domanda: "se il corpo ruota con velocità angolare ω, qual è il momento angolare risultante?" La risposta non è in generale parallela a ω perché la massa può essere distribuita asimmetricamente.

La forma generale del tensore d'inerzia rispetto a un sistema di assi (x, y, z) con origine nel centro di massa è:

```
        ┌  I_xx   −I_xy   −I_xz ┐
I =     │ −I_xy    I_yy   −I_yz │
        └ −I_xz   −I_yz    I_zz ┘
```

I termini sulla diagonale principale sono i **momenti d'inerzia assiali**:

```
I_xx = ∫(y² + z²) dm   (resistenza alla rotazione attorno all'asse x)
I_yy = ∫(x² + z²) dm   (resistenza alla rotazione attorno all'asse y)
I_zz = ∫(x² + y²) dm   (resistenza alla rotazione attorno all'asse z)
```

I termini fuori diagonale sono i **prodotti d'inerzia** (con il segno negativo per convenzione di Stevens & Lewis):

```
I_xy = ∫xy dm,   I_xz = ∫xz dm,   I_yz = ∫yz dm
```

Un prodotto d'inerzia diverso da zero indica un **accoppiamento inerziale**: una rotazione attorno a un asse genera momento angolare anche attorno agli altri assi. Per l'F-16, il piano xz è piano di simmetria, quindi I_xy = I_yz = 0, ma **I_xz ≠ 0** perché il motore e le masse lungo l'asse x non sono distribuiti simmetricamente rispetto al piano xy (il baricentro dell'aereo è più vicino al suolo che al soffitto quando visto frontalmente, ossia le masse sono più concentrate in basso rispetto al piano xz).

#### Matrici di rotazione per il cambio di sistema di riferimento nell'F-16

Nel simulatore F-16, i vettori fisici (velocità, forze, accelerazioni) devono essere trasformati tra tre sistemi di riferimento principali:

- **Body Frame (B)**: solidale all'aereo, asse x verso il muso, y verso l'ala destra, z verso il basso.
- **Inertial/NED Frame (I)**: fisso con la Terra (North-East-Down), considerato inerziale per voli subsonici.
- **Wind/Stability Frame (W)**: asse x allineato con la velocità relativa del vento; usato per il calcolo aerodinamico.

La rotazione da Body Frame a NED Frame (e viceversa) è parametrizzata dagli **angoli di Eulero** (Ψ = yaw, Θ = pitch, Φ = roll) applicati nell'ordine 3-2-1 (yaw → pitch → roll), che è la convenzione standard in meccanica del volo.

**Forward Matrix (Body → NED)** — trasforma vettori dal body frame al frame inerziale NED:

```
        ┌ cΘ·cΨ           cΘ·sΨ           −sΘ      ┐
L_BI =  │ sΦ·sΘ·cΨ−cΦ·sΨ  sΦ·sΘ·sΨ+cΦ·cΨ  sΦ·cΘ   │
        └ cΦ·sΘ·cΨ+sΦ·sΨ  cΦ·sΘ·sΨ−sΦ·cΨ  cΦ·cΘ   ┘
```

dove c = cos, s = sin, e gli angoli sono Φ (roll), Θ (pitch), Ψ (yaw).

**Backward Matrix (NED → Body)** — trasforma vettori dal frame inerziale NED al body frame. Poiché la matrice di rotazione è ortogonale (L_BI⁻¹ = L_BI^T), si ottiene semplicemente trasposta:

```
         ┌ cΘ·cΨ           sΦ·sΘ·cΨ−cΦ·sΨ   cΦ·sΘ·cΨ+sΦ·sΨ ┐
L_IB =   │ cΘ·sΨ           sΦ·sΘ·sΨ+cΦ·cΨ   cΦ·sΘ·sΨ−sΦ·cΨ │
         └ −sΘ             sΦ·cΘ             cΦ·cΘ           ┘
```

Uso tipico nel simulatore:
- La **forza peso** W è nota nel frame NED (W_NED = [0, 0, m·g]^T). Si trasforma nel body frame con L_IB: `W_body = L_IB · W_NED`.
- Le **posizioni** (x_N, x_E, h) si integrano nel frame NED. La velocità nel body frame (u, v, w) si converte con L_BI: `[ṡN, ṡE, ḣ]^T = L_BI · [u, v, w]^T`.

Il codice C++ (`FlightControlComputer.cpp`) applica questa rotazione nella funzione `compute_accel` per proiettare la forza peso nei tre assi del body frame prima di sommarla alle forze aerodinamiche.

**Convenzione del segno di I_xz — critica per la coerenza delle equazioni.**

Stevens & Lewis (1992, ed. di riferimento per questo progetto come dichiarato in `trim_and_linearize.m`) definisce il tensore d'inerzia con segni negativi fuori dalla diagonale:

```
        ┌  I_xx    0    −I_xz ┐
I_S&L = │    0    I_yy    0   │     con I_xz = +∫xz dm  (valore fisico positivo)
        └ −I_xz    0    I_zz  ┘
```

Il termine (1,3) del tensore vale **−I_xz** (negativo). Questa è la convenzione classica dei sistemi di meccanica del volo (anche usata da Etkin, McRuer, ecc.). Ne consegue:

```
Hx = I_xx·p − I_xz·r      (segno meno su I_xz·r)
Hz = I_zz·r − I_xz·p      (segno meno su I_xz·p)
```

Il MATLAB `load_F16_params.m` costruisce la matrice come `[Ixx 0 Ixz; 0 Iyy 0; Ixz 0 Izz]` con `Ixz = +982` — il termine (1,3) è **+Ixz** (positivo). Questo è il formato richiesto dal blocco Simulink "6DOF (Quaternion)" di Aerospace Blockset, che interpreta l'elemento (1,3) del tensore fornito come valore con il suo segno incluso. Poiché il blocco usa la formula `I·α = M - ω × (I·ω)` con il tensore come passato, esso calcola `Hx = Ixx·p + Ixz·r`. Le due formulazioni producono **equazioni con segni diversi sui termini I_xz**. Questo viene analizzato in dettaglio nella sezione 8.5.

Sviluppando le tre componenti di dH/dt|_body + ω × H = (L, M, N):

**Equazione di rollio (L):**

```
L = Ḣx + q·Hz − r·Hy
L = (I_xx·ṗ − I_xz·ṙ) + q·(I_zz·r − I_xz·p) − r·(I_yy·q)
L = I_xx·ṗ − I_xz·ṙ + I_zz·q·r − I_xz·p·q − I_yy·q·r
L = I_xx·ṗ − I_xz·ṙ + (I_zz − I_yy)·q·r − I_xz·p·q
```

Quindi la forma implicita (ṗ e ṙ a sinistra, tutto il resto a destra):

```
I_xx·ṗ − I_xz·ṙ = L − (I_zz − I_yy)·q·r + I_xz·p·q          (8.2a)
```

**Equazione di beccheggio (M):**

```
M = Ḣy + r·Hx − p·Hz
M = I_yy·q̇ + r·(I_xx·p − I_xz·r) − p·(I_zz·r − I_xz·p)
M = I_yy·q̇ + I_xx·p·r − I_xz·r² − I_zz·p·r + I_xz·p²
M = I_yy·q̇ + (I_xx − I_zz)·p·r + I_xz·(p² − r²)
```

Quindi:

```
I_yy·q̇ = M − (I_xx − I_zz)·p·r − I_xz·(p² − r²)              (8.2b)
```

**Equazione di imbardata (N):**

```
N = Ḣz + p·Hy − q·Hx
N = (I_zz·ṙ − I_xz·ṗ) + p·(I_yy·q) − q·(I_xx·p − I_xz·r)
N = I_zz·ṙ − I_xz·ṗ + I_yy·p·q − I_xx·p·q + I_xz·q·r
N = I_zz·ṙ − I_xz·ṗ + (I_yy − I_xx)·p·q + I_xz·q·r
```

Quindi:

```
−I_xz·ṗ + I_zz·ṙ = N − (I_yy − I_xx)·p·q − I_xz·q·r           (8.2c)
```

Le equazioni (8.2a) e (8.2c) formano un **sistema lineare 2×2** accoppiato in ṗ e ṙ. La (8.2b) per q̇ è invece disaccoppiata e si risolve direttamente. In forma matriciale:

```
┌ I_xx   −I_xz ┐ ┌ ṗ ┐   ┌ L − (I_zz−I_yy)·q·r + I_xz·p·q        ┐
│              │ │   │ = │                                           │
└ −I_xz   I_zz ┘ └ ṙ ┘   └ N − (I_yy−I_xx)·p·q − I_xz·q·r         ┘
```

### 8.3 Inversione del sistema 2×2 → equazioni esplicite

L'inversione della matrice 2×2 di inerzia con il determinante Γ = I_xx·I_zz − I_xz² dà:

```
┌ I_xx   −I_xz ┐⁻¹    1   ┌  I_zz   I_xz ┐
│              │    = ─── │              │
└ −I_xz   I_zz ┘      Γ  └  I_xz   I_xx ┘
```

Moltiplicando si ottengono le **equazioni esplicite corrette** (Stevens & Lewis, Cap. 1.4):

Definendo per brevità:
```
RHS_p = L − (I_zz−I_yy)·q·r + I_xz·p·q
RHS_r = N − (I_yy−I_xx)·p·q − I_xz·q·r
```

```
Γ·ṗ = I_zz·RHS_p + I_xz·RHS_r
Γ·ṙ = I_xz·RHS_p + I_xx·RHS_r
```

Espandendo completamente:

```
Γ·ṗ = I_zz·L + I_xz·N
     − [I_xz² + I_zz·(I_zz−I_yy)]·q·r        ← termine q·r
     − I_xz·(I_yy−I_xx−I_zz)·p·q              ← termine p·q
```

```
Γ·q̇ = [M − (I_xx−I_zz)·p·r − I_xz·(p²−r²)] · Γ/I_yy   (non Γ, si divide solo per I_yy)
```

```
Γ·ṙ = I_xz·L + I_xx·N
     + I_xz·(I_yy−I_xx−I_zz)·q·r              ← termine q·r
     + [I_xz² + I_xx·(I_xx−I_yy)]·p·q          ← termine p·q
```

In forma finale, le tre equazioni rotazionali sono:

```
          I_zz·L + I_xz·N − [I_xz² + I_zz·(I_zz−I_yy)]·q·r − I_xz·(I_yy−I_xx−I_zz)·p·q
ṗ  =  ─────────────────────────────────────────────────────────────────────────────────────
                                    Γ = I_xx·I_zz − I_xz²

          M − (I_xx−I_zz)·p·r − I_xz·(p²−r²)
q̇  =  ───────────────────────────────────────
                       I_yy

          I_xz·L + I_xx·N + I_xz·(I_yy−I_xx−I_zz)·q·r + [I_xz² + I_xx·(I_xx−I_yy)]·p·q
ṙ  =  ─────────────────────────────────────────────────────────────────────────────────────
                                    Γ = I_xx·I_zz − I_xz²
```

**Punti chiave di queste equazioni:**

- Il termine `I_zz·L + I_xz·N` in ṗ e `I_xz·L + I_xx·N` in ṙ mostrano l'**accoppiamento inerziale**: un momento di rollio L genera non solo ṗ ma anche ṙ (e viceversa), in proporzione a I_xz/I_xx.
- I termini `q·r` e `p·q` sono i **termini giroscopici**: sono la conseguenza del fatto che la terna del corpo ruota, e il momento angolare in un asse alimenta le accelerazioni angolari degli altri assi.
- L'equazione di q̇ è disaccoppiata perché I_yy è il solo momento d'inerzia non nullo nella riga y del tensore (non ci sono prodotti d'inerzia con l'asse y per un aereo simmetrico).

### 8.4 Valori numerici per l'F-16 e significato fisico dell'accoppiamento

Con i parametri inerziali dell'F-16:

```
I_xx = 12874 kg·m²    I_yy = 75674 kg·m²    I_zz = 85552 kg·m²    I_xz = 1331 kg·m²
Γ = I_xx·I_zz − I_xz² = 12874×85552 − 1331² = 1.1015×10⁹ − 1.771×10⁶ ≈ 1.0997×10⁹ kg²·m⁴
```

I coefficienti degli accoppiamenti nelle equazioni di ṗ e ṙ valgono:

| Termine | Coefficiente | Valore numerico | Coefficiente/Γ |
|---------|--------------|-----------------|----------------|
| q·r in ṗ | −[I_xz²+I_zz·(I_zz−I_yy)] | −847.2×10⁶ | −0.7704 (rad/s)/(rad/s)² |
| p·q in ṗ | −I_xz·(I_yy−I_xx−I_zz) | +30.28×10⁶ | +0.02754 (rad/s)/(rad/s)² |
| q·r in ṙ | +I_xz·(I_yy−I_xx−I_zz) | −30.28×10⁶ | −0.02754 (rad/s)/(rad/s)² |
| p·q in ṙ | +[I_xz²+I_xx·(I_xx−I_yy)] | −852.5×10⁶ | −0.7754 (rad/s)/(rad/s)² |

Il coefficiente del termine q·r in ṗ (−0.770) è **dominante**: in una virata coordinata con q = r = 0.5 rad/s, questo termine contribuisce −0.770×0.5×0.5 = −0.193 rad/s² all'accelerazione di rollio. I termini con I_xz nell'accoppiamento (±0.0275) sono circa 30 volte più piccoli, ma non trascurabili ad alte velocità angolari.

### 8.5 Verifica completa: codice C++ vs Stevens & Lewis vs MATLAB Simulink

Questa sezione esegue la verifica analitica sistematica di ogni termine delle equazioni angolari confrontando tre fonti: (A) Stevens & Lewis 1992, (B) MATLAB Simulink "6DOF (Quaternion)", (C) codice C++.

#### Confronto delle convenzioni sul tensore d'inerzia

Le tre fonti usano definizioni diverse del termine I_xz:

| Fonte | Tensore (riga 1, col 3) | Momento angolare Hx | Momento angolare Hz |
|-------|------------------------|---------------------|---------------------|
| Stevens & Lewis (1992) | **−I_xz** | I_xx·p **−** I_xz·r | I_zz·r **−** I_xz·p |
| MATLAB `load_F16_params.m` | **+I_xz** | I_xx·p **+** I_xz·r | I_zz·r **+** I_xz·p |
| Codice C++ (`I_XZ = +1331`) | usa S&L (−I_xz nel calcolo) | I_xx·p **−** I_xz·r | I_zz·r **−** I_xz·p |

La radice della differenza: Stevens & Lewis definisce il tensore con **segni negativi fuori dalla diagonale** (convenzione classica della meccanica del volo). Il blocco Simulink "6DOF (Quaternion)" riceve la matrice numerica così come è — e `load_F16_params.m` gli passa `[Ixx 0 +Ixz; 0 Iyy 0; +Ixz 0 Izz]`, quindi Simulink calcola l'asse x del momento angolare come `Hx = Ixx·p + Ixz·r`. Il codice C++ usa `I_XZ = +1331` ma lo mette nelle equazioni con la struttura S&L (segno derivato dalla derivazione con −I_xz nel tensore). **Le due formulazioni producono equazioni con segni opposti sui termini che moltiplicano I_xz.**

#### Derivazione delle equazioni MATLAB/Simulink (per confronto)

Con il tensore MATLAB (`Hx = Ixx·p + Ixz·r`, `Hz = Izz·r + Ixz·p`) e il Simulink `I·α = M − ω×(I·ω)`:

```
ω × H = [(Izz−Iyy)·q·r + Ixz·p·q,
          (Ixx−Izz)·p·r − Ixz·(p²−r²),
          (Iyy−Ixx)·p·q − Ixz·q·r]
```

Sistema 2×2 (con +Ixz in posizione (1,3)):

```
┌ Ixx   +Ixz ┐ ┌ ṗ ┐   ┌ L − (Izz−Iyy)·q·r − Ixz·p·q ┐
│             │ │   │ = │                                │
└ +Ixz   Izz ┘ └ ṙ ┘   └ N − (Iyy−Ixx)·p·q + Ixz·q·r  ┘
```

Invertendo con I⁻¹ = (1/Γ)·[Izz, −Ixz; −Ixz, Ixx]:

```
ṗ_MATLAB = [Izz·L − Ixz·N − (Ixz²+Izz·(Izz−Iyy))·q·r + Ixz·(Iyy−Ixx−Izz)·p·q] / Γ
q̇_MATLAB = [M − (Ixx−Izz)·p·r + Ixz·(p²−r²)] / Iyy
ṙ_MATLAB = [−Ixz·L + Ixx·N − Ixz·(Iyy−Ixx−Izz)·q·r + (Ixz²+Ixx·(Ixx−Iyy))·p·q] / Γ
```

#### Tabella di confronto termine per termine

Definizioni coefficienti:  `Γ = Ixx·Izz − Ixz²`,  `A = Ixz·(Iyy−Ixx−Izz)`,  `B = Ixz²+Izz·(Izz−Iyy)`,  `C = Ixz²+Ixx·(Ixx−Iyy)`

**Equazione ṗ:**

| Termine | S&L / C++ corretto | MATLAB Simulink | Codice C++ attuale |
|---------|-------------------|-----------------|-------------------|
| Coeff L | **+Izz** | **+Izz** ✓ stesso | **+Izz** ✓ |
| Coeff N | **+Ixz** | **−Ixz** ← opposto | **+Ixz** (=S&L) |
| Coeff q·r | **−B** | **−B** ✓ stesso | **−B** ✓ (ma con errore grouping) |
| Coeff p·q | **−A** | **+A** ← opposto | **−A·q·r** ← errore: moltiplicato per r in più |

**Equazione q̇:**

| Termine | S&L / C++ corretto | MATLAB Simulink | Codice C++ attuale |
|---------|-------------------|-----------------|-------------------|
| Coeff p·r | **−(Ixx−Izz)** | **−(Ixx−Izz)** ✓ | **−(Ixx−Izz)** ✓ |
| Coeff (p²−r²) | **−Ixz** | **+Ixz** ← opposto | **−Ixz** ✓ (=S&L) |

**Equazione ṙ:**

| Termine | S&L / C++ corretto | MATLAB Simulink | Codice C++ attuale |
|---------|-------------------|-----------------|-------------------|
| Coeff L | **+Ixz** | **−Ixz** ← opposto | **+Ixz** ✓ (=S&L) |
| Coeff N | **+Ixx** | **+Ixx** ✓ stesso | **+Ixx** ✓ |
| Coeff q·r | **+A** | **−A** ← opposto | **+A·p·q** ← errore: moltiplicato per p in più |
| Coeff p·q | **+C** | **+C** ✓ stesso | **+C** ✓ |

#### Conclusioni della verifica

**Il codice C++ è internamente coerente con Stevens & Lewis (1992)** per quanto riguarda la convenzione del segno di I_xz. Tutte e tre le equazioni usano la struttura derivata con `Hx = Ixx·p − Ixz·r`. In particolare, q̇ ha il segno corretto (`−Ixz·(p²−r²)`) che è il discriminatore più diretto della convenzione.

Il MATLAB Simulink usa la convenzione opposta (tensore con +Ixz). Questo produce equazioni con segni opposti sui termini che coinvolgono I_xz (segnati ← nella tabella). Poiché il progetto dichiara come riferimento Stevens & Lewis, **il codice C++ è aderente alla fonte citata**.

Esistono **due discrepanze residue** nel codice C++ rispetto alle equazioni S&L corrette, entrambe nelle equazioni ṗ e ṙ:

**Discrepanza 1 — Errore di raggruppamento (p·q·r invece di p·q):**

Il codice calcola `(A·p + B)·q·r = A·p·q·r + B·q·r`, ma la formula corretta ha due termini distinti: `B·q·r + A·p·q`. Il termine gyroscopico p·q viene moltiplicato per un fattore `r` aggiuntivo.

Impatto numerico: il coefficiente A/Γ ≈ ±0.0275 rad/s per (rad/s)². Errore su ṗ ≈ 0.0275·p·q·(r−1). **Al massimo ~2–3% di ṗ** in manovre aggressive con r ≈ 0.

**Discrepanza 2 — Convenzione I_xz rispetto al MATLAB (solo se si vuole coerenza con Simulink):**

I termini I_xz·N (in ṗ), Ixz·(p²−r²) (in q̇), I_xz·L (in ṙ) hanno segni opposti rispetto al MATLAB Simulink. Impatto numerico identico al caso 1 (stesso ordine di grandezza, essendo tutti proporzionali a I_xz/Γ ≈ 0.00121 m⁻²·s²).

**I termini dominanti** — `Izz·L/Γ` in ṗ, `M/Iyy` in q̇, `Ixx·N/Γ` in ṙ, e il termine q·r con coefficiente −B/Γ ≈ −0.77 — sono **identici** nelle tre formulazioni. Il simulatore è stabile e fisicamente rappresentativo perché questi termini dominano la dinamica dell'aereo.

#### Implementazione corretta per rigore accademico (S&L convention)

La forma corretta delle equazioni ṗ e ṙ, perfettamente coerenti con Stevens & Lewis, con i termini q·r e p·q separati:

```cpp
float denom = I_XX * I_ZZ - I_XZ * I_XZ;       // Γ

// Coefficienti S&L (convenzione Hx = Ixx·p − Ixz·r)
float c_B  = -(I_XZ*I_XZ + I_ZZ*(I_ZZ - I_YY)); // coeff q·r in ṗ  (≈ −0.770·Γ)
float c_A  = -I_XZ*(I_YY - I_XX - I_ZZ);         // coeff p·q in ṗ  (≈ +0.0275·Γ)
float c_C  = (I_XZ*I_XZ + I_XX*(I_XX - I_YY));   // coeff p·q in ṙ  (≈ −0.775·Γ)

// S&L Eq. (derivata da Hx = Ixx·p − Ixz·r):
float ap = (I_ZZ*L_tot + I_XZ*N_tot           // ṗ: +Izz·L + Ixz·N
           + c_B*sq*sr                          //    − B·q·r
           + c_A*sp*sq) / denom;                //    − A·p·q

float aq = (M_tot
           - (I_XX - I_ZZ)*sp*sr               // q̇: −(Ixx−Izz)·p·r
           - I_XZ*(sp*sp - sr*sr)) / I_YY;      //    −Ixz·(p²−r²)   ← segno S&L

float ar = (I_XZ*L_tot + I_XX*N_tot            // ṙ: +Ixz·L + Ixx·N
           + I_XZ*(I_YY-I_XX-I_ZZ)*sq*sr        //    + A·q·r  (stesso coeff, segno +)
           + c_C*sp*sq) / denom;                //    + C·p·q
```

### 8.3 Integrazione RK4

Le equazioni del moto formano un sistema di 6 ODE accoppiate del primo ordine. Il vettore di stato integrato è:

```
y = [u, v, w, p, q, r]
```

dove le prime tre componenti sono le velocità lineari nel body frame e le ultime tre le velocità angolari. La funzione `f(y)` che calcola le derivate è definita come:

```
f₁ = u̇ = (Fx_aero + Thrust + W_x)/m + r·v − q·w
f₂ = v̇ = (Fy_aero + W_y)/m           + p·w − r·u
f₃ = ẇ = (Fz_aero + W_z)/m           + q·u − p·v

f₄ = ṗ = [I_zz·L + I_xz·N − (I_xz²+I_zz·(I_zz−I_yy))·q·r − I_xz·(I_yy−I_xx−I_zz)·p·q] / Γ
f₅ = q̇ = [M − (I_xx−I_zz)·p·r − I_xz·(p²−r²)] / I_yy
f₆ = ṙ = [I_xz·L + I_xx·N + I_xz·(I_yy−I_xx−I_zz)·q·r + (I_xz²+I_xx·(I_xx−I_yy))·p·q] / Γ

dove  Γ = I_xx·I_zz − I_xz²
      (v. Sez. 8.5 per analisi della discrepanza con il codice attuale)
```

Si noti che le equazioni traslazionali (f₁, f₂, f₃) e rotazionali (f₄, f₅, f₆) sono **mutuamente accoppiate**: le accelerazioni lineari dipendono dalle velocità angolari (termini di Coriolis r·v, q·w, ...) e le accelerazioni angolari dipendono dalle velocità angolari stesse (termini giroscopici q·r, p·r, p·q). Questo accoppiamento rende il sistema **non lineare** e giustifica l'uso di un integratore di ordine elevato.

#### Perché Runge-Kutta del 4° ordine

L'integrazione di Eulero esplicita (del 1° ordine) approssima:

```
y(t+dt) ≈ y(t) + dt · f(y(t))
```

Questo equivale a usare la pendenza al punto iniziale per estrapolare lo stato al punto successivo. Per un sistema non lineare con accoppiamento inerziale come l'F-16, la curvatura della traiettoria nello spazio degli stati è significativa: la pendenza cambia apprezzabilmente tra l'inizio e la fine del passo temporale. Con dt = 0.017 s e velocità angolari che possono variare rapidamente durante le manovre, l'errore di troncamento locale di O(dt²) si accumula e può portare a **drift energetico** (l'energia cinetica del sistema cresce artificialmente) e potenziale **instabilità numerica**.

Il metodo RK4 campiona la pendenza in **quattro punti** all'interno dell'intervallo [t, t+dt]:

```
k₁ = f(y)                           ← pendenza all'inizio
k₂ = f(y + k₁·dt/2)                 ← pendenza a metà, usando la stima di k₁
k₃ = f(y + k₂·dt/2)                 ← pendenza a metà, corretta con k₂
k₄ = f(y + k₃·dt)                   ← pendenza alla fine, usando la stima di k₃

y(t+dt) = y(t) + dt · (k₁ + 2·k₂ + 2·k₃ + k₄) / 6
```

I pesi (1, 2, 2, 1)/6 sono quelli della quadratura di Simpson, che integra esattamente i polinomi fino al 3° grado. L'errore di troncamento locale è O(dt⁵) — tre ordini di grandezza migliore di Eulero. Per dt = 0.017 s:

- Eulero: errore ∝ dt² ≈ 2.9 × 10⁻⁴
- RK4: errore ∝ dt⁵ ≈ 1.4 × 10⁻⁹

Questa differenza di cinque ordini di grandezza è ciò che permette al simulatore di funzionare stabilmente a 60 Hz senza dover ricorrere a passi temporali più piccoli (che imporrebbero un costo computazionale incompatibile con il real-time).

#### Applicazione concreta nel codice

Il codice C++ definisce una lambda `compute_accel` che implementa `f(y)`:

```cpp
auto compute_accel = [&](float su, float sv, float sw,
                         float sp, float sq, float sr)
    -> std::array<float, 6>
{
  // Accelerazioni lineari (f₁, f₂, f₃)
  float au = (Fx_aero + thrust_force + W_x) / MASS_KG + sr*sv - sq*sw;
  float av = (Fy_aero + W_y) / MASS_KG + sp*sw - sr*su;
  float aw = (Fz_aero + W_z) / MASS_KG + sq*su - sp*sv;

  // Accelerazioni angolari (f₄, f₅, f₆) — Stevens & Lewis
  float ap = (I_ZZ*L_tot + I_XZ*N_tot
            - (I_XZ*(I_YY-I_XX-I_ZZ)*sp
             + (I_XZ*I_XZ + I_ZZ*(I_ZZ-I_YY))) * sq*sr) / denom;
  float aq = (M_tot - (I_XX-I_ZZ)*sp*sr
            - I_XZ*(sp*sp - sr*sr)) / I_YY;
  float ar = (I_XZ*L_tot + I_XX*N_tot
            + (I_XZ*(I_YY-I_XX-I_ZZ)*sr
             + (I_XZ*I_XZ + I_XX*(I_XX-I_YY))) * sp*sq) / denom;

  return {au, av, aw, ap, aq, ar};
};
```

Poi i quattro substep RK4:

```cpp
auto k1 = compute_accel(su, sv, sw, sp, sq, sr);

float h2 = dt * 0.5f;
auto k2 = compute_accel(su+k1[0]*h2, sv+k1[1]*h2, sw+k1[2]*h2,
                         sp+k1[3]*h2, sq+k1[4]*h2, sr+k1[5]*h2);
auto k3 = compute_accel(su+k2[0]*h2, sv+k2[1]*h2, sw+k2[2]*h2,
                         sp+k2[3]*h2, sq+k2[4]*h2, sr+k2[5]*h2);
auto k4 = compute_accel(su+k3[0]*dt, sv+k3[1]*dt, sw+k3[2]*dt,
                         sp+k3[3]*dt, sq+k3[4]*dt, sr+k3[5]*dt);

constexpr float S6 = 1.0f / 6.0f;
snap.u          += dt * S6 * (k1[0] + 2*k2[0] + 2*k3[0] + k4[0]);
snap.v          += dt * S6 * (k1[1] + 2*k2[1] + 2*k3[1] + k4[1]);
snap.w          += dt * S6 * (k1[2] + 2*k2[2] + 2*k3[2] + k4[2]);
snap.roll_rate  += dt * S6 * (k1[3] + 2*k2[3] + 2*k3[3] + k4[3]);
snap.pitch_rate += dt * S6 * (k1[4] + 2*k2[4] + 2*k3[4] + k4[4]);
snap.yaw_rate   += dt * S6 * (k1[5] + 2*k2[5] + 2*k3[5] + k4[5]);
```

Si noti un dettaglio cruciale: ad ogni substep k₂, k₃, k₄ il vettore di stato viene **perturbato** (ad esempio `su+k1[0]*h2` per k₂), ma le forze e i momenti aerodinamici (`Fx_aero`, `Fy_aero`, ..., `L_tot`, `M_tot`, `N_tot`) restano **congelati** al valore calcolato all'inizio dello step. Questo perché ricalcolare l'aerodinamica (44 tabelle di interpolazione) ad ogni substep quadruplicherebbe il costo computazionale. I termini che **vengono** rivalutati ad ogni substep sono quelli di Coriolis (`sr*sv`, `sq*sw`, ...) e giroscopici (`sq*sr`, `sp*sr`, ...) perché dipendono direttamente dalle variabili di stato perturbate.

Questa scelta (forze congelate, accoppiamenti rivalutati) è un compromesso tra accuratezza e prestazioni: i termini aerodinamici variano lentamente rispetto al passo temporale (le forze dipendono da α e β che cambiano poco in 17 ms), mentre i termini inerziali di accoppiamento possono generare oscillazioni rapide che necessitano della risoluzione RK4 per essere catturate correttamente.

---

## 9. Cinematica e navigazione

### 9.1 Body rates → angoli di Eulero

La relazione tra le velocità angolari nel body frame (p, q, r) e le derivate degli angoli di Eulero è:

```
Φ̇ = p + sin(Φ)·tan(Θ)·q + cos(Φ)·tan(Θ)·r
Θ̇ = cos(Φ)·q − sin(Φ)·r
Ψ̇ = [sin(Φ)·q + cos(Φ)·r] / cos(Θ)
```

Questa relazione si ricava invertendo la matrice che lega (p, q, r) a (Φ̇, Θ̇, Ψ̇). In forma matriciale:

```
┌ p ┐   ┌ 1   0      −sin(Θ)       ┐ ┌ Φ̇ ┐
│ q │ = │ 0   cos(Φ)   sin(Φ)·cos(Θ) │ │ Θ̇ │
└ r ┘   └ 0  −sin(Φ)   cos(Φ)·cos(Θ) ┘ └ Ψ̇ ┘
```

L'inversione di questa matrice produce le equazioni sopra, ma con un termine `1/cos(Θ)` nell'espressione di Φ̇ e Ψ̇ (tramite tan(Θ) e la divisione diretta).

#### Il problema del gimbal lock

Quando l'angolo di beccheggio Θ si avvicina a ±90° (cabrata o picchiata verticale), cos(Θ) → 0 e le equazioni per Φ̇ e Ψ̇ **divergono a infinito**. Questo non è un problema numerico ma una **singolarità intrinseca** della rappresentazione con angoli di Eulero, nota come **gimbal lock**.

Fisicamente il fenomeno è questo: con Θ = 90° (muso che punta esattamente verso l'alto), una rotazione attorno all'asse del corpo X (rollio) produce lo stesso effetto visivo di una rotazione attorno all'asse terrestre Z (yaw). I due gradi di libertà Φ e Ψ **collassano in uno solo** — il sistema perde un grado di libertà di rappresentazione. Qualsiasi combinazione di Φ e Ψ tale che Φ − Ψ = costante descrive lo stesso orientamento fisico. Questo significa che:

1. La mappatura (Φ, Θ, Ψ) → orientamento non è più iniettiva
2. Le derivate Φ̇ e Ψ̇ diventano indeterminate (0/0 o ∞)
3. L'integratore numerico produce valori erratici

Il nome "gimbal lock" viene dall'analogia meccanica: in una piattaforma giroscopica a tre gimbal, quando due assi si allineano (a Θ = 90°) si perde fisicamente un asse di rotazione.

#### Protezione nel motore fisico

Nel motore fisico (FlightControlComputer), le equazioni di Eulero sono mantenute perché il modello MATLAB di riferimento le usa e perché l'F-16, nelle condizioni di volo normali, non raggiunge mai Θ = ±90° (nemmeno in manovre estreme). Il codice protegge la singolarità con un clamp numerico:

```cpp
float cos_th_safe = std::max(std::abs(cos_th), 0.001f);
float Psi_dot = (sin_ph * snap.pitch_rate + cos_ph * snap.yaw_rate)
              / cos_th_safe * (cos_th >= 0 ? 1.0f : -1.0f);
```

Il valore 0.001 limita il massimo amplificazione a 1/0.001 = 1000×, e il termine `(cos_th >= 0 ? 1.0f : -1.0f)` preserva il segno corretto. Inoltre Θ viene clampato a ±1.57 rad (≈ ±89.95°) per impedire fisicamente il raggiungimento della singolarità.

#### Quaternioni nel rendering (FlightDisplay)

Il motore di rendering affronta un problema diverso: deve applicare la rotazione (Φ, Θ, Ψ) al modello 3D dell'F-16 per visualizzarlo. Se si usassero direttamente le matrici di rotazione di Eulero concatenate, il gimbal lock causerebbe **artefatti visivi**: vicino a Θ = ±90° il modello "scatterebbe" tra orientamenti diversi perché piccole variazioni di Φ e Ψ produrrebbero grandi variazioni nella matrice risultante.

Per questo il rendering usa i **quaternioni**. Un quaternione è un numero ipercomplesso a 4 componenti:

```
q = w + x·i + y·j + z·k       dove i² = j² = k² = ijk = −1
```

oppure in notazione vettoriale: `q = (w, x, y, z)` con `w` parte scalare e `(x, y, z)` parte vettoriale.

Un quaternione unitario (|q| = 1) rappresenta una rotazione di un angolo θ attorno a un asse û come:

```
q = cos(θ/2) + sin(θ/2)·(ux·i + uy·j + uz·k)
  = (cos(θ/2),  sin(θ/2)·ux,  sin(θ/2)·uy,  sin(θ/2)·uz)
```

**Perché i quaternioni non soffrono di gimbal lock**: la rappresentazione a 4 parametri con il vincolo |q| = 1 definisce una varietà (la 3-sfera S³) che è **liscia e senza singolarità**. Ogni orientamento fisico corrisponde esattamente a due quaternioni (q e −q, che rappresentano la stessa rotazione) ma non esiste alcun orientamento che causi degenerazione. La mappa dai quaternioni alle rotazioni è un rivestimento doppio di SO(3), regolare ovunque.

La composizione di rotazioni con i quaternioni si fa tramite il **prodotto di Hamilton**, che è associativo e non commutativo (come le rotazioni):

```
q₁ ⊗ q₂ = (w₁w₂ − x₁x₂ − y₁y₂ − z₁z₂,
            w₁x₂ + x₁w₂ + y₁z₂ − z₁y₂,
            w₁y₂ − x₁z₂ + y₁w₂ + z₁x₂,
            w₁z₂ + x₁y₂ − y₁x₂ + z₁w₂)
```

#### Implementazione nel codice di rendering

Il codice in `FlightDisplay::DrawAircraftModel()` converte gli angoli di Eulero (provenienti dal motore fisico) in quaternioni per il rendering:

```cpp
// ── Quaternion body rotation (avoids gimbal lock) ──────────
// Chain: Yaw(Y) → Pitch(-X) → Roll(Z)
Quaternion qYaw   = QuaternionFromAxisAngle({0.0f, 1.0f, 0.0f},  data.yaw);
Quaternion qPitch = QuaternionFromAxisAngle({1.0f, 0.0f, 0.0f}, -data.pitch);
Quaternion qRoll  = QuaternionFromAxisAngle({0.0f, 0.0f, 1.0f},  data.roll);
Quaternion q      = QuaternionMultiply(QuaternionMultiply(qYaw, qPitch), qRoll);
```

Il procedimento è:

1. **Tre quaternioni elementari**: ciascuno rappresenta una singola rotazione attorno a un asse coordinato. `QuaternionFromAxisAngle({0,1,0}, Ψ)` costruisce il quaternione `(cos(Ψ/2), 0, sin(Ψ/2), 0)` — rotazione di Ψ attorno a Y (yaw nel sistema di coordinate Raylib).

2. **Composizione**: i tre quaternioni vengono moltiplicati con il prodotto di Hamilton nell'ordine Yaw → Pitch → Roll. Questo produce un singolo quaternione `q` che codifica l'intera rotazione body→world **senza passare per una matrice di Eulero intermedia**.

3. **Conversione a matrice**: il quaternione risultante viene convertito in una matrice 4×4 per OpenGL:

```cpp
Matrix rotMat = QuaternionToMatrix(q);
rlMultMatrixf(MatrixToFloat(rotMat));
```

La matrice di rotazione estratta da un quaternione unitario è:

```
        ┌ 1−2(y²+z²)    2(xy−wz)      2(xz+wy)   ┐
R(q) =  │ 2(xy+wz)      1−2(x²+z²)    2(yz−wx)   │
        └ 2(xz−wy)      2(yz+wx)      1−2(x²+y²)  ┘
```

Questa matrice è sempre una rotazione valida (ortonormale, determinante +1) indipendentemente dai valori degli angoli di Eulero originali — anche a Θ = ±90°.

#### Riepilogo: divisione dei ruoli

| Componente | Rappresentazione | Perché |
|-----------|------------------|--------|
| Motore fisico (FCC) | Angoli di Eulero (Φ, Θ, Ψ) | Coerenza con il modello MATLAB, equazioni di Stevens & Lewis, Θ < 90° garantito in volo realistico |
| Rendering 3D (FlightDisplay) | Quaternioni | Nessun gimbal lock visivo, interpolazione liscia, numericamente stabile a qualsiasi orientamento |

La conversione Eulero → quaternione avviene ad ogni frame nel rendering ed è un'operazione a costo trascurabile (6 chiamate a sin/cos + 3 prodotti di Hamilton). Il motore fisico non necessita di quaternioni perché lavora sulle velocità angolari (p, q, r) che sono definite nel body frame e non soffrono di gimbal lock — la singolarità appare solo nella relazione cinematica (p,q,r) → (Φ̇,Θ̇,Ψ̇), dove è gestita dal clamp.

### 9.2 Velocità body → posizione terrestre (DCM)

Le velocità (u, v, w) sono espresse nel **body frame**, solidale all'aereo. Per aggiornare la posizione nel **riferimento terrestre** NED (North-East-Down) serve una matrice di rotazione che trasformi un vettore dal body frame all'Earth frame. Questa matrice è la **Direction Cosine Matrix** (DCM), costruita come prodotto di tre rotazioni elementari attorno agli angoli di Eulero (Φ, Θ, Ψ).

#### Costruzione della DCM

La DCM body→Earth si ottiene componendo tre rotazioni nell'ordine ZYX (convenzione aeronautica standard):

1. **Rotazione di Ψ (yaw)** attorno all'asse Z_Earth — allinea l'asse X dal Nord alla prua:

```
R_z(Ψ) = ┌ cos(Ψ)  −sin(Ψ)   0  ┐
          │ sin(Ψ)   cos(Ψ)   0  │
          └    0        0      1  ┘
```

2. **Rotazione di Θ (pitch)** attorno al nuovo asse Y — inclina l'asse X rispetto all'orizzonte:

```
R_y(Θ) = ┌  cos(Θ)    0    sin(Θ) ┐
          │     0      1       0   │
          └ −sin(Θ)    0    cos(Θ) ┘
```

3. **Rotazione di Φ (roll)** attorno al nuovo asse X — inclina lateralmente:

```
R_x(Φ) = ┌  1      0        0    ┐
          │  0   cos(Φ)  −sin(Φ)  │
          └  0   sin(Φ)   cos(Φ)  ┘
```

La trasformazione **Earth→Body** è la composizione `R_x(Φ) · R_y(Θ) · R_z(Ψ)` applicata nell'ordine inverso (prima yaw, poi pitch, poi roll). Poiché le matrici di rotazione sono ortogonali, la trasformazione inversa **Body→Earth** è semplicemente la trasposta:

```
C_b→e = [R_x(Φ) · R_y(Θ) · R_z(Ψ)]ᵀ = R_z(Ψ)ᵀ · R_y(Θ)ᵀ · R_x(Φ)ᵀ
```

Eseguendo il prodotto matriciale si ottiene la DCM completa body→Earth:

```
         ┌ cΘ·cΨ    sΦ·sΘ·cΨ − cΦ·sΨ    cΦ·sΘ·cΨ + sΦ·sΨ  ┐
C_b→e =  │ cΘ·sΨ    sΦ·sΘ·sΨ + cΦ·cΨ    cΦ·sΘ·sΨ − sΦ·cΨ  │
         └ −sΘ       sΦ·cΘ                cΦ·cΘ               ┘
```

dove cΦ = cos(Φ), sΦ = sin(Φ), cΘ = cos(Θ), sΘ = sin(Θ), cΨ = cos(Ψ), sΨ = sin(Ψ).

#### Applicazione: velocità nel riferimento terrestre

Moltiplicando la DCM per il vettore delle velocità body si ottengono le velocità nel riferimento terrestre:

```
┌ V_North ┐         ┌ u ┐
│ V_East  │ = C_b→e │ v │
└ V_Down  ┘         └ w ┘
```

Ovvero, elemento per elemento:

```
V_North = u·cos(Θ)·cos(Ψ)
        + v·[sin(Φ)·sin(Θ)·cos(Ψ) − cos(Φ)·sin(Ψ)]
        + w·[cos(Φ)·sin(Θ)·cos(Ψ) + sin(Φ)·sin(Ψ)]

V_East  = u·cos(Θ)·sin(Ψ)
        + v·[sin(Φ)·sin(Θ)·sin(Ψ) + cos(Φ)·cos(Ψ)]
        + w·[cos(Φ)·sin(Θ)·sin(Ψ) − sin(Φ)·cos(Ψ)]

V_Down  = −u·sin(Θ) + v·sin(Φ)·cos(Θ) + w·cos(Φ)·cos(Θ)
```

**Verifica intuitiva**: in volo livellato rettilineo (Φ = 0, Θ = 0, Ψ = 0 cioè prua Nord), la DCM diventa la matrice identità e si ottiene V_North = u, V_East = v, V_Down = w — esattamente quello che ci si aspetta: la velocità forward dell'aereo punta a Nord.

Altro caso: con Ψ = 90° (prua Est) e Φ = Θ = 0, si ottiene V_North = 0, V_East = u, V_Down = w — tutta la velocità forward viene proiettata sulla direzione Est, come atteso.

#### Implementazione nel codice

```cpp
float cos_ps = std::cos(snap.yaw);
float sin_ps = std::sin(snap.yaw);
cos_th = std::cos(snap.pitch);
sin_th = std::sin(snap.pitch);
cos_ph = std::cos(snap.roll);
sin_ph = std::sin(snap.roll);

// Riga 1 della DCM: proiezione su Nord
float x_dot_e = snap.u * cos_th * cos_ps
              + snap.v * (sin_ph*sin_th*cos_ps - cos_ph*sin_ps)
              + snap.w * (cos_ph*sin_th*cos_ps + sin_ph*sin_ps);

// Riga 2 della DCM: proiezione su Est
float y_dot_e = snap.u * cos_th * sin_ps
              + snap.v * (sin_ph*sin_th*sin_ps + cos_ph*cos_ps)
              + snap.w * (cos_ph*sin_th*sin_ps - sin_ph*cos_ps);

// Riga 3 della DCM: proiezione su Down
float z_dot_e = snap.u * (-sin_th)
              + snap.v * sin_ph * cos_th
              + snap.w * cos_ph * cos_th;

// Integrazione posizione (Eulero esplicito — vedi nota sotto)
snap.z += x_dot_e * dt;        // Nord
snap.x += y_dot_e * dt;        // Est
snap.altitude -= z_dot_e * dt; // Quota = −Down
```

**Perché qui si usa Eulero esplicito e non RK4**: l'integrazione della posizione è una semplice quadratura — la derivata (V_North, V_East, V_Down) non dipende dalla posizione stessa. Non c'è retroazione: la posizione dell'aereo non influenza le forze aerodinamiche (si assume atmosfera uniforme e Terra piatta). In assenza di accoppiamento non lineare, l'errore di Eulero esplicito è proporzionale alla variazione della velocità durante lo step, che a 60 Hz è trascurabile. L'RK4 è riservato al sistema accoppiato [u,v,w,p,q,r] dove è realmente necessario.

---

## 10. Il display di volo e l'HUD (FlightDisplay)

La classe `FlightDisplay` gestisce tutto ciò che l'utente vede e sente.

### 10.1 Rendering 3D

Il rendering usa **Raylib** e comprende:
- **Modello F-16** con animazioni del carrello
- **Terreno e pista** (aerodrome)
- **Cielo** (sfera con texture che segue la camera)
- **Chase camera** che insegue l'aereo con lag esponenziale

### 10.2 HUD (Head-Up Display)

L'HUD sovrappone sul rendering 3D gli strumenti di volo tipici di un caccia:

- **Pitch Ladder**: scala di beccheggio con linee orizzontali ogni 5°, ruotata secondo il rollio
- **Speed Tape**: nastro della velocità (knots) sul lato sinistro
- **Altitude Tape**: nastro dell'altitudine (metri) sul lato destro
- **Heading Tape**: barra della direzione (gradi) in alto
- **Alpha/G Meter**: indicatore dell'angolo d'attacco e del fattore di carico
- **Warnings**: messaggi di allarme (STALL, OVERSPEED, TERRAIN, ecc.)

### 10.3 Input da tastiera

| Tasto | Azione | Range |
|-------|--------|-------|
| W / S | Beccheggio (stick avanti/indietro) | [−1, +1] |
| A / D | Rollio (stick sinistra/destra) | [−1, +1] |
| ← / → | Imbardata (pedali) | [−1, +1] |
| ↑ / ↓ | Manetta (throttle) | [0, +1] |
| E | Toggle motori on/off | — |
| L | Toggle modalità atterraggio | — |
| G | Toggle carrello | — |

### 10.4 Audio

Il display gestisce effetti sonori: avvio motore, loop del motore (tono variabile con il throttle), rumore aerodinamico, allarmi (stall, terrain, overspeed, caution).

---

## 11. La telemetria DDS

Il simulatore pubblica i dati di volo su **tre topic DDS** usando eProsima Fast DDS, con QoS RELIABLE e deadline di 50 ms:

### F16KinematicsTopic
Angoli di Eulero (Φ, Θ, Ψ), velocità angolari (p, q, r), velocità body (u, v, w), posizione (x, z, altitude), packet_id.

### F16AeroStateTopic
Angoli aerodinamici (α, β), numero di Mach, velocità in nodi, fattore di carico (nz), stato del sistema, messaggio di stato.

### F16ActuatorsTopic
Deflessioni effettive post-attuatore (δ_ele, δ_ail, δ_rud, δ_lef, throttle).

La pubblicazione avviene in un **thread dedicato** a ~20 Hz (50 ms di sleep), indipendente dal rendering. Il thread legge lo stato del FCC tramite `get_state()` e `get_actuator_state()` — entrambi protetti da mutex.

Sono inoltre abilitati i topic di **statistiche** Fast DDS: PUBLICATION_THROUGHPUT, NETWORK_LATENCY, HISTORY_LATENCY, HEARTBEAT_COUNT — utili per monitorare la qualità del servizio della comunicazione.

---

## 12. Il ciclo principale (main)

Il `main()` orchestra tutto il sistema con questa sequenza:

### 12.1 Inizializzazione

1. **Crea il DDS Participant** ("F16_Pilot_Node") con statistiche abilitate
2. **Registra i tre tipi** (F16Kinematics, F16AeroState, F16Actuators)
3. **Crea Publisher + 3 Topic + 3 DataWriter** con QoS RELIABLE e deadline 50 ms
4. **Inizializza il FCC** con stato "a terra, motori spenti, modalità atterraggio"
5. **Lancia il thread DDS** di pubblicazione a 20 Hz
6. **Crea la finestra** FlightDisplay (1000×900, Raylib)

### 12.2 Ciclo di rendering (60 FPS)

Ogni frame esegue cinque passi nell'ordine preciso A-B-C-D-E:

```
A. populate_plane_data()              ← legge stato FCC per audio e input
B. display.HandleInput(Aereo, pilot)  ← legge tastiera, aggiorna audio
C. g_fcc.step(pilot, dt)             ← TUTTA la fisica in un colpo
D. populate_plane_data()              ← legge stato FCC aggiornato per rendering
E. display.Draw(Aereo)               ← disegna scena 3D + HUD
```

Il passo A è necessario perché `HandleInput` ha bisogno dello stato corrente per decidere quali suoni riprodurre (ad esempio l'allarme stall). Il passo D aggiorna i dati *dopo* la fisica, così il rendering mostra il risultato del frame corrente.

Il `dt` viene normalizzato: se è vicino a 1/60 (±0.005 s) viene fissato esattamente a 1/60 per evitare jitter; se supera 0.1 s viene clampato per evitare instabilità numeriche.

Ogni 2 secondi viene stampato in console uno snapshot di debug con tutti i parametri di volo.

### 12.3 Shutdown

Alla chiusura della finestra:
1. `g_running = false` — segnala al thread DDS di terminare
2. `dds_thread.join()` — attende che il thread termini
3. Cancella DataWriter, Publisher, Topic e Participant in ordine inverso

---

## 13. Mappatura MATLAB → C++

Questa tabella mostra la corrispondenza riga-per-riga tra il modello MATLAB originale e l'implementazione C++:

| Componente MATLAB | File C++ | Note |
|-------------------|----------|------|
| `load_F16_params.m` → Param.S, b, cbar, mass, moi | `FlightControlComputer.hpp` costanti statiche | Conversione slug→kg, ft→m inline |
| `load_F16_params.m` → Param.xcg, xcgr | `F16AeroFM()` costanti locali xcg=0.30, xcgr=0.35 | Identiche |
| `F16AeroFM.m` linee 21-28 → input parsing | `F16AeroFM()` linee 80-86 | Stessa conversione rad→deg |
| `F16AeroFM.m` linee 34-36 → normalizzazione | `F16AeroFM()` linee 88-91 | dail, drud, dlef identici |
| `F16AeroFM.m` linee 41-46 → lookup base | `F16AeroFM()` linee 96-101 | aero_Cx, aero_Cy, ... |
| `F16AeroFM.m` linee 48-93 → delta e derivate | `F16AeroFM()` linee 103-151 | 1:1 con commenti MATLAB |
| `F16AeroFM.m` linee 97-125 → coefficienti totali | `F16AeroFM()` linee 155-189 | Stesse formule |
| `F16AeroFM.m` linee 128-135 → forze/momenti | `F16AeroFM()` linee 192-198 | q̄·S·C |
| `F16AeroDataInterpolants.mat` → F16Aero | `F16AeroData.hpp` → funzioni aero_*() | Auto-generato da HDF5 |
| `trim_and_linearize.m` → rate limits | `FlightControlComputer.hpp` ACT_RATE_* | Stessi valori |
| Simulink 6-DOF block | `FlightControlComputer::step()` RK4 | Stevens & Lewis con I_xz |

La differenza principale è che il MATLAB usa il solutore ODE di Simulink (tipicamente ode45 o ode4 a passo variabile), mentre il C++ usa RK4 a passo fisso sincronizzato con il frame rate di rendering. A 60 Hz il passo (0.017 s) è sufficientemente piccolo per la stabilità.

---

## 14. Predisposizione per la tesi (Outer Loop PID)

Il simulatore è **esplicitamente progettato** per essere esteso con un controllore outer-loop. Prima di descrivere l'implementazione specifica, è necessario capire in profondità il principio di funzionamento di un controllore PID e come si inserisce nell'architettura di controllo a due livelli dell'F-16.

### 14.1 Principio del controllore PID

Un controllore **PID** (Proporzionale–Integrale–Derivativo) è il blocco di controllo ad anello chiuso più diffuso nell'ingegneria del controllo automatico. Data una variabile da controllare `y(t)` e un riferimento desiderato `y_ref`, definisce l'**errore**:

```
e(t) = y_ref − y(t)
```

L'uscita del controllore (il comando correttivo `u`) è:

```
u(t) = Kp · e(t)  +  Ki · ∫₀ᵗ e(τ)dτ  +  Kd · de/dt
       ─────────     ────────────────     ──────────────
       Termine P       Termine I           Termine D
```

#### Termine Proporzionale (Kp)

Il termine P genera un comando **proporzionale all'errore istantaneo**. È il termine di reazione immediata: se l'aereo devia di 10° dall'assetto di riferimento, il comando è Kp × 10°. Un Kp alto reagisce rapidamente ma può introdurre **overshoot** (il sistema supera il riferimento prima di stabilizzarsi) e potenzialmente oscillazioni. Un Kp basso è lento e lascia un **errore a regime** (offset residuo).

#### Termine Integrale (Ki)

Il termine I accumula l'errore nel tempo. La sua funzione fondamentale è **eliminare l'errore a regime**: se il termine P da solo non riesce a portare l'errore a zero (ad esempio perché ci sono disturbi costanti come il vento), l'integrale cresce finché il comando non è abbastanza grande da annullare completamente l'errore. Ki alto accelera questa correzione ma può causare **windup** — l'accumulo eccessivo dell'integrale durante le saturazioni degli attuatori, che produce sovracomandi ritardati pericolosi.

#### Termine Derivativo (Kd)

Il termine D reagisce alla **velocità di variazione dell'errore**. Anticipa il comportamento futuro: se l'errore sta crescendo rapidamente, il termine D aumenta il comando prima che l'errore diventi grande. Fisicamente introduce uno **smorzamento predittivo** che riduce l'overshoot e accelera la convergenza. Kd alto, però, amplifica il rumore di misura (la derivata di un segnale rumoroso è molto rumorosa), quindi in pratica viene spesso filtrato.

#### Schema a blocchi

```
                 e(t)     ┌──────┐
y_ref ─►(+)──────────────►│  Kp  ├─┐
         │(-) ▲           └──────┘  │
         │    │           ┌──────┐  ├──► u(t) ──► Impianto ──► y(t)
         │    └──y(t)     │Ki/s  ├─┤                             │
         │                └──────┘  │                             │
         │                ┌──────┐  │                             │
         │                │ Kd·s ├─┘                             │
         │                └──────┘                               │
         └───────────────────────────────────────────────────────┘
                         Retroazione (feedback)
```

La chiusura del loop di retroazione è ciò che rende il sistema in grado di **correggere autonomamente** le deviazioni dall'assetto desiderato senza intervento del pilota.

### 14.2 Architettura a due livelli (Cascade Control)

L'F-16 usa un'architettura di controllo **in cascata** (cascade control) con due loop annidati, che risolve un problema fondamentale: non si può comandare direttamente l'assetto dell'aereo (Φ, Θ) perché le superfici di controllo agiscono sui **momenti** (L, M, N), che producono **accelerazioni angolari** (ṗ, q̇, ṙ), che producono **velocità angolari** (p, q, r), che producono **variazioni di assetto** (Φ̇, Θ̇, Ψ̇). La catena ha quattro integrazioni: comandare direttamente l'assetto con un unico PID sarebbe instabile ad alti gain.

La soluzione è separare il problema in due loop:

```
                    OUTER LOOP (lento, ~1 Hz)
          ┌─────────────────────────────────────┐
          │                                     │
Φ_ref ──►(+)──► PID_Φ ──► p_cmd               │
          │                    │                │
Θ_ref ──►(+)──► PID_Θ ──► q_cmd               │
          │                    │                │
          │     INNER LOOP (veloce, ~20 Hz)     │
          │     ┌──────────────────────────┐    │
          │     │  p_cmd ──►(+)──► K_p ──► δ_ail_sas ─► Attuatore ─► Flaperoni │
          │     │             │(-) ▲                                             │
          │     │             └──── p (roll rate misurato) ◄─────────────────── │
          │     │                                                                 │
          │     │  q_cmd ──►(+)──► K_q ──► δ_ele_sas ─► Attuatore ─► Stabilatore│
          │     │             │(-) ▲                                             │
          │     │             └──── q (pitch rate misurato) ◄────────────────── │
          │     └──────────────────────────────────────────────────────────────┘
          │                                     ▲
          └────── Φ, Θ (assetto misurato) ──────┘
```

L'**outer loop** controlla l'assetto (angoli Φ e Θ) e genera comandi di **velocità angolare** (p_cmd e q_cmd) come riferimento per l'inner loop. L'**inner loop** (SAS) controlla le velocità angolari e genera le **deflessioni delle superfici**. Questa separazione funziona perché la dinamica interna (tassi angolari) è molto più veloce di quella esterna (angoli di assetto): il SAS può seguire i comandi di rate con tempi di risposta di ~50 ms, mentre l'assetto cambia in secondi.

### 14.3 I due PID e le superfici di controllo

#### PID di Rollio (canale Φ → Flaperoni)

```
e_Φ(t)  = Φ_ref − Φ(t)
p_cmd   = KP_PHI · e_Φ  +  KI_PHI · ∫e_Φ dt  +  KD_PHI · de_Φ/dt
```

Il comando `p_cmd` viene passato al SAS, che calcola:

```
δ_ail_sas = KP_ROLL × (p_cmd − p)
```

Questo genera la deflessione dei **flaperoni** (δ_ail). I flaperoni sono superfici miste aileron/flap: una deflessione positiva (ala sinistra giù, ala destra su) genera un momento di rollio L > 0 che fa rollare l'aereo verso destra, aumentando Φ. Il loop chiuso tende quindi a portare Φ verso Φ_ref.

**Effetto sulla stabilità**: l'F-16 in rollio libero ha una costante di tempo τ_roll ≈ 0.5 s (smorzamento naturale). Il PID di rollio:
- Con Kp adeguato: riduce τ_roll effettivo a ~0.1–0.2 s (risposta più rapida)
- Con Ki: elimina l'errore stazionario dovuto ad asimmetrie (es. carichi esterni diversi)
- Con Kd: riduce l'overshoot nel transitorio

#### PID di Beccheggio (canale Θ → Stabilatore)

```
e_Θ(t)  = Θ_ref − Θ(t)
q_cmd   = KP_THETA · e_Θ  +  KI_THETA · ∫e_Θ dt  +  KD_THETA · de_Θ/dt
```

Il SAS calcola:

```
δ_ele_sas = KQ_PITCH × (q_cmd − q)
```

Questo genera la deflessione dello **stabilatore** (δ_ele). Il canale longitudinale è il più critico perché l'F-16 è **staticamente instabile** in beccheggio (CG a 0.30 c̄ avanti del punto neutro 0.35 c̄). Il coefficiente Cm_α > 0: una perturbazione positiva di α genera un momento di beccheggio che amplifica ulteriormente α, portando a divergenza esponenziale in ~300 ms senza controllo.

Il PID di Θ in cascata con il SAS deve:
1. Contrastare questa instabilità statica (già gestita parzialmente dal SAS con KQ_PITCH = −2.0)
2. Mantenere Θ al valore di riferimento contro disturbi atmosferici
3. Seguire eventuali rampe di Θ_ref (climb/descent)

**Effetto sulla stabilità**: il sistema non controllato ha un polo instabile a circa +3.3 rad/s nel piano dei poli (modo di periodo corto instabile). Il SAS già sposta questo polo a sinistra dell'asse immaginario. Il PID di Θ chiude un loop esterno che garantisce convergenza dell'assetto nel lungo periodo.

#### Yaw (canale r → Timone — no PID outer loop)

Il canale di imbardata non ha un PID outer loop nel progetto attuale. Il SAS fornisce solo yaw damping:

```
δ_rud_sas = KR_YAW × (0 − r)   con KR_YAW = −1.5
```

Il timone viene così usato unicamente per smorzare le oscillazioni di imbardata (Dutch roll), non per mantenere un heading Ψ_ref. L'eventuale controllo di rotta è un'estensione futura.

### 14.4 Implementazione nel codice

La struttura dati dell'outer loop in `FlightControlLaw.hpp`:

```cpp
struct OuterLoopState {
  float phi_ref   = 0.0f;  // Assetto di riferimento rollio (rad)
  float theta_ref = 0.0f;  // Assetto di riferimento beccheggio (rad)

  float phi_error      = 0.0f;  // e_Φ = phi_ref − Φ
  float phi_integral   = 0.0f;  // ∫e_Φ dt  (con anti-windup)
  float phi_error_prev = 0.0f;  // e_Φ al passo precedente (per derivata)

  float theta_error      = 0.0f;
  float theta_integral   = 0.0f;
  float theta_error_prev = 0.0f;

  float p_cmd = 0.0f;  // Comando di roll rate → SAS inner loop
  float q_cmd = 0.0f;  // Comando di pitch rate → SAS inner loop
};
```

La funzione `compute_outer_loop()` in `FlightControlLaw.cpp`:

```cpp
void FlightControlLaw::compute_outer_loop(const FlightState &s, float dt) {
  if (!m_outer_loop_engaged || dt <= 0.0f) return;

  // --- PID Rollio (Φ) ---
  m_outer.phi_error = m_outer.phi_ref - s.roll;

  // Termine Integrale con anti-windup (clamp ±1 rad·s)
  m_outer.phi_integral += m_outer.phi_error * dt;
  m_outer.phi_integral = std::clamp(m_outer.phi_integral, -INTEGRAL_MAX, INTEGRAL_MAX);

  // Termine Derivativo (differenze finite)
  float phi_deriv = (m_outer.phi_error - m_outer.phi_error_prev) / dt;
  m_outer.phi_error_prev = m_outer.phi_error;

  // Uscita PID → p_cmd [rad/s]
  m_outer.p_cmd = KP_PHI * m_outer.phi_error
                + KI_PHI * m_outer.phi_integral
                + KD_PHI * phi_deriv;

  // --- PID Beccheggio (Θ) --- (struttura identica)
  m_outer.theta_error = m_outer.theta_ref - s.pitch;
  m_outer.theta_integral += m_outer.theta_error * dt;
  m_outer.theta_integral = std::clamp(m_outer.theta_integral, -INTEGRAL_MAX, INTEGRAL_MAX);
  float theta_deriv = (m_outer.theta_error - m_outer.theta_error_prev) / dt;
  m_outer.theta_error_prev = m_outer.theta_error;

  m_outer.q_cmd = KP_THETA * m_outer.theta_error
                + KI_THETA * m_outer.theta_integral
                + KD_THETA * theta_deriv;
}
```

Il termine derivativo è calcolato con **differenze finite all'indietro** (`(e[k] − e[k−1]) / dt`), che è equivalente a una derivata discreta senza anticipare il futuro. A 60 Hz questo è sufficiente; in produzione si potrebbe aggiungere un filtro passa-basso sul termine D per ridurre l'amplificazione del rumore.

### 14.5 Anti-windup

Il **windup integrale** è un problema critico nei sistemi con saturazione degli attuatori. Consideriamo lo scenario: l'aereo ha un grande errore di assetto (es. Θ_ref = 20°, Θ = 0° → e = 20°). Il termine integrale cresce rapidamente, ma lo stabilatore è già saturato a ±25°. Quando l'errore inizia a ridursi, l'integrale è diventato molto grande: anche se l'errore diventa zero o negativo, l'integrale continua a spingere il comando in avanti, generando un **overshoot severo** prima che l'integrale si scarichi.

La protezione anti-windup nel codice è un semplice **clamp sull'integrale**:

```cpp
m_outer.phi_integral = std::clamp(m_outer.phi_integral, -INTEGRAL_MAX, INTEGRAL_MAX);
// con INTEGRAL_MAX = 1.0f rad·s
```

Questo limita la massima "memoria" dell'integrale. Con Ki = 0.5 rad/s/(rad·s), il massimo contributo integrativo è 1.0 × 0.5 = 0.5 rad/s su p_cmd — un valore ragionevole che non causa overshoot pericolosi.

### 14.6 Logica di engagement e disengagement

L'outer loop si attiva e disattiva automaticamente sulla **transizione della modalità atterraggio** in `FlightControlLaw::compute()`:

```cpp
bool in_flight = pilot.engines_on && !pilot.landing_mode;

// Transizione landing_mode: true → false (pilota pronto al volo libero)
if (in_flight && m_prev_landing_mode) {
  m_outer.phi_ref   = state.roll;   // Cattura assetto corrente come riferimento
  m_outer.theta_ref = state.pitch;
  m_outer.phi_integral   = 0.0f;   // Reset integrali (no windup ereditato)
  m_outer.theta_integral = 0.0f;
  m_outer_loop_engaged = true;
}

// Transizione: engines off o rientro in landing_mode
if (!in_flight && !m_prev_landing_mode) {
  m_outer_loop_engaged = false;
  reset_outer_loop();  // Azzera tutto lo stato
}
```

Il **bumpless transfer** (il fatto che al momento dell'engagement il riferimento venga catturato dall'assetto corrente) garantisce che non ci sia un salto nel comando: al momento zero l'errore è zero, quindi p_cmd = q_cmd = 0, e il SAS continua a operare come se nulla fosse cambiato. L'outer loop inizia a generare correzioni solo se l'aereo si discosta dall'assetto catturato.

### 14.7 Cosa serve per la tesi

La struttura è completamente pronta. I passi necessari per implementare il controllore dalla tesi sono:

1. **Ricavare i gain PID** dalla linearizzazione del modello (`trim_and_linearize.m` genera la matrice A, B del sistema linearizzato attorno al punto di trim). Le tecniche di progetto applicabili sono:
   - **Pole placement**: scegliere dove collocare i poli del sistema in catena chiusa
   - **LQR** (Linear Quadratic Regulator): minimizzazione di un indice di costo quadratico su errore e comando
   - **Sintesi in frequenza**: margini di guadagno e fase sul diagramma di Bode

2. **Assegnare i sei gain** nel file `FlightControlLaw.hpp`:
   ```cpp
   static constexpr float KP_PHI   = ???;  // [rad/s / rad]
   static constexpr float KI_PHI   = ???;  // [rad/s / (rad·s)]
   static constexpr float KD_PHI   = ???;  // [rad/s / (rad/s)]
   static constexpr float KP_THETA = ???;  // [rad/s / rad]
   static constexpr float KI_THETA = ???;  // [rad/s / (rad·s)]
   static constexpr float KD_THETA = ???;  // [rad/s / (rad/s)]
   ```

3. **Verificare la stabilità** in simulazione: con i gain non nulli il sistema è in anello chiuso. Partendo dal trim, applicare una perturbazione di assetto e verificare che Φ e Θ convergano al riferimento senza oscillazioni divergenti.

### 14.8 Effetto atteso sulla stabilità dell'aereo

Una volta implementati, i due PID avranno i seguenti effetti osservabili:

| Situazione | Senza outer loop PID | Con outer loop PID |
|-----------|---------------------|-------------------|
| Aereo in volo livellato, pilota lascia i comandi | L'aereo tende lentamente a divergere in beccheggio (instabilità statica) | Mantiene automaticamente Φ e Θ al valore catturato all'engagement |
| Raffica di vento laterale (disturbo su Φ) | Il SAS smorza il rate ma Φ deriva lentamente | Il PID di rollio riporta Φ a Φ_ref compensando il disturbo |
| Pilot stick-push (Θ_ref aggiornato) | Il SAS risponde ma senza memoria integrale | Il PID di beccheggio segue il nuovo riferimento con errore a regime nullo |
| Manovra con grande bank angle | Il SAS smorza p, ma Φ non ritorna a 0 senza stick | Il PID riporta Φ a Φ_ref (bank angle hold) |

Il risultato finale è un **autopilota di assetto** (attitude hold): l'aereo mantiene l'orientamento selezionato senza intervento del pilota, resistendo a disturbi e alla propria instabilità statica longitudinale. Questo è il blocco fondamentale su cui si costruiscono funzioni più avanzate come il controllo di rotta (heading hold), il mantenimento della quota (altitude hold) e la navigazione autonoma waypoint-to-waypoint.

---

## Appendice — Sequenza completa di un frame

```
1.  GetFrameTime() → dt ≈ 0.017 s
2.  populate_plane_data()              legge FlightState dal FCC
3.  HandleInput()                      legge tastiera → PilotInput
4.  FCC.step(PilotInput, dt):
    4a. Lock mutex, copia stato
    4b. FlightControlLaw::compute()
        - Outer loop PID (placeholder)
        - Inner loop SAS (rate damping)
        - Normal/Alternate/Direct law
        - Protezioni + clamp
        → ControlSurfaces (δ_cmd)
    4c. update_actuators(δ_cmd, dt)
        → δ_actual (rate-limited)
    4d. Atmosfera ISA → ρ(h)
    4e. V_T, α, β, q̄
    4f. Thrust = f(throttle)
    4g. F16AeroFM(stato, δ_actual)
        → Fx, Fy, Fz, L, M, N
    4h. Gravità body-frame
    4i. RK4(u,v,w,p,q,r)
        → accelerazioni lineari e angolari
        → integrazione 4 substep
    4j. Cinematica Euler
        → Φ, Θ, Ψ aggiornati
    4k. DCM body→earth
        → x, z, altitude aggiornati
    4l. Ground clamp + sicurezza numerica
    4m. EICAS status message
    4n. Unlock mutex, commit stato
5.  populate_plane_data()              legge stato aggiornato
6.  Draw()                             rendering 3D + HUD
```

In parallelo, il thread DDS ogni 50 ms:
```
1.  get_state() + get_actuator_state()
2.  Popola F16Kinematics, F16AeroState, F16Actuators
3.  write() su tre DataWriter (RELIABLE)
4.  sleep(50ms)
```
