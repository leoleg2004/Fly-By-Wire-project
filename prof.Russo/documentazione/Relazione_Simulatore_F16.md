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

Le equazioni del moto sono quelle del corpo rigido a sei gradi di libertà (three translational + three rotational) secondo la formulazione di **Stevens & Lewis, Eq. 1.4-4**.

### 8.1 Equazioni di forza (accelerazioni lineari)

```
m·u̇ = Fx_aero + Thrust + W_x + m·(r·v − q·w)
m·v̇ = Fy_aero +         W_y + m·(p·w − r·u)
m·ẇ = Fz_aero +         W_z + m·(q·u − p·v)
```

I termini `r·v − q·w`, `p·w − r·u`, `q·u − p·v` sono i **termini di Coriolis** dovuti alla rotazione del body frame.

### 8.2 Equazioni di momento (accelerazioni angolari)

L'F-16 ha un **prodotto d'inerzia I_xz non trascurabile** (1331 kg·m²) che accoppia i canali di rollio e imbardata. Le equazioni non possono essere semplificate come spesso si fa nei modelli didattici:

```
I_xx·ṗ + I_xz·ṙ = L + (I_yy − I_xx − I_zz)·p·q·I_xz + (I_xz² + I_zz·(I_zz − I_yy))·q·r
I_yy·q̇           = M − (I_xx − I_zz)·p·r − I_xz·(p² − r²)
I_xz·ṗ + I_zz·ṙ = N + (I_yy − I_xx − I_zz)·r·q·I_xz + (I_xz² + I_xx·(I_xx − I_yy))·p·q
```

Per risolvere il sistema accoppiato ṗ e ṙ, si usa la formula inversa:

```
denominatore = I_xx·I_zz − I_xz²

ṗ = [I_zz·L + I_xz·N − (...)·q·r] / denominatore
q̇ = [M − (I_xx − I_zz)·p·r − I_xz·(p² − r²)] / I_yy
ṙ = [I_xz·L + I_xx·N + (...)·p·q] / denominatore
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

f₄ = ṗ = [I_zz·L + I_xz·N − (I_xz·(I_yy−I_xx−I_zz)·p + (I_xz²+I_zz·(I_zz−I_yy)))·q·r] / Γ
f₅ = q̇ = [M − (I_xx−I_zz)·p·r − I_xz·(p²−r²)] / I_yy
f₆ = ṙ = [I_xz·L + I_xx·N + (I_xz·(I_yy−I_xx−I_zz)·r + (I_xz²+I_xx·(I_xx−I_yy)))·p·q] / Γ

dove  Γ = I_xx·I_zz − I_xz²
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

Nota: la formula per Ψ̇ ha una singolarità a Θ = ±90° (gimbal lock). Il codice protegge con `cos_th_safe = max(|cos(Θ)|, 0.001)`.

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

Il simulatore è **esplicitamente progettato** per essere esteso con un controllore outer-loop. Tutto è pronto:

### Cosa c'è già

- La struct `OuterLoopState` con tutti i campi PID (errori, integrali, derivate, comandi di rate)
- Il metodo `compute_outer_loop()` con la struttura PID completa (proporzionale + integrale + derivativo)
- Anti-windup con clamp sull'integrale (±1 rad·s)
- La logica di engagement/disengagement sulla transizione landing_mode
- Il SAS che già accetta `p_cmd` e `q_cmd` dall'outer loop come riferimento

### Cosa manca (da implementare per la tesi)

1. **Assegnare i gain PID** — attualmente tutti a zero:
   ```cpp
   static constexpr float KP_PHI   = ???;  // da tesi
   static constexpr float KI_PHI   = ???;
   static constexpr float KD_PHI   = ???;
   static constexpr float KP_THETA = ???;
   static constexpr float KI_THETA = ???;
   static constexpr float KD_THETA = ???;
   ```

2. **Generare i riferimenti** — attualmente `phi_ref` e `theta_ref` sono catturati al momento dell'engagement. Per un autopilota, dovrebbero venire da un path planner o dal pilota.

3. **Verifica stabilità** — con gain non nulli il loop è chiuso e va verificato che non diverga. Il modello linearizzato dal MATLAB (`trim_and_linearize.m`) può aiutare.

### Come attivarlo

Una volta assegnati i gain, l'outer loop si attiva automaticamente quando il pilota:
1. Accende i motori (tasto E)
2. Attende il warmup (20 s)
3. Disattiva la modalità atterraggio (tasto L)

In quel momento `m_outer_loop_engaged` diventa `true` e il PID inizia a generare `p_cmd` e `q_cmd` che il SAS usa come riferimento. L'aereo cercherà di mantenere l'assetto catturato al momento dell'attivazione.

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
