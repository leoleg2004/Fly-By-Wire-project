from vpython import *

# Parameters
A = 1.00        # Amplitude
phi = 0.0       # Phase angle
x0 = A          # Initial position
v0 = 0.0        # Initial velocity
m = 3.0         # Mass
k = 2.0         # Spring constant
omega = sqrt(k/m) # Angular frequency
dt = 0.1        # Time step
time_max = 1000 # Max time
ilen = 1.0      # Rest length of spring

# Function for Block-Spring Setup Generation
def construct(bheight, sprlen, block_color=vector(0,1,1)):
    block = box(
        pos = vector(0.5, bheight, 0),
        length = 0.3, width = 0.3, height = 0.3,
        color = block_color
    )
    
    spring = helix(
        pos = vector(block.pos.x-sprlen, block.pos.y, 0),
        axis = vector(1, 0, 0),
        length = sprlen,
        radius = block.width/3,
        thickness = 0.02,
        color = block_color
    )
    
    return {'block': block, 'spring': spring}

f1 = graph(title = "Position vs Time", xtitle = 'Time', ytitle = 'Pos_x')
exact_curve = gcurve(color = color.red)
euler_curve = gcurve(color = color.cyan)
implicit_curve = gcurve(color = color.orange)
rk2_curve = gcurve(color = color.purple)
rk4_curve = gcurve(color = color.green)

error_graph = graph(title = "Errors: Euler [cyan] vs Implicit [orange] vs RK2 [purple] vs RK4 [green]", xtitle = 'Time', ytitle = 'Error')
rk4_error = gcurve(color = color.green)
rk2_error = gcurve(color = color.purple)
euler_error = gcurve(color = color.cyan)
implicit_error = gcurve(color = color.orange)

scene = canvas(title="Exact [red] vs Euler [cyan] vs Implicit [orange] vs RK2 [purple] vs RK4 [green]") 

exact_setup = construct(1, ilen + A, block_color = color.red)
euler_setup = construct(0.5, ilen + A, block_color = color.cyan)
implicit_setup = construct(0, ilen + A, block_color = color.orange)
rk2_setup = construct(-0.5, ilen + A, block_color = color.purple)
rk4_setup = construct(-1, ilen + A, block_color = color.green)

# Spring Updater Function
def update_spring(pos, spring):
    spring.axis = pos - spring.pos
    spring.length = mag(spring.axis)

# Exact Solution
def exact_solution(t, A, omega, phi):
    x = A * cos(omega * t + phi)
    v = -A * omega * sin(omega * t + phi)
    return x, v

# Euler Method
def euler_step(x, v, dt, k, m):
    a = - (k / m) * x
    v_new = v + a * dt
    x_new = x + v * dt
    return x_new, v_new
    
# Implicit Euler Method
def implicit_euler_step(x, v, dt, k, m):
    A_imp = 1 + (dt**2 * k / m)
    x_new = (x + dt * v) / A_imp
    v_new = v + dt * (-k / m * x_new)
    return x_new, v_new
    
# RK2 Method 
def rk2_step(x, v, dt, k, m):
    def derivatives(x_val, v_val):
        return v_val, - (k / m) * x_val
    
    k1x, k1v = derivatives(x, v)
    k2x, k2v = derivatives(x + dt*k1x, v + dt*k1v)
    
    x_new = x + (dt / 2) * (k1x + k2x)
    v_new = v + (dt / 2) * (k1v + k2v)
    return x_new, v_new

# RK4 Method
def rk4_step(x, v, dt, k, m):
    def derivatives(x_val, v_val):
        return v_val, - (k / m) * x_val
    
    k1x, k1v = derivatives(x, v)
    k2x, k2v = derivatives(x + 0.5 * dt * k1x, v + 0.5 * dt * k1v)
    k3x, k3v = derivatives(x + 0.5 * dt * k2x, v + 0.5 * dt * k2v)
    k4x, k4v = derivatives(x + dt * k3x, v + dt * k3v)
    
    x_new = x + (dt / 6) * (k1x + 2 * k2x + 2 * k3x + k4x)
    v_new = v + (dt / 6) * (k1v + 2 * k2v + 2 * k3v + k4v)
    return x_new, v_new

# Set up initial conditions
x_euler, v_euler = x0, v0
x_implicit, v_implicit = x0, v0
x_rk2, v_rk2 = x0, v0
x_rk4, v_rk4 = x0, v0

# Interactivity
def set_dt(s):
    global dt
    print("Nuovo dt:", s.value)
    dt = s.value

dt_slider = slider(
    bind = set_dt,
    max = 0.5,
    min = 0.0001,
    step=0.0001,
    value = 0.1,
    length = 275
)

running = False

def run(b):
    global running
    running = not running
    if running: b.text = "Stop"
    else: b.text = "Run"
    
run_button = button(
    text = "Run",   
    pos = scene.title_anchor,
    bind = run,
)

# Run simulation
t = 0
while t < time_max:
    rate(60) 
    
    if running:
        x_euler, v_euler = euler_step(x_euler, v_euler, dt, k, m)
        x_implicit, v_implicit = implicit_euler_step(x_implicit, v_implicit, dt, k, m)
        x_rk2, v_rk2 = rk2_step(x_rk2, v_rk2, dt, k, m)
        x_rk4, v_rk4 = rk4_step(x_rk4, v_rk4, dt, k, m)
        
        exact_setup['block'].pos.x = exact_solution(t, A, omega, phi)[0]
        update_spring(exact_setup['block'].pos, exact_setup['spring'])
        
        euler_setup['block'].pos.x = x_euler
        update_spring(euler_setup['block'].pos, euler_setup['spring'])
        
        implicit_setup['block'].pos.x = x_implicit
        update_spring(implicit_setup['block'].pos, implicit_setup['spring'])
        
        rk2_setup['block'].pos.x = x_rk2
        update_spring(rk2_setup['block'].pos, rk2_setup['spring'])
        
        rk4_setup['block'].pos.x = x_rk4
        update_spring(rk4_setup['block'].pos, rk4_setup['spring'])
        
        exact_curve.plot(t, exact_setup['block'].pos.x)
        euler_curve.plot(t, euler_setup['block'].pos.x)
        implicit_curve.plot(t, implicit_setup['block'].pos.x)
        rk2_curve.plot(t, rk2_setup['block'].pos.x)
        rk4_curve.plot(t, rk4_setup['block'].pos.x)
         
        rk4_error.plot(t, abs(exact_setup['block'].pos.x - rk4_setup['block'].pos.x))
        rk2_error.plot(t, abs(exact_setup['block'].pos.x - rk2_setup['block'].pos.x))
            
        t += dt