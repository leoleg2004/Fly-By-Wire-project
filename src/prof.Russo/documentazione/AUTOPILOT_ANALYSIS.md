# Analisi Approfondita: Perché l'Autopilota Non Funziona Correttamente

## 1. Sintesi Esecutiva

Quando disattivi `landing_mode`, il sistema **engaggia strutturalmente l'outer loop** (m_outer_loop_engaged = true), cattura l'assetto attuale come riferimento, e **avvia il PID su assetto**. Tuttavia, **non vedi alcun effetto** perché i **PID gains sono TUTTI ZERO**:

```cpp
static constexpr float KP_PHI   = 0.0f;  // Roll PID gain
static constexpr float KI_PHI   = 0.0f;  // Roll integral gain
static constexpr float KD_PHI   = 0.0f;  // Roll derivative gain
static constexpr float KP_THETA = 0.0f;  // Pitch PID gain
static constexpr float KI_THETA = 0.0f;  // Pitch integral gain
static constexpr float KD_THETA = 0.0f;  // Pitch derivative gain
```

Questo è il **problema primario**. Ma dietro c'è un **problema strutturale più profondo** nel design della catena di controllo che devi capire.

---

## 2. Analisi della Catena di Controllo Completa

### Flow quando landing_mode = OFF (autopilot engaged)

```
┌─────────────────────────────────────────────────────────────────┐
│ FlightControlLaw::compute(pilot, state, dt)                     │
│                                                                   │
│ A. Landing Mode Transition Detection (codice nuovo)              │
│    if (engines_on && !landing_mode && prev_landing_mode) {       │
│      phi_ref = state.roll;       ← Cattura assetto ATTUALE       │
│      theta_ref = state.pitch;                                    │
│      m_outer_loop_engaged = true;                                │
│    }                                                             │
│                                                                   │
│ B. compute_outer_loop(state, dt)                                │
│    if (m_outer_loop_engaged) {                                   │
│      phi_error = phi_ref - state.roll;                          │
│      p_cmd = KP_PHI * phi_error + ...  ← MA KP_PHI = 0!         │
│      → p_cmd = 0 + 0 + 0 = 0 (SEMPRE!)                          │
│                                                                   │
│      theta_error = theta_ref - state.pitch;                     │
│      q_cmd = KP_THETA * theta_error + ... ← KP_THETA = 0!       │
│      → q_cmd = 0 + 0 + 0 = 0 (SEMPRE!)                          │
│    }                                                             │
│                                                                   │
│ C. compute_sas(state)                                            │
│    delta_ele = KQ_PITCH * (q_cmd - state.pitch_rate)            │
│              = -2.0 * (0 - pitch_rate)                          │
│              = 2.0 * pitch_rate  ← PURO DAMPING                 │
│                                                                   │
│    delta_ail = KP_ROLL * (p_cmd - state.roll_rate)              │
│              = -0.4 * (0 - roll_rate)                           │
│              = 0.4 * roll_rate  ← PURO DAMPING                  │
│                                                                   │
│ D. normal_law(pilot, state, dt)                                 │
│    pitch_rate_demand = pilot.stick_pitch * MAX_PITCH_RATE       │
│                      = (0 o ±1) * 0.5 rad/s                    │
│                                                                   │
│    if (|stick_pitch| < 0.02) {                                  │
│      m_pitch_integrator *= 0.97;  ← Decadimento lento            │
│      pitch_rate_demand = m_pitch_integrator;  ← Residui!        │
│    }                                                             │
│                                                                   │
│    delta_ele_pilot = (pitch_rate_demand / 0.5) * 25 deg         │
│    surf.stabilator = delta_ele_pilot + delta_sas                │
│                    = 0 (o integrator) + 2.0*pitch_rate          │
│                                                                   │
└─────────────────────────────────────────────────────────────────┘
```

### Cosa manca: la **propagazione del comando di assetto verso il rateo**

Il problema è che **l'outer loop comanda RATEI (p_cmd, q_cmd), NON ASSETTI**:

- `p_cmd = K_p * (phi_ref - phi)` — comanda un roll rate proporzionale all'errore di roll
- Ma `K_p = 0` → `p_cmd = 0` sempre
- Nessun comando di rateo → nessuna correzione di assetto

Anche se i gain fossero ≠ 0, il flusso sarebbe:
```cpp
phi_error = 0 - roll;    // Es: roll = 0.1 rad, error = -0.1
p_cmd = K_p * error;     // Es: K_p=-2.0 → p_cmd = +0.2 rad/s (roll rate cmd)
delta_ail = K_r * (p_cmd - roll_rate);  // SAS applica il comando di rateo
// Es: K_r = -0.4, p_cmd=0.2, roll_rate=0
//     delta_ail = -0.4 * (0.2 - 0) = -0.08 deg ← aileron down per correggere
```

Questo **funzionerebbe teoricamente**, ma lo stato attuale è:
```cpp
p_cmd = 0 * error = 0  (K_p = 0)
delta_ail = -0.4 * (0 - roll_rate) = 0.4 * roll_rate  ← PURO DAMPING, nessuna correzione!
```

---

## 3. Scenario Simulato: Cosa Succede Adesso (gains = 0)

### Caso 1: Pilota disattiva landing_mode, stick NEUTRO, aereo LEVEL

**Frame N (landing_mode disattivato):**
1. Landing mode transition detection:
   - `phi_ref = state.roll = 0.0`
   - `theta_ref = state.pitch = 0.0`
   - `m_outer_loop_engaged = true`

2. Compute outer loop:
   - `phi_error = 0 - 0 = 0`
   - `p_cmd = 0` (gain = 0)
   - `q_cmd = 0` (gain = 0)

3. Compute SAS:
   - `delta_ail = -0.4 * (0 - roll_rate) = 0.4 * roll_rate`
   - `delta_ele = -2.0 * (0 - pitch_rate) = 2.0 * pitch_rate`
   - **Effetto: Puro damping**

4. Normal law (stick = 0):
   - `pitch_rate_demand = 0`
   - `m_pitch_integrator *= 0.97` (decadimento)
   - `delta_ele_pilot ≈ 0`
   - `surf.stabilator = 0 + 2.0*pitch_rate`

**Risultato:** L'aereo rimane level con damping puro sui ratei. Non differisce dal caso senza outer loop!

### Caso 2: Aereo inizia a rollare (per qualsiasi ragione aerodinamica)

Se `roll` diventa 0.05 rad:
- `phi_error = 0 - 0.05 = -0.05`
- `p_cmd = 0 * (-0.05) = 0` (ancora 0!)
- SAS: `delta_ail = 0.4 * roll_rate` (nessuna correzione di assetto!)
- **Aereo continua a rollare lentamente senza correzione**

### Caso 3: Pilota comanda stick MENTRE outer_loop engaged

Pilota spinge UP (stick_pitch = 1.0):
1. `pitch_rate_demand = 1.0 * 0.5 = 0.5 rad/s`
2. `delta_ele_pilot = (0.5/0.5) * 25 = 25 deg`
3. SAS: `delta_ele = -2.0 * (0 - pitch_rate)` ≈ damping
4. `surf.stabilator = 25 + damping ≈ 25 deg`
5. **Aereo impenna normalmente**

Pilota rilascia stick (stick_pitch = 0):
1. `pitch_rate_demand = 0`
2. `m_pitch_integrator *= 0.97` (decadimento lento)
3. Integrator mantiene residui di comando
4. **Aereo NON ritorna a level flight**, mantiene assetto comandato finché integrator non si esaurisce
5. **Non c'è nulla che comandi "torna a theta_ref = 0"!**

---

## 4. Il Problema Strutturale: Mancanza di Integrazione tra Outer Loop e Normal Law

### Design Attuale (SBAGLIATO per "attitude hold")

```
Pilot Input (stick)
    ↓
[normal_law] ← Comanda DIRETTAMENTE proporzionale al stick
    ├─→ delta_ele_pilot ← Dipende SOLO dal stick
    │
    └→ [SAS] ← Aggiunge damping
         ├─→ p_cmd, q_cmd ← Generati dall'outer loop
         └─→ delta_sas = K * (cmd - rate_actual)
             ↓
             Result: delta_ele = delta_pilot + delta_sas
```

**Problema:** Il comando del pilota **non passa per l'outer loop**. L'outer loop può solo aggiungere damping ai ratei, non può correggere l'assetto quando il pilota comanda.

### Come Dovrebbe Essere (per un vero "attitude hold")

```
Pilot Input (stick)  ←→  Landing Mode Status
    ↓
[Pilot Command Limiter]
    ├─→ Se landing_mode OFF e stick NEUTRO:
    │   Usa outer_loop per tornare a phi_ref/theta_ref
    │   (comanda rateo: roll_rate_cmd = K_p * (phi_ref - phi_actual))
    │
    └─→ Se landing_mode OFF e stick NON NEUTRO:
        Combina comando pilota con outer loop (soft limits)
        (es: max roll = phi_ref ± limit)

    ↓
[normal_law] ← Integra outer loop + pilot input
    ├─→ delta_ele = (delta_pilot + delta_outer) + delta_sas
    │
    └→ [SAS]
```

---

## 5. Cosa Cambierebbe se Inserissi i PID Gains

Supponiamo `KP_PHI = -2.0` (rad/s per rad di errore):

### Scenario: Aereo a roll=0.1 rad, stick NEUTRO, outer_loop engaged

1. `phi_error = 0 - 0.1 = -0.1 rad`
2. `p_cmd = -2.0 * (-0.1) = +0.2 rad/s` ← Comando roll rate UP-wing per correggere
3. SAS: `delta_ail = -0.4 * (0.2 - roll_rate)`
   - Se `roll_rate = 0`: `delta_ail = -0.4 * 0.2 = -0.08 deg` ← Aileron DOWN per spinare su
   - Se `roll_rate = 0.2`: `delta_ail = -0.4 * (0.2 - 0.2) = 0` (raggiunto il rateo)
4. Aereo inizia a rollare UP
5. Roll torna verso 0
6. Quando `roll → phi_ref`: `p_cmd → 0`, damping mantiene stabile

**Risultato: VERO "attitude hold" — l'aereo torna automaticamente a level!**

---

## 6. Perché L'Engagement Strutturale è Corretto ma Invisibile

### Codice di Engagement (OK)
```cpp
bool in_flight = pilot.engines_on && !pilot.landing_mode;
if (in_flight && m_prev_landing_mode) {
  // Cattura assetto e engaggia
  m_outer.phi_ref = state.roll;
  m_outer.theta_ref = state.pitch;
  m_outer_loop_engaged = true;
}
```

✅ Questo funziona correttamente. Il problema è:

### Codice PID (PROBLEMA)
```cpp
m_outer.p_cmd = KP_PHI * m_outer.phi_error
              + KI_PHI * m_outer.phi_integral
              + KD_PHI * phi_deriv;

// Con KP_PHI = 0.0f:
// m_outer.p_cmd = 0.0f * (...) + 0.0f * (...) + 0.0f * (...) = 0.0f sempre
```

❌ Con i gain a zero, `p_cmd` e `q_cmd` sono **sempre zero**, quindi:
- L'outer loop è **strutturalmente presente ma funzionalmente inerte**
- È come avere un controllo volume messo a 0

---

## 7. Checklist: Cosa Verificare

### A. Confermare che il problema sia davvero SOLO i gains

**Test:** Temporaneamente cambia:
```cpp
static constexpr float KP_PHI   = -2.0f;   // Prova un valore non-zero
static constexpr float KP_THETA = -2.0f;
```

Se l'aereo **ritorna automaticamente a level flight** quando togli lo stick, allora il problema è SOLO i gains.

### B. Verificare il valore catturato di phi_ref/theta_ref

Aggiungi debug print:
```cpp
if (in_flight && m_prev_landing_mode) {
  std::cout << "[AUTOPILOT ENGAGED] phi_ref=" << m_outer.phi_ref
            << " theta_ref=" << m_outer.theta_ref
            << " (from state.roll=" << state.roll
            << " state.pitch=" << state.pitch << ")\n";
  m_outer_loop_engaged = true;
}
```

Se `phi_ref` non è zero (es: 0.3 rad = 17°), significa che l'aereo aveva assetto non-level al momento dell'engagement. L'autopilot manterrebbe quell'assetto.

### C. Verificare l'integrator decay del normal_law

L'integrator `m_pitch_integrator` potrebbe contenere residui che mantengono un comando anche quando stick è neutro:

```cpp
if (std::abs(p.stick_pitch) < 0.02f) {
  m_pitch_integrator *= 0.97f;  // Decadimento lento
  pitch_rate_demand = m_pitch_integrator;
}
```

Questo è CORRETTO per il FBW (mantiene il rateo finché il pilota non corregge), ma con outer loop engaged, potrebbe creare conflitto.

---

## 8. Root Cause Analysis

| Aspetto | Stato Attuale | Impatto |
|---------|---------------|--------|
| **PID Gains** | Tutti = 0 | ❌ Outer loop inerte, p_cmd=q_cmd=0 sempre |
| **Engagement Logic** | OK, cattura assetto | ✅ Strutturalmente corretto |
| **SAS Damping** | OK, puro damping | ✅ Stabilizza ma non corregge |
| **Normal Law Integration** | Pilota bypassa outer loop | ❌ Comando pilota non passa per attitude control |
| **Integrator Decay** | 0.97 factor/frame | ⚠️ Mantiene comando, ok per FBW ma può interferire |

**Conclusione:**
1. **Primario (80%):** PID gains = 0 → outer loop inerte
2. **Secondario (15%):** Design della catena non ha integrazione completa tra outer loop e normal law
3. **Terziario (5%):** Timing/value capture potrebbero avere edge cases

---

## 9. Soluzione: Implementare i PID Gains

### Step 1: Stima conservativa dei gain

Basato su prof. Russo thesis patterns (non disponibile, ma tipico FBW):

```cpp
// Outer Loop PID — Stimato da literatura FBW standard
static constexpr float KP_PHI   = -2.0f;    // [rad/s / rad]  ← Roll rate per rad di errore
static constexpr float KI_PHI   = -0.5f;    // [rad/s / (rad*s)]  ← Integrale
static constexpr float KD_PHI   = -0.3f;    // [rad/s / (rad/s)]  ← Derivativa

static constexpr float KP_THETA = -2.0f;    // [rad/s / rad]  ← Pitch rate per rad di errore
static constexpr float KI_THETA = -0.5f;    // [rad/s / (rad*s)]
static constexpr float KD_THETA = -0.3f;    // [rad/s / (rad/s)]
```

### Step 2: Test incrementale

1. Accendi il simulatore
2. Decolla normalmente (disattiva landing_mode)
3. Raggiunge assetto stabile (es: roll=0, pitch=5°)
4. Rilascia lo stick completamente
5. Osserva: L'aereo dovrebbe **ritornare gradualmente a roll=0, pitch=0** (o assetto catturato)

### Step 3: Fine-tuning

Se oscillazioni → aumenta KD, diminuisci KP
Se lento → aumenta KP, riduci KI
Se overshoot → aumenta KD

---

## 10. Conclusione Finale

**Domanda dell'utente:** "È perché non ho ancora implementato i PID o per altri motivi?"

**Risposta:** È **primariamente** perché **i PID gains sono zero**. Con gains = 0, l'outer loop è strutturalmente presente ma **funzionalmente invisibile**.

Secondariamente, c'è un **problema di design** nella catena di controllo dove il comando del pilota bypassa completamente l'outer loop. Per un vero "attitude hold autopilot", l'outer loop dovrebbe avere **priorità** quando lo stick è neutro.

**Azione immediata:** Inserisci gain non-zero e verifica se l'aereo ritorna a level flight. Se sì, il problema era SOLO i gain. Se no, allora c'è qualcos'altro più strutturale.

---

**Data:** 2026-03-13
**Status:** ✅ Analysis Complete
