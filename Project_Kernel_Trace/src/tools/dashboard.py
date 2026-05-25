#!/usr/bin/env python3
import os
import time
import subprocess
import threading
import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox, simpledialog
from PIL import Image, ImageTk
import shutil
import signal
import uuid

# Get paths
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, '..', '..'))
BIN_TOOLS_DIR = os.path.join(PROJECT_ROOT, 'bin', 'tools')
BIN_TEST_DIR = os.path.join(PROJECT_ROOT, 'bin', 'Test')
BIN_APP_DIR = os.path.join(PROJECT_ROOT, 'bin', 'app')

# Gray Theme Colors
BG_COLOR = "#f3f3f3"
FG_COLOR = "#2d2d2d"
PANEL_BG = "#ffffff"
ACCENT_COLOR = "#0066cc"
HOVER_COLOR = "#0052a3"
BUTTON_BG = "#e0e0e0"
BUTTON_FG = "#333333"
TEXT_BG = "#ffffff"
CONSOLE_BG = "#1e1e1e"
CONSOLE_FG = "#00ff00"

class KernelTraceDashboard(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Kernel Trace Dashboard")
        self.geometry("1200x800")
        self.configure(bg=BG_COLOR)
        
        self.current_process = None
        self.sudo_password = None
        self.running_script_name = None
        self.running_apps = {} # Maps display_name -> {"name": app_name, "process": Popen, "sudo": bool}
        
        self.setup_styles()
        
        # Main container
        main_frame = tk.Frame(self, bg=BG_COLOR)
        main_frame.pack(fill="both", expand=True, padx=10, pady=10)
        
        self.create_header(main_frame)
        
        # Notebook for Tabs
        self.notebook = ttk.Notebook(main_frame)
        self.notebook.pack(fill="both", expand=True, pady=(10, 0))
        
        # Tabs
        self.tab_scripts = tk.Frame(self.notebook, bg=BG_COLOR)
        self.tab_dds = tk.Frame(self.notebook, bg=BG_COLOR)
        self.tab_traces = tk.Frame(self.notebook, bg=BG_COLOR)
        
        self.notebook.add(self.tab_scripts, text="  Scripts Manager (Single)  ")
        self.notebook.add(self.tab_dds, text="  DDS Studio (Multi)  ")
        self.notebook.add(self.tab_traces, text="  Trace Manager  ")
        
        # Setup Tabs content
        self.setup_scripts_tab()
        self.setup_dds_studio_tab()
        self.setup_traces_tab()
        self.refresh_apps()
        
    def setup_styles(self):
        style = ttk.Style(self)
        style.theme_use('clam')
        
        # Global configuration
        style.configure(".", background=BG_COLOR, foreground=FG_COLOR, font=("Helvetica", 10))
        
        # Notebook (Tabs) style
        style.configure("TNotebook", background=BG_COLOR, borderwidth=0)
        style.configure("TNotebook.Tab", background="#dcdcdc", foreground=FG_COLOR, padding=[15, 5], font=("Helvetica", 11, "bold"), borderwidth=1, bordercolor="#cccccc")
        style.map("TNotebook.Tab", background=[("selected", PANEL_BG)], foreground=[("selected", ACCENT_COLOR)])
        
        # Frames
        style.configure("TFrame", background=BG_COLOR)
        style.configure("Card.TFrame", background=PANEL_BG, borderwidth=1, relief="solid", bordercolor="#cccccc")
        
        # LabelFrames
        style.configure("TLabelframe", background=PANEL_BG, foreground=ACCENT_COLOR, font=("Helvetica", 12, "bold"), borderwidth=1, bordercolor="#cccccc")
        style.configure("TLabelframe.Label", background=PANEL_BG, foreground=ACCENT_COLOR, font=("Helvetica", 12, "bold"))
        
        # Labels
        style.configure("TLabel", background=BG_COLOR, foreground=FG_COLOR)
        style.configure("Panel.TLabel", background=PANEL_BG, foreground=FG_COLOR)
        style.configure("Header.TLabel", background=BG_COLOR, foreground=ACCENT_COLOR, font=("Helvetica", 24, "bold"))
        style.configure("SubHeader.TLabel", background=PANEL_BG, foreground=ACCENT_COLOR, font=("Helvetica", 12, "bold"))
        
        # Buttons
        style.configure("TButton", background=BUTTON_BG, foreground=BUTTON_FG, font=("Helvetica", 10, "bold"), padding=6, borderwidth=1, bordercolor="#bbbbbb")
        style.map("TButton", background=[("active", "#d0d0d0"), ("disabled", "#f0f0f0")], foreground=[("disabled", "#aaaaaa")])
        
        style.configure("Stop.TButton", foreground="#cc0000")
        
        # Checkbuttons
        style.configure("TCheckbutton", background=PANEL_BG, font=("Helvetica", 10, "bold"))
        
        # Entries
        style.configure("TEntry", fieldbackground=TEXT_BG, foreground=FG_COLOR, insertcolor=FG_COLOR, borderwidth=1)
        style.configure("TCombobox", fieldbackground=TEXT_BG, foreground=FG_COLOR)

    def create_header(self, parent):
        header_frame = tk.Frame(parent, bg=BG_COLOR)
        header_frame.pack(fill="x", pady=(0, 10))
        
        logo_path = os.path.join(SCRIPT_DIR, "logo.png")
        if os.path.exists(logo_path):
            try:
                img = Image.open(logo_path)
                img.thumbnail((80, 80))
                self.logo_img = ImageTk.PhotoImage(img)
                lbl = tk.Label(header_frame, image=self.logo_img, bg=BG_COLOR)
                lbl.pack(side="left", padx=(0, 20))
            except Exception as e:
                pass
                
        title_lbl = ttk.Label(header_frame, text="Kernel Trace Unified Dashboard", style="Header.TLabel")
        title_lbl.pack(side="left")

    # ==========================================
    # SCRIPTS TAB
    # ==========================================

    def setup_scripts_tab(self):
        self.tab_scripts.columnconfigure(0, weight=1)
        self.tab_scripts.rowconfigure(0, weight=1)
        
        # --- Single Process Script Execution ---
        phase2_frame = ttk.LabelFrame(self.tab_scripts, text="Active Tracing (Single Process)", style="TLabelframe")
        phase2_frame.grid(row=0, column=0, sticky="nsew", padx=10, pady=10)
        phase2_frame.columnconfigure(1, weight=1)
        phase2_frame.rowconfigure(0, weight=1)
        
        # Left Panel - Script List
        left_panel = tk.Frame(phase2_frame, bg=PANEL_BG)
        left_panel.grid(row=0, column=0, sticky="ns", padx=10, pady=10)
        
        self.script_listbox_single = tk.Listbox(left_panel, width=30, font=("Helvetica", 11), bg=TEXT_BG, fg=FG_COLOR, selectbackground=ACCENT_COLOR, selectforeground="white", highlightthickness=1, highlightbackground="#cccccc", relief="flat")
        self.script_listbox_single.pack(fill="both", expand=True, pady=(0, 10))
        self.script_listbox_single.bind("<<ListboxSelect>>", self.on_script_select_single)
        
        ttk.Button(left_panel, text="Refresh Scripts List", command=self.refresh_scripts).pack(fill="x")
        
        # Right Panel - Script Config
        right_panel = tk.Frame(phase2_frame, bg=PANEL_BG)
        right_panel.grid(row=0, column=1, sticky="nsew", padx=(0, 10), pady=10)
        right_panel.columnconfigure(1, weight=1)
        right_panel.rowconfigure(3, weight=1) # console grows
        
        ttk.Label(right_panel, text="Selected Script:", style="Panel.TLabel").grid(row=0, column=0, sticky="nw", pady=5)
        self.selected_script_var_single = tk.StringVar(value="None")
        ttk.Label(right_panel, textvariable=self.selected_script_var_single, font=("Helvetica", 13, "bold"), background=PANEL_BG, foreground=FG_COLOR).grid(row=0, column=1, sticky="w", padx=10, pady=5)
        
        # Single-Process List
        ttk.Label(right_panel, text="Target Process to Trace:", style="Panel.TLabel").grid(row=1, column=0, sticky="nw", pady=5)
        
        proc_frame = tk.Frame(right_panel, bg=PANEL_BG)
        proc_frame.grid(row=1, column=1, sticky="ew", padx=10, pady=5)
        proc_frame.columnconfigure(0, weight=1)
        
        self.single_proc_combobox = ttk.Combobox(proc_frame, font=("Helvetica", 11), state="normal")
        self.single_proc_combobox.grid(row=0, column=0, sticky="ew")
        
        # Checkbox & Run Buttons
        controls_frame = tk.Frame(right_panel, bg=PANEL_BG)
        controls_frame.grid(row=2, column=0, columnspan=2, sticky="ew", pady=10)
        
        self.run_sudo_var_single = tk.BooleanVar(value=False)
        ttk.Checkbutton(controls_frame, text="Run as Root", variable=self.run_sudo_var_single).pack(side="left", padx=10)
        
        self.run_bg_btn_single = ttk.Button(controls_frame, text="▶ Run in Background", command=self.run_in_background_single, state="disabled")
        self.run_bg_btn_single.pack(side="left", padx=(10, 10))
        
        self.run_term_btn_single = ttk.Button(controls_frame, text="▶ Run in Terminal", command=self.run_in_terminal_single, state="disabled")
        self.run_term_btn_single.pack(side="left", padx=(0, 10))
        
        self.stop_btn_single = ttk.Button(controls_frame, text="🛑 Stop Trace", command=self.stop_process, state="disabled", style="Stop.TButton")
        self.stop_btn_single.pack(side="right")
        
        # Console
        console_frame = tk.Frame(right_panel, bg=PANEL_BG)
        console_frame.grid(row=3, column=0, columnspan=2, sticky="nsew", pady=(10, 0))
        console_frame.columnconfigure(0, weight=1)
        console_frame.rowconfigure(1, weight=1)
        
        header = tk.Frame(console_frame, bg=PANEL_BG)
        header.grid(row=0, column=0, sticky="ew", pady=(0, 5))
        header.columnconfigure(0, weight=1)
        ttk.Label(header, text="Console Output", style="SubHeader.TLabel").grid(row=0, column=0, sticky="w")
        ttk.Button(header, text="Clear", command=lambda: self.log_text_single.delete(1.0, tk.END)).grid(row=0, column=1, sticky="e")
        
        self.log_text_single = scrolledtext.ScrolledText(console_frame, bg=CONSOLE_BG, fg=CONSOLE_FG, font=("Consolas", 10), insertbackground="white", relief="flat", highlightthickness=1, highlightbackground="#cccccc")
        self.log_text_single.grid(row=1, column=0, sticky="nsew")

    def setup_dds_studio_tab(self):
        self.tab_dds.columnconfigure(0, weight=1)
        self.tab_dds.rowconfigure(1, weight=1)
        
        # --- Step 1: Select Targets & Launch ---
        step1_frame = ttk.LabelFrame(self.tab_dds, text="Step 1: Select Targets & Launch", style="TLabelframe")
        step1_frame.grid(row=0, column=0, sticky="ew", padx=10, pady=(10, 5))
        
        proc_frame = tk.Frame(step1_frame, bg=PANEL_BG)
        proc_frame.pack(fill="x", padx=10, pady=10)
        
        ttk.Label(proc_frame, text="Target Processes:", style="Panel.TLabel").grid(row=0, column=0, sticky="nw", pady=5)
        
        self.process_listbox = tk.Listbox(proc_frame, height=3, font=("Helvetica", 11), bg=TEXT_BG, fg=FG_COLOR, selectbackground=ACCENT_COLOR, highlightthickness=1, highlightbackground="#cccccc", relief="flat")
        self.process_listbox.grid(row=0, column=1, columnspan=3, sticky="ew", pady=(0, 5), padx=(10,0))
        
        self.add_proc_combobox = ttk.Combobox(proc_frame, font=("Helvetica", 11), state="normal")
        self.add_proc_combobox.grid(row=1, column=1, sticky="ew", padx=(10,0))
        self.add_proc_combobox.bind("<Return>", lambda e: self.add_process())
        
        ttk.Button(proc_frame, text="+ Add Target", command=self.add_process, width=12).grid(row=1, column=2, padx=(5,0))
        ttk.Button(proc_frame, text="- Remove", command=self.remove_process, width=8).grid(row=1, column=3, padx=(5,0))
        
        self.launch_sudo_var = tk.BooleanVar(value=True)
        ttk.Checkbutton(proc_frame, text="Run as Root", variable=self.launch_sudo_var).grid(row=2, column=1, sticky="w", padx=(10, 0), pady=(5,0))
        
        ttk.Button(proc_frame, text="🚀 Launch Selected", command=self.launch_selected_terminal).grid(row=2, column=2, padx=(5,0), sticky="e", pady=(5,0))
        ttk.Button(proc_frame, text="🚀 Launch ALL", command=self.launch_all_terminals).grid(row=2, column=3, padx=(5,0), sticky="e", pady=(5,0))

        # --- Step 2: Trace Scripts ---
        step2_frame = ttk.LabelFrame(self.tab_dds, text="Step 2: Trace Scripts", style="TLabelframe")
        step2_frame.grid(row=1, column=0, sticky="nsew", padx=10, pady=(5, 10))
        step2_frame.columnconfigure(1, weight=1)
        step2_frame.rowconfigure(2, weight=1) # console grows
        
        # Left Panel - Script List
        left_panel = tk.Frame(step2_frame, bg=PANEL_BG)
        left_panel.grid(row=0, column=0, rowspan=3, sticky="ns", padx=10, pady=10)
        
        self.script_listbox_dds = tk.Listbox(left_panel, width=30, font=("Helvetica", 11), bg=TEXT_BG, fg=FG_COLOR, selectbackground=ACCENT_COLOR, selectforeground="white", highlightthickness=1, highlightbackground="#cccccc", relief="flat")
        self.script_listbox_dds.pack(fill="both", expand=True, pady=(0, 10))
        self.script_listbox_dds.bind("<<ListboxSelect>>", self.on_script_select_dds)
        
        ttk.Button(left_panel, text="Refresh Scripts List", command=self.refresh_scripts).pack(fill="x")
        
        # Right Panel - Script Config
        right_panel = tk.Frame(step2_frame, bg=PANEL_BG)
        right_panel.grid(row=0, column=1, sticky="nsew", padx=(0, 10), pady=10)
        right_panel.columnconfigure(1, weight=1)
        
        ttk.Label(right_panel, text="Selected Script:", style="Panel.TLabel").grid(row=0, column=0, sticky="nw", pady=5)
        self.selected_script_var_dds = tk.StringVar(value="None")
        ttk.Label(right_panel, textvariable=self.selected_script_var_dds, font=("Helvetica", 13, "bold"), background=PANEL_BG, foreground=FG_COLOR).grid(row=0, column=1, sticky="w", padx=10, pady=5)
        
        # Checkbox & Run Buttons
        controls_frame = tk.Frame(step2_frame, bg=PANEL_BG) 
        controls_frame.grid(row=1, column=1, sticky="ew", padx=(0,10), pady=10)
        
        self.run_sudo_var_dds = tk.BooleanVar(value=False)
        ttk.Checkbutton(controls_frame, text="Run as Root", variable=self.run_sudo_var_dds).pack(side="left", padx=(0,10))
        
        self.run_bg_btn_dds = ttk.Button(controls_frame, text="▶ Run Script in Background", command=self.run_in_background_dds, state="disabled")
        self.run_bg_btn_dds.pack(side="left", padx=(0, 10))
        
        self.run_term_btn_dds = ttk.Button(controls_frame, text="▶ Run in Terminal", command=self.run_in_terminal_dds, state="disabled")
        self.run_term_btn_dds.pack(side="left", padx=(0, 10))
        
        self.stop_btn_dds = ttk.Button(controls_frame, text="🛑 Stop Trace Script", command=self.stop_process, state="disabled", style="Stop.TButton")
        self.stop_btn_dds.pack(side="right")
        
        # Console
        console_frame = tk.Frame(step2_frame, bg=PANEL_BG)
        console_frame.grid(row=2, column=1, sticky="nsew", padx=(0,10), pady=(0, 10))
        console_frame.columnconfigure(0, weight=1)
        console_frame.rowconfigure(1, weight=1)
        
        header = tk.Frame(console_frame, bg=PANEL_BG)
        header.grid(row=0, column=0, sticky="ew", pady=(0, 5))
        header.columnconfigure(0, weight=1)
        ttk.Label(header, text="Console Output", style="SubHeader.TLabel").grid(row=0, column=0, sticky="w")
        ttk.Button(header, text="Clear", command=lambda: self.log_text_dds.delete(1.0, tk.END)).grid(row=0, column=1, sticky="e")
        
        self.log_text_dds = scrolledtext.ScrolledText(console_frame, bg=CONSOLE_BG, fg=CONSOLE_FG, font=("Consolas", 10), insertbackground="white", relief="flat", highlightthickness=1, highlightbackground="#cccccc")
        self.log_text_dds.grid(row=1, column=0, sticky="nsew")

    def refresh_apps(self):
        apps = []
        if os.path.exists(BIN_APP_DIR):
            for f in os.listdir(BIN_APP_DIR):
                if os.path.isfile(os.path.join(BIN_APP_DIR, f)) and os.access(os.path.join(BIN_APP_DIR, f), os.X_OK):
                    apps.append(f)
        apps.sort()
        self.single_proc_combobox['values'] = apps
        self.add_proc_combobox['values'] = apps
        if apps:
            self.single_proc_combobox.set(apps[0])
            self.add_proc_combobox.set(apps[0])

    def add_process(self):
        val = self.add_proc_combobox.get().strip()
        if val:
            self.process_listbox.insert(tk.END, val)

    def remove_process(self):
        sel = self.process_listbox.curselection()
        if sel:
            self.process_listbox.delete(sel[0])

    def launch_selected_terminal(self):
        selection = self.process_listbox.curselection()
        if not selection:
            messagebox.showwarning("No Selection", "Please select a target process from the list to launch.")
            return
            
        val = self.process_listbox.get(selection[0])
        use_sudo = self.launch_sudo_var.get()
        if use_sudo and not self.sudo_password:
            pwd = simpledialog.askstring("Sudo Required", "Enter your sudo password to run applications as root:\\n(It will not be stored)", show='*')
            if pwd is None: return
            self.sudo_password = pwd
            
        app_path = os.path.join(BIN_APP_DIR, val)
        if not os.path.exists(app_path):
            self.log_message(f"--- Executable {val} not found in bin/app/ ---\n")
            return
            
        if use_sudo:
            cmd = f"sudo -S bash -c '{app_path} < /dev/tty'; echo; echo \"Process finished. Press Enter to close.\"; read < /dev/tty"
            cmd_to_run = f"echo '{self.sudo_password}' | {{ {cmd}; }}"
        else:
            cmd = f"{app_path}; echo; echo \"Process finished. Press Enter to close.\"; read"
            cmd_to_run = cmd
            
        terminal_cmd = []
        if shutil.which("gnome-terminal"):
            terminal_cmd = ["gnome-terminal", "--", "bash", "-c", cmd_to_run]
        elif shutil.which("terminator"):
            terminal_cmd = ["terminator", "-x", "bash", "-c", cmd_to_run]
        elif shutil.which("x-terminal-emulator"):
            terminal_cmd = ["x-terminal-emulator", "-e", f"bash -c '{cmd_to_run}'"]
        else:
            self.log_message("--- Failed to launch terminal: No supported terminal emulator found ---\n\n")
            return
            
        self.log_message(f"--- Launching in external terminal: {val} ---\n")
        try:
            subprocess.Popen(terminal_cmd, cwd=PROJECT_ROOT)
        except Exception as e:
            self.log_message(f"--- Failed to launch terminal for {val}: {str(e)} ---\n")

    def launch_all_terminals(self):
        targets = self.get_arguments_dds()
        if not targets:
            messagebox.showwarning("No Targets", "Please add at least one target process.")
            return
            
        use_sudo = self.launch_sudo_var.get()
        if use_sudo and not self.sudo_password:
            pwd = simpledialog.askstring("Sudo Required", "Enter your sudo password to run applications as root:\\n(It will not be stored)", show='*')
            if pwd is None: return
            self.sudo_password = pwd
            
        for val in targets:
            app_path = os.path.join(BIN_APP_DIR, val)
            if not os.path.exists(app_path):
                self.log_message(f"--- Executable {val} not found in bin/app/ ---\n")
                continue
                
            if use_sudo:
                cmd = f"sudo -S bash -c '{app_path} < /dev/tty'; echo; echo \"Process finished. Press Enter to close.\"; read < /dev/tty"
                cmd_to_run = f"echo '{self.sudo_password}' | {{ {cmd}; }}"
            else:
                cmd = f"{app_path}; echo; echo \"Process finished. Press Enter to close.\"; read"
                cmd_to_run = cmd
                
            terminal_cmd = []
            if shutil.which("gnome-terminal"):
                terminal_cmd = ["gnome-terminal", "--", "bash", "-c", cmd_to_run]
            elif shutil.which("terminator"):
                terminal_cmd = ["terminator", "-x", "bash", "-c", cmd_to_run]
            elif shutil.which("x-terminal-emulator"):
                terminal_cmd = ["x-terminal-emulator", "-e", f"bash -c '{cmd_to_run}'"]
            else:
                self.log_message("--- Failed to launch terminal: No supported terminal emulator found ---\n\n")
                continue
                
            self.log_message(f"--- Launching in external terminal: {val} ---\n")
            try:
                subprocess.Popen(terminal_cmd, cwd=PROJECT_ROOT)
                time.sleep(1.0)  # Avoid sudo-rs concurrent authentication lock
            except Exception as e:
                self.log_message(f"--- Failed to launch terminal for {val}: {str(e)} ---\n")

    def refresh_scripts(self):
        self.script_listbox_single.delete(0, tk.END)
        self.script_listbox_dds.delete(0, tk.END)
        if os.path.exists(BIN_TOOLS_DIR):
            scripts = [f for f in os.listdir(BIN_TOOLS_DIR) if f.endswith(".sh")]
            scripts.sort()
            for s in scripts:
                self.script_listbox_single.insert(tk.END, s)
                self.script_listbox_dds.insert(tk.END, s)

    def on_script_select_single(self, event):
        selection = self.script_listbox_single.curselection()
        if selection:
            script_name = self.script_listbox_single.get(selection[0])
            self.selected_script_var_single.set(script_name)
            self.run_bg_btn_single.config(state="normal")
            self.run_term_btn_single.config(state="normal")
            if "marker1" in script_name or "marker." in script_name or "marker_dds" in script_name or "run_trace" in script_name:
                self.run_sudo_var_single.set(True)
            else:
                self.run_sudo_var_single.set(False)

    def on_script_select_dds(self, event):
        selection = self.script_listbox_dds.curselection()
        if selection:
            script_name = self.script_listbox_dds.get(selection[0])
            self.selected_script_var_dds.set(script_name)
            self.run_bg_btn_dds.config(state="normal")
            self.run_term_btn_dds.config(state="normal")
            
            if "marker1" in script_name or "marker." in script_name or "marker_dds" in script_name or "run_trace" in script_name:
                self.run_sudo_var_dds.set(True)
                if self.process_listbox.size() == 0 and len(self.add_proc_combobox['values']) > 0:
                    self.process_listbox.insert(tk.END, self.add_proc_combobox['values'][0])
            else:
                self.run_sudo_var_dds.set(False)

    def log_message(self, message):
        try:
            self.log_text_single.insert(tk.END, message)
            self.log_text_single.see(tk.END)
        except: pass
        try:
            self.log_text_dds.insert(tk.END, message)
            self.log_text_dds.see(tk.END)
        except: pass

    def clear_log(self):
        try: self.log_text_single.delete(1.0, tk.END); self.log_text_dds.delete(1.0, tk.END)
        except: pass

    def stop_process(self):
        if self.current_process:
            self.log_message("\n--- Stopping trace script... ---\n")
            try:
                # Invia SIGINT all'intero process group per raggiungere trace-cmd
                pgid = os.getpgid(self.current_process.pid)
                if self.sudo_password is not None and self.sudo_password:
                    subprocess.run(['sudo', '-S', '-k', 'kill', '-INT', '-' + str(pgid)], 
                                   input=self.sudo_password+'\n', text=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
                else:
                    os.killpg(pgid, signal.SIGINT)
            except Exception as e:
                self.log_message(f"Failed to stop script: {e}\n")
                # Fallback: kill the process directly
                try:
                    self.current_process.send_signal(signal.SIGINT)
                except Exception:
                    pass
            
            self.stop_btn_single.config(state="disabled")
        self.stop_btn_dds.config(state="disabled")

    def run_pipeline(self):
        # Chain marker2, marker3, marker4
        def pipeline_thread():
            self.log_message("\n=== STARTING AUTOMATIC PIPELINE ===\n")
            scripts = ["marker2.sh", "marker3.sh", "marker4.sh"]
            for script in scripts:
                script_path = os.path.join(BIN_TOOLS_DIR, script)
                self.log_message(f"--- Running Pipeline Step: {script} ---\n")
                try:
                    proc = subprocess.Popen(['bash', script_path], stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1, cwd=PROJECT_ROOT)
                    for line in proc.stdout:
                        self.log_text_single.after(0, self.log_message, line)
                    proc.wait()
                except Exception as e:
                    self.log_text_single.after(0, self.log_message, f"--- Pipeline failed on {script}: {e} ---\n")
                    break
            self.log_message("=== PIPELINE FINISHED ===\n\n")
            self.log_text_single.after(0, self.refresh_traces)
        
        threading.Thread(target=pipeline_thread, daemon=True).start()

    def _run_subprocess(self, cmd, use_sudo):
        self.log_message(f"--- Running Script: {' '.join(cmd)} ---\n")
        self.run_bg_btn_single.config(state="disabled")
        self.run_bg_btn_dds.config(state="disabled")
        self.run_term_btn_single.config(state="disabled")
        self.run_term_btn_dds.config(state="disabled")
        self.stop_btn_single.config(state="normal")
        self.stop_btn_dds.config(state="normal")
        
        try:
            if use_sudo:
                self.current_process = subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1, cwd=PROJECT_ROOT, start_new_session=True)
                self.current_process.stdin.write(self.sudo_password + '\n')
                self.current_process.stdin.flush()
            else:
                self.current_process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1, cwd=PROJECT_ROOT, start_new_session=True)
                
            for line in self.current_process.stdout:
                self.log_text_single.after(0, self.log_message, line)
                
            self.current_process.wait()
            self.log_text_single.after(0, self.log_message, f"--- Script finished with code {self.current_process.returncode} ---\n\n")
            
            # Check if we should run pipeline (applies to marker1 or marker)
            if self.running_script_name and ("marker1" in self.running_script_name or "marker." in self.running_script_name or "marker_dds" in self.running_script_name):
                # We need to run this in main thread so messagebox works, so we use after
                self.after(500, self.check_pipeline)

        except Exception as e:
            self.log_text_single.after(0, self.log_message, f"--- Execution Failed: {str(e)} ---\n\n")
        finally:
            self.current_process = None
            self.running_script_name = None
            self.run_bg_btn_single.config(state="normal")
            self.run_bg_btn_dds.config(state="normal")
            self.run_term_btn_single.config(state="normal")
            self.run_term_btn_dds.config(state="normal")
            self.stop_btn_single.config(state="disabled")
            self.stop_btn_dds.config(state="disabled")
            self.after(0, self.refresh_traces)

    def check_pipeline(self):
        if messagebox.askyesno("Recording Complete", "The marker recording has finished.\nDo you want to automatically generate the report and view the monitor? (Runs marker2, marker3, marker4)"):
            self.run_pipeline()

    def get_arguments_single(self):
        val = self.single_proc_combobox.get().strip()
        return [val] if val else []

    def get_arguments_dds(self):
        return list(self.process_listbox.get(0, tk.END))

    def run_in_background_single(self):
        if self.current_process is not None:
            messagebox.showwarning("Busy", "A trace script is already running. Stop it first.")
            return
            
        script_name = self.selected_script_var_single.get()
        if script_name == "None": return
        self.running_script_name = script_name
        
        script_path = os.path.join(BIN_TOOLS_DIR, script_name)
        args = self.get_arguments_single()
        
        use_sudo = self.run_sudo_var_single.get()
        cmd = []
        if use_sudo:
            if not self.sudo_password:
                pwd = simpledialog.askstring("Sudo Required", "Enter your sudo password to run the script:\\n(It will not be stored)", show='*')
                if pwd is None: return
                self.sudo_password = pwd
            cmd = ['sudo', '-S', '-k', '-E', 'bash', script_path] + args
        else:
            cmd = ['bash', script_path] + args
            
        threading.Thread(target=self._run_subprocess, args=(cmd, use_sudo), daemon=True).start()

    def run_in_background_dds(self):
        if self.current_process is not None:
            messagebox.showwarning("Busy", "A trace script is already running. Stop it first.")
            return
            
        script_name = self.selected_script_var_dds.get()
        if script_name == "None": return
        self.running_script_name = script_name
        
        script_path = os.path.join(BIN_TOOLS_DIR, script_name)
        args = self.get_arguments_dds()
        
        use_sudo = self.run_sudo_var_dds.get()
        cmd = []
        if use_sudo:
            if not self.sudo_password:
                pwd = simpledialog.askstring("Sudo Required", "Enter your sudo password to run the script:\\n(It will not be stored)", show='*')
                if pwd is None: return
                self.sudo_password = pwd
            cmd = ['sudo', '-S', '-k', '-E', 'bash', script_path] + args
        else:
            cmd = ['bash', script_path] + args
            
        threading.Thread(target=self._run_subprocess, args=(cmd, use_sudo), daemon=True).start()

    def run_in_terminal_single(self):
        script_name = self.selected_script_var_single.get()
        if script_name == "None": return
        
        script_path = os.path.join(BIN_TOOLS_DIR, script_name)
        args = " ".join(self.get_arguments_single())
        
        cmd = f"bash -c '{script_path} {args}; echo; echo \"Process finished. Press Enter to close.\"; read'"
        terminal_cmd = []
        if shutil.which("gnome-terminal"):
            terminal_cmd = ["gnome-terminal", "--", "bash", "-c", cmd]
        elif shutil.which("terminator"):
            terminal_cmd = ["terminator", "-x", "bash", "-c", cmd]
        elif shutil.which("x-terminal-emulator"):
            terminal_cmd = ["x-terminal-emulator", "-e", f"bash -c \'{cmd}\'"]
        else:
            self.log_message("--- Failed to launch terminal: No supported terminal emulator found ---\n\n")
            return
        self.log_message(f"--- Launching in external terminal: {script_name} {args} ---\n")
        try:
            subprocess.Popen(terminal_cmd, cwd=PROJECT_ROOT)
        except Exception as e:
            self.log_message(f"--- Failed to launch terminal: {str(e)} ---\n\n")

    def run_in_terminal_dds(self):
        script_name = self.selected_script_var_dds.get()
        if script_name == "None": return
        
        script_path = os.path.join(BIN_TOOLS_DIR, script_name)
        args = " ".join(self.get_arguments_dds())
        
        cmd = f"bash -c '{script_path} {args}; echo; echo \"Process finished. Press Enter to close.\"; read'"
        terminal_cmd = []
        if shutil.which("gnome-terminal"):
            terminal_cmd = ["gnome-terminal", "--", "bash", "-c", cmd]
        elif shutil.which("terminator"):
            terminal_cmd = ["terminator", "-x", "bash", "-c", cmd]
        elif shutil.which("x-terminal-emulator"):
            terminal_cmd = ["x-terminal-emulator", "-e", f"bash -c \'{cmd}\'"]
        else:
            self.log_message("--- Failed to launch terminal: No supported terminal emulator found ---\n\n")
            return
        self.log_message(f"--- Launching in external terminal: {script_name} {args} ---\n")
        try:
            subprocess.Popen(terminal_cmd, cwd=PROJECT_ROOT)
        except Exception as e:
            self.log_message(f"--- Failed to launch terminal: {str(e)} ---\n\n")

    # ==========================================
    # TRACES TAB
    # ==========================================
    def setup_traces_tab(self):
        self.tab_traces.columnconfigure(1, weight=1)
        self.tab_traces.rowconfigure(0, weight=1)
        
        # Left Panel - Trace List
        left_panel = ttk.Frame(self.tab_traces, style="Card.TFrame")
        left_panel.grid(row=0, column=0, sticky="nsew", padx=10, pady=10)
        left_panel.rowconfigure(1, weight=1)
        
        ttk.Label(left_panel, text="Generated Traces", style="SubHeader.TLabel").grid(row=0, column=0, sticky="w", padx=10, pady=10)
        
        self.trace_listbox = tk.Listbox(left_panel, width=45, font=("Helvetica", 10), bg=TEXT_BG, fg=FG_COLOR, selectbackground=ACCENT_COLOR, selectforeground="white", highlightthickness=1, highlightbackground="#cccccc", relief="flat")
        self.trace_listbox.grid(row=1, column=0, sticky="nsew", padx=10, pady=(0, 10))
        self.trace_listbox.bind("<<ListboxSelect>>", self.on_trace_select)
        
        ttk.Button(left_panel, text="Refresh Traces", command=self.refresh_traces).grid(row=2, column=0, sticky="ew", padx=10, pady=(0, 10))
        
        # Right Panel - Trace Details
        self.trace_detail_panel = ttk.Frame(self.tab_traces, style="Card.TFrame")
        self.trace_detail_panel.grid(row=0, column=1, sticky="nsew", padx=(0, 10), pady=10)
        self.trace_detail_panel.columnconfigure(0, weight=1)
        self.trace_detail_panel.rowconfigure(3, weight=1)
        
        # Selected Trace Name
        self.selected_trace_var = tk.StringVar(value="Select a trace to view details")
        ttk.Label(self.trace_detail_panel, textvariable=self.selected_trace_var, font=("Helvetica", 14, "bold"), background=PANEL_BG, foreground=FG_COLOR).grid(row=0, column=0, sticky="w", padx=15, pady=15)
        
        # Action Buttons
        self.trace_actions_frame = tk.Frame(self.trace_detail_panel, bg=PANEL_BG)
        self.trace_actions_frame.grid(row=1, column=0, sticky="ew", padx=15, pady=(0, 15))
        
        self.btn_open_shark = ttk.Button(self.trace_actions_frame, text="🦈 Open KernelShark", command=self.open_kernelshark, state="disabled")
        self.btn_open_shark.pack(side="left", padx=(0, 10))
        
        self.btn_open_img = ttk.Button(self.trace_actions_frame, text="🖼️ View Monitor Image", command=self.open_monitor_image, state="disabled")
        self.btn_open_img.pack(side="left", padx=(0, 10))
        
        self.btn_delete_trace = ttk.Button(self.trace_actions_frame, text="🗑️ Delete Trace", command=self.delete_trace, state="disabled", style="Stop.TButton")
        self.btn_delete_trace.pack(side="left", padx=(0, 10))
        
        # Files List Label
        ttk.Label(self.trace_detail_panel, text="Files & Output:", style="SubHeader.TLabel").grid(row=2, column=0, sticky="w", padx=15, pady=(0, 5))
        
        # File viewer / info
        self.trace_content_text = scrolledtext.ScrolledText(self.trace_detail_panel, bg=TEXT_BG, fg=FG_COLOR, font=("Consolas", 10), insertbackground=FG_COLOR, relief="flat", highlightthickness=1, highlightbackground="#cccccc")
        self.trace_content_text.grid(row=3, column=0, sticky="nsew", padx=15, pady=(0, 15))
        
        self.refresh_traces()

    def refresh_traces(self):
        self.trace_listbox.delete(0, tk.END)
        self.trace_content_text.delete(1.0, tk.END)
        self.selected_trace_var.set("Select a trace to view details")
        self.btn_open_shark.config(state="disabled")
        self.btn_open_img.config(state="disabled")
        self.btn_delete_trace.config(state="disabled")
        
        if os.path.exists(BIN_TEST_DIR):
            traces = []
            for d in os.listdir(BIN_TEST_DIR):
                if d.startswith("trace_"):
                    full_path = os.path.join(BIN_TEST_DIR, d)
                    if os.path.isdir(full_path):
                        traces.append(d)
            traces.sort(reverse=True) # newest first usually
            for t in traces:
                self.trace_listbox.insert(tk.END, t)

    def on_trace_select(self, event):
        selection = self.trace_listbox.curselection()
        if selection:
            trace_name = self.trace_listbox.get(selection[0])
            self.selected_trace_var.set(trace_name)
            self.btn_delete_trace.config(state="normal")
            
            trace_dir = os.path.join(BIN_TEST_DIR, trace_name)
            files = []
            if os.path.exists(trace_dir):
                files = os.listdir(trace_dir)
            
            self.trace_content_text.delete(1.0, tk.END)
            self.trace_content_text.insert(tk.END, f"--- Contents of {trace_name} ---\n\n")
            
            if not files:
                self.trace_content_text.insert(tk.END, "Directory is empty.\n")
                self.btn_open_shark.config(state="disabled")
                self.btn_open_img.config(state="disabled")
                return
                
            for f in sorted(files):
                f_size = os.path.getsize(os.path.join(trace_dir, f)) / 1024
                self.trace_content_text.insert(tk.END, f"📄 {f} ({f_size:.1f} KB)\n")
                
            # Check for KernelShark
            if "trace.dat" in files:
                self.btn_open_shark.config(state="normal")
            else:
                self.btn_open_shark.config(state="disabled")
                
            # Check for Monitor Image
            if "timeline_monitor.png" in files:
                self.btn_open_img.config(state="normal")
            else:
                self.btn_open_img.config(state="disabled")
                
            # Try to show results if they exist
            results_file = os.path.join(trace_dir, "risultati_finali.txt")
            if os.path.exists(results_file):
                self.trace_content_text.insert(tk.END, "\n\n--- Analysis Results (risultati_finali.txt) ---\n\n")
                try:
                    with open(results_file, 'r') as rf:
                        content = rf.read()
                        self.trace_content_text.insert(tk.END, content)
                except Exception as e:
                    self.trace_content_text.insert(tk.END, f"Error reading results: {e}\n")

    def open_kernelshark(self):
        selection = self.trace_listbox.curselection()
        if not selection: return
        trace_name = self.trace_listbox.get(selection[0])
        trace_dir = os.path.join(BIN_TEST_DIR, trace_name)
        trace_dat = os.path.join(trace_dir, "trace.dat")
        
        if os.path.exists(trace_dat):
            self.trace_content_text.insert(tk.END, f"\n[Opening KernelShark for {trace_name}...]\n")
            subprocess.Popen(["kernelshark", "trace.dat"], cwd=trace_dir, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    def open_monitor_image(self):
        selection = self.trace_listbox.curselection()
        if not selection: return
        trace_name = self.trace_listbox.get(selection[0])
        trace_dir = os.path.join(BIN_TEST_DIR, trace_name)
        img_path = os.path.join(trace_dir, "timeline_monitor.png")
        
        if os.path.exists(img_path):
            self.trace_content_text.insert(tk.END, f"\n[Opening Monitor Image for {trace_name}...]\n")
            try:
                subprocess.Popen(["xdg-open", img_path], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            except Exception as e:
                self.trace_content_text.insert(tk.END, f"\nFailed to open image: {e}\n")

    def delete_trace(self):
        selection = self.trace_listbox.curselection()
        if not selection: return
        trace_name = self.trace_listbox.get(selection[0])
        
        if messagebox.askyesno("Delete Trace", f"Are you sure you want to permanently delete:\n{trace_name}?"):
            trace_dir = os.path.join(BIN_TEST_DIR, trace_name)
            try:
                shutil.rmtree(trace_dir)
                self.refresh_traces()
            except Exception as e:
                messagebox.showerror("Error", f"Could not delete directory:\n{e}")

if __name__ == "__main__":
    app = KernelTraceDashboard()
    app.mainloop()
