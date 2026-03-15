"""
F-16  6-DOF  Inertial Coupling Demo
====================================
Integra le 6 equazioni di stato (u, v, w, p, q, r) con 4 metodi numerici:
  - Eulero Esplicito
  - Eulero Implicito (approssimato con Newton)
  - Runge-Kutta 2 (metodo di Heun)
  - Runge-Kutta 4

Confronto con la soluzione "esatta" (scipy solve_ivp, RK45, tolleranze 1e-12).

Equazioni di stato (Stevens & Lewis, con accoppiamento Ixz):
  u' = rv - qw + Fx/m
  v' = pw - ru + Fy/m
  w' = qu - pv + Fz/m
  p' = (Izz*L_eff + Ixz*N_eff) / Gamma
  q' = [M - (Ixx-Izz)*pr - Ixz*(p^2-r^2)] / Iyy
  r' = (Ixx*N_eff + Ixz*L_eff) / Gamma

Visualizzazione via vpython (browser).
"""

import numpy as np

# ─────────────────────────────────────────────────────────────────────────────
# Parametri inerziali F-16  (load_F16_params.m)
# ─────────────────────────────────────────────────────────────────────────────
mass  = 636.94
Ixx   = 9496.0
Iyy   = 55814.0
Izz   = 63100.0
Ixz   = 982.0
Gamma = Ixx * Izz - Ixz**2

# ─────────────────────────────────────────────────────────────────────────────
# Simulazione
# ─────────────────────────────────────────────────────────────────────────────
DT_DEFAULT = 1.0 / 60.0
T_END      = 55.0

IC = np.array([500.0, 0.0, 0.0,    # u, v, w  [ft/s]
               2.0,   0.5, 0.0])   # p, q, r  [rad/s]


def compute_aero(state):
    return 0.0, 0.0, 0.0, 0.0, 0.0, 0.0


def derivatives(state, Fx, Fy, Fz, L, M, N):
    u, v, w, p, q, r = state

    u_dot = r*v - q*w + Fx / mass
    v_dot = p*w - r*u + Fy / mass
    w_dot = q*u - p*v + Fz / mass

    q_dot = (M - (Ixx - Izz)*p*r - Ixz*(p**2 - r**2)) / Iyy

    L_eff = L - q*r*(Izz - Iyy) + Ixz*p*q
    N_eff = N + p*q*(Ixx - Iyy) - Ixz*q*r
    p_dot = (Izz*L_eff + Ixz*N_eff) / Gamma
    r_dot = (Ixx*N_eff + Ixz*L_eff) / Gamma

    return np.array([u_dot, v_dot, w_dot, p_dot, q_dot, r_dot])


def f(state):
    """Wrapper: derivatives con aero congelato (zero)."""
    Fx, Fy, Fz, L, M, N = compute_aero(state)
    return derivatives(state, Fx, Fy, Fz, L, M, N)


# ── Metodi numerici ────────────────────────────────────────────────────────

def euler_explicit_step(state, dt):
    return state + dt * f(state)


def euler_implicit_step(state, dt, n_iter=5):
    """Eulero implicito approssimato con iterazioni di punto fisso."""
    y_next = state + dt * f(state)  # stima iniziale (Eulero esplicito)
    for _ in range(n_iter):
        y_next = state + dt * f(y_next)
    return y_next


def rk2_step(state, dt):
    """Runge-Kutta 2 (metodo di Heun)."""
    k1 = f(state)
    k2 = f(state + dt * k1)
    return state + (dt / 2.0) * (k1 + k2)


def rk4_step(state, dt):
    """Runge-Kutta 4 classico."""
    k1 = f(state)
    k2 = f(state + 0.5*dt*k1)
    k3 = f(state + 0.5*dt*k2)
    k4 = f(state +     dt*k3)
    return state + (dt / 6.0) * (k1 + 2.0*k2 + 2.0*k3 + k4)


def kinetic_energy(state):
    u, v, w, p, q, r = state
    KE_t = 0.5 * mass * (u**2 + v**2 + w**2)
    KE_r = 0.5 * (Ixx*p**2 + Iyy*q**2 + Izz*r**2 - 2.0*Ixz*p*r)
    return KE_t, KE_r, KE_t + KE_r


# ── Soluzione esatta (RK4 ad alta precisione) ──────────────────────────

def compute_reference_solution(ic, t_end, n_pts):
    """Integra con RK4 a dt molto piccolo per avere la soluzione 'esatta'."""
    print("Calcolo soluzione esatta con RK4 ad alta precisione...")
    # Usa dt = 1/1000 del dt nominale per massima precisione
    dt_nominal = t_end / (n_pts - 1)
    dt_tiny = dt_nominal / 1000.0

    t_hist = [0.0]
    y_hist = [ic.copy()]
    y = ic.copy()
    t = 0.0
    step_count = 0

    while t < t_end - 1e-12:
        dt_step = min(dt_tiny, t_end - t)
        y = rk4_step(y, dt_step)
        t += dt_step
        step_count += 1
        # Salva ogni 1000 passi per allinearsi ai tempi richiesti
        if step_count % 1000 == 0 or t >= t_end - 1e-12:
            t_hist.append(t)
            y_hist.append(y.copy())

    return np.array(t_hist), np.array(y_hist).T  # shape: (6, len(t_hist))


# ── Quaternioni per l'assetto 3D ───────────────────────────────────────────

def quat_deriv(quat, p_rate, q_pitch, r_rate):
    qw, qx, qy, qz = quat
    ox, oy, oz = p_rate, -r_rate, q_pitch
    return 0.5 * np.array([
        -qx*ox - qy*oy - qz*oz,
         qw*ox + qy*oz - qz*oy,
         qw*oy + qz*ox - qx*oz,
         qw*oz + qx*oy - qy*ox
    ])


def quat_integrate(quat, p_rate, q_pitch, r_rate, dt):
    k1 = quat_deriv(quat,              p_rate, q_pitch, r_rate)
    k2 = quat_deriv(quat + 0.5*dt*k1, p_rate, q_pitch, r_rate)
    k3 = quat_deriv(quat + 0.5*dt*k2, p_rate, q_pitch, r_rate)
    k4 = quat_deriv(quat +     dt*k3, p_rate, q_pitch, r_rate)
    q_new = quat + (dt / 6.0) * (k1 + 2.0*k2 + 2.0*k3 + k4)
    return q_new / np.linalg.norm(q_new)


# ─────────────────────────────────────────────────────────────────────────────
if __name__ == "__main__":

    # Pre-calcola soluzione esatta
    print("Calcolo soluzione esatta (scipy)...")
    n_steps = int(T_END / DT_DEFAULT)
    n_pts   = n_steps + 1
    t_ref, y_ref = compute_reference_solution(IC, T_END, n_pts)
    print(f"Soluzione esatta calcolata: {n_pts} punti")

    from vpython import (canvas, graph, gcurve, color, rate, button, slider,
                         wtext, vector, box, cone, cylinder, compound, arrow,
                         label, sphere, attach_trail)

    def quat_to_vectors(q):
        w, x, y, z = q
        fx = 1 - 2*(y*y + z*z)
        fy = 2*(x*y + w*z)
        fz = 2*(x*z - w*y)
        ux = 2*(x*y - w*z)
        uy = 1 - 2*(x*x + z*z)
        uz = 2*(y*z + w*x)
        return vector(fx, fy, fz), vector(ux, uy, uz)

    # ── Scena 3D ───────────────────────────────────────────────────────────
    scene = canvas(
        title="<b>F-16  6-DOF</b> &mdash; Accoppiamento inerziale<br>"
              "Premi <b>Run</b> per avviare",
        width=900, height=450,
        center=vector(0, 0, 0),
        background=vector(0.05, 0.05, 0.15)
    )
    scene.camera.pos = vector(10, 6, 8)
    scene.camera.axis = vector(-10, -6, -8)

    ax_len = 6
    arrow(pos=vector(0,0,0), axis=vector(ax_len,0,0),
          color=color.red,   shaftwidth=0.06, headwidth=0.15)
    arrow(pos=vector(0,0,0), axis=vector(0,ax_len,0),
          color=color.green, shaftwidth=0.06, headwidth=0.15)
    arrow(pos=vector(0,0,0), axis=vector(0,0,ax_len),
          color=color.blue,  shaftwidth=0.06, headwidth=0.15)
    label(pos=vector(ax_len+0.3,0,0), text="X fwd",   height=10, border=4,
          color=color.red,   box=False)
    label(pos=vector(0,ax_len+0.3,0), text="Y up",    height=10, border=4,
          color=color.green, box=False)
    label(pos=vector(0,0,ax_len+0.3), text="Z right", height=10, border=4,
          color=color.blue,  box=False)

    grid_col = vector(0.15, 0.15, 0.25)
    for i in range(-8, 9, 2):
        cylinder(pos=vector(i, -3, -8), axis=vector(0,0,16),
                 radius=0.02, color=grid_col)
        cylinder(pos=vector(-8, -3, i), axis=vector(16,0,0),
                 radius=0.02, color=grid_col)

    # ── Modello F-16 realistico ───────────────────────────────────────────
    # Colori (f-16 grigio lucido con accenti scuri)
    fuselage_col = vector(0.55, 0.55, 0.58)
    wing_col     = vector(0.50, 0.50, 0.53)
    canopy_col   = vector(0.3, 0.5, 0.7)
    intake_col   = vector(0.25, 0.25, 0.28)
    nozzle_col   = vector(0.15, 0.15, 0.18)
    accent_col   = vector(1.0, 0.4, 0.1)

    # Fusoliera principale (più lungo e affusolato)
    fuselage = cylinder(pos=vector(-2.2, 0, 0), axis=vector(4.8, 0, 0),
                        radius=0.25, color=fuselage_col)

    # Muso affusolato (cone allungato)
    nose = cone(pos=vector(2.6, 0, 0), axis=vector(1.3, 0, 0),
                radius=0.25, color=vector(0.65, 0.65, 0.68))

    # Abitacolo (cockpit) - piccolo rialzo
    cockpit = cylinder(pos=vector(0.8, 0.15, 0), axis=vector(0.8, 0, 0),
                       radius=0.12, color=fuselage_col)

    # Canopy (vetratura trasparente)
    canopy = box(pos=vector(1.2, 0.22, 0), length=0.6, height=0.25, width=0.28,
                 color=canopy_col)

    # Presa d'aria ventrale (intake) - canonico F-16
    intake_main = box(pos=vector(0.3, -0.32, 0), length=1.2, height=0.18, width=0.4,
                      color=intake_col)
    intake_ramp = box(pos=vector(0.5, -0.25, 0), length=0.8, height=0.08, width=0.35,
                      color=intake_col)

    # Ali delta canoniche (riprese leggermente indietro)
    wing_L = box(pos=vector(-0.3, 0, -1.6), length=2.2, height=0.04, width=2.8,
                 color=wing_col)
    wing_R = box(pos=vector(-0.3, 0, 1.6),  length=2.2, height=0.04, width=2.8,
                 color=wing_col)

    # Strisce aerodinamiche (leading edge extensions - LEX)
    lex_L = box(pos=vector(-0.5, 0.12, -0.8), length=1.8, height=0.06, width=1.0,
                color=vector(0.48, 0.48, 0.51))
    lex_R = box(pos=vector(-0.5, 0.12, 0.8),  length=1.8, height=0.06, width=1.0,
                color=vector(0.48, 0.48, 0.51))

    # Deriva verticale (tall tail)
    vtail = box(pos=vector(-1.6, 0.95, 0), length=1.3, height=1.6, width=0.04,
                color=wing_col)

    # Piani di coda orizzontali (taileron)
    htail_L = box(pos=vector(-1.8, 0, -0.9), length=0.8, height=0.035, width=1.3,
                  color=wing_col)
    htail_R = box(pos=vector(-1.8, 0, 0.9),  length=0.8, height=0.035, width=1.3,
                  color=wing_col)

    # Motore (doppio, ma visualizzato come una presa unica)
    nozzle = cylinder(pos=vector(-2.2, -0.1, 0), axis=vector(0.5, 0, 0),
                      radius=0.38, color=nozzle_col)
    nozzle_afterburner = sphere(pos=vector(-1.7, -0.1, 0), radius=0.32,
                                color=vector(0.2, 0.2, 0.25))

    # Luci di navigazione
    light_wingtip_L = sphere(pos=vector(0.5, 0.05, -1.8), radius=0.06,
                             color=accent_col, emissive=True)
    light_wingtip_R = sphere(pos=vector(0.5, 0.05, 1.8), radius=0.06,
                             color=accent_col, emissive=True)
    light_tail = sphere(pos=vector(-2.2, -0.1, 0), radius=0.08,
                        color=vector(1, 0.1, 0.1), emissive=True)

    # Componi il modello
    airplane = compound([
        fuselage, nose, cockpit, canopy,
        intake_main, intake_ramp,
        wing_L, wing_R,
        lex_L, lex_R,
        vtail, htail_L, htail_R,
        nozzle, nozzle_afterburner,
        light_wingtip_L, light_wingtip_R, light_tail
    ])
    PLANE_LEN = 4.0
    attach_trail(airplane, color=vector(0.3, 1.0, 0.3), radius=0.03,
                 retain=400, pps=60)

    # ── Colori coerenti per i metodi ───────────────────────────────────────
    COL_EXACT  = color.black
    COL_EULER  = color.red
    COL_IMPL   = color.orange
    COL_RK2    = color.green
    COL_RK4    = color.cyan

    # ── Grafici stato: Integrazione Numerica vs Soluzione Esatta ───────────
    GW = 450
    GH = 280

    state_labels = [
        ("u [ft/s]", "t [s]", "u [ft/s]"),
        (" v [ft/s]", "t [s]", "v [ft/s]"),
        (" w [ft/s]", "t [s]", "w [ft/s]"),
        (" p [rad/s] roll",  "t [s]", "p [rad/s]"),
        (" q [rad/s] pitch", "t [s]", "q [rad/s]"),
        (" r [rad/s] yaw",   "t [s]", "r [rad/s]"),
    ]

    graphs   = []
    c_exact  = []
    c_euler  = []
    c_impl   = []
    c_rk2    = []
    c_rk4    = []

    for title, xt, yt in state_labels:
        align = "left" if len(graphs) % 3 != 2 else "right"
        g = graph(title=title, xtitle=xt, ytitle=yt,
                  width=GW, height=GH, align="left")
        graphs.append(g)
        c_exact.append(gcurve(graph=g, color=COL_EXACT, label="Esatta",         width=2))
        c_rk4.append(  gcurve(graph=g, color=COL_RK4,   label="RK4",            width=1))
        c_rk2.append(  gcurve(graph=g, color=COL_RK2,   label="RK2 (Heun)",     width=1))
        c_impl.append( gcurve(graph=g, color=COL_IMPL,  label="Eulero Implicito",width=1))
        c_euler.append(gcurve(graph=g, color=COL_EULER,  label="Eulero Esplicito",width=1))


    # ── Grafico KE drift ──────────────────────────────────────────────────
    g_ke = graph(title="Drift Energia Cinetica [%]",
                 xtitle="t [s]", ytitle="KE drift [%]",
                 width=GW, height=GH, align="left")
    cke_exact = gcurve(graph=g_ke, color=COL_EXACT, label="Esatta",           width=2)
    cke_rk4   = gcurve(graph=g_ke, color=COL_RK4,   label="RK4",             width=1)
    cke_rk2   = gcurve(graph=g_ke, color=COL_RK2,   label="RK2",             width=1)
    cke_impl  = gcurve(graph=g_ke, color=COL_IMPL,  label="Eulero Implicito",width=1)
    cke_euler = gcurve(graph=g_ke, color=COL_EULER,  label="Eulero Esplicito",width=1)

    # ── Controlli ──────────────────────────────────────────────────────────
    sim_dt  = DT_DEFAULT
    running = False

    def toggle_run(b):
        global running
        running = not running
        b.text = "Stop" if running else "Run"

    button(text="Run", bind=toggle_run)

    def set_dt(s):
        global sim_dt
        sim_dt = s.value

    slider(min=0.001, max=0.05, value=sim_dt, step=0.001,
           length=260, bind=set_dt)
    wtext(text="  dt")

    info = label(pos=vector(0, 4.5, 0), text="Premi Run per avviare",
                 height=14, border=6, color=color.white,
                 background=vector(0.1, 0.1, 0.2), box=True)

    # ── Pre-plot soluzione esatta (tutta in una volta) ─────────────────────
    for j in range(len(t_ref)):
        t_j = t_ref[j]
        for i in range(6):
            c_exact[i].plot(t_j, y_ref[i, j])

    KE0 = kinetic_energy(IC)[2]
    for j in range(len(t_ref)):
        ke_j = kinetic_energy(y_ref[:, j])[2]
        cke_exact.plot(t_ref[j], (ke_j - KE0) / KE0 * 100.0)

    # ── Loop di simulazione ────────────────────────────────────────────────
    s_euler_e = IC.copy()
    s_euler_i = IC.copy()
    s_rk2     = IC.copy()
    s_rk4     = IC.copy()
    t         = 0.0
    quat      = np.array([1.0, 0.0, 0.0, 0.0])
    step_idx  = 0

    while t < T_END:
        rate(60)
        if not running:
            continue

        # ── Integrazione ───────────────────────────────────────────────
        s_euler_e = euler_explicit_step(s_euler_e, sim_dt)
        s_euler_i = euler_implicit_step(s_euler_i, sim_dt)
        s_rk2     = rk2_step(s_rk2, sim_dt)
        s_rk4     = rk4_step(s_rk4, sim_dt)
        t        += sim_dt
        step_idx += 1

        # ── Quaternione (da RK4) ───────────────────────────────────────
        p_r, q_p, r_r = s_rk4[3], s_rk4[4], s_rk4[5]
        quat = quat_integrate(quat, p_r, q_p, r_r, sim_dt)

        fwd, up = quat_to_vectors(quat)
        airplane.axis = fwd * PLANE_LEN
        airplane.up   = up

        # ── Soluzione esatta al tempo corrente (interpolazione) ────────
        ref_idx = min(step_idx, len(t_ref) - 1)
        ref_now = y_ref[:, ref_idx]

        # ── Grafici stato ──────────────────────────────────────────────
        for i in range(6):
            c_rk4[i].plot(t, s_rk4[i])
            c_rk2[i].plot(t, s_rk2[i])
            c_impl[i].plot(t, s_euler_i[i])
            c_euler[i].plot(t, s_euler_e[i])


        # ── KE drift ──────────────────────────────────────────────────
        cke_rk4.plot(t,   (kinetic_energy(s_rk4)[2]     - KE0) / KE0 * 100.0)
        cke_rk2.plot(t,   (kinetic_energy(s_rk2)[2]     - KE0) / KE0 * 100.0)
        cke_impl.plot(t,  (kinetic_energy(s_euler_i)[2]  - KE0) / KE0 * 100.0)
        cke_euler.plot(t, (kinetic_energy(s_euler_e)[2]  - KE0) / KE0 * 100.0)

        # ── Info ───────────────────────────────────────────────────────
        drift_rk4 = (kinetic_energy(s_rk4)[2] - KE0) / KE0 * 100.0
        info.text = (
            f"t = {t:.2f} s   dt = {sim_dt:.4f}\n"
            f"RK4:  u={s_rk4[0]:.1f}  v={s_rk4[1]:.1f}  w={s_rk4[2]:.1f}\n"
            f"      p={p_r:.3f}  q={q_p:.3f}  r={r_r:.3f}\n"
            f"KE drift RK4: {drift_rk4:.2e} %"
        )

    info.text = (
        f"COMPLETATA  t = {t:.2f} s\n"
        f"RK4   KE drift: {(kinetic_energy(s_rk4)[2] - KE0) / KE0 * 100:.2e} %\n"
        f"RK2   KE drift: {(kinetic_energy(s_rk2)[2] - KE0) / KE0 * 100:.2e} %\n"
        f"Impl  KE drift: {(kinetic_energy(s_euler_i)[2] - KE0) / KE0 * 100:.2e} %\n"
        f"Euler KE drift: {(kinetic_energy(s_euler_e)[2] - KE0) / KE0 * 100:.2e} %"
    )
    print("Simulazione completata.")
