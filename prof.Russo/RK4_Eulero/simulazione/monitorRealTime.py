import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches

def main():
    # 1. Carica il file CSV generato dal programma C++
    csv_file = "timeline.csv"
    try:
        df = pd.read_csv(csv_file)
    except FileNotFoundError:
        print(f"Errore: Impossibile trovare il file '{csv_file}'.")
        return

    # 2. Estrai tutti i task unici (Activity_1, kworker, swapper, ecc.)
    tasks = df['task'].unique()
    
    # Assegna a ogni task un livello sull'asse Y (es. 0, 1, 2, 3...)
    # Invertiamo l'ordine per avere il primo task in alto
    task_to_y = {task: i for i, task in enumerate(reversed(tasks))}

    # 3. Configura i colori per i diversi stati
    color_map = {
        'RUN': 'tab:green',       # Verde: Il task sta usando la CPU
        'PREEMPT': 'tab:red',     # Rosso: Il task è pronto ma interrotto da un altro
        'SLEEP': 'lightgrey'      # Grigio: Il task sta dormendo (ha finito il periodo o aspetta)
    }

    # 4. Prepara la figura
    fig, ax = plt.subplots(figsize=(14, 7))
    
    # 5. Disegna i blocchi per ogni riga del CSV
    for index, row in df.iterrows():
        task = row['task']
        event_type = row['type']
        start_time = row['start']
        duration_sec = row['end'] - row['start']
        y_pos = task_to_y[task]

        # Se è uno stato (RUN, PREEMPT, SLEEP) disegniamo un blocco
        if event_type in color_map:
            # ax.broken_barh accetta una lista di tuple (inizio, durata) e (y_inizio, altezza_barra)
            ax.broken_barh([(start_time, duration_sec)], 
                           (y_pos - 0.4, 0.8), 
                           facecolors=color_map[event_type], 
                           edgecolor='black', 
                           linewidth=0.5)
            
        # Se è un MARKER, disegniamo una linea verticale e un puntino
        elif event_type.startswith('MARKER_'):
            marker_name = event_type.replace('MARKER_', '')
            ax.plot(start_time, y_pos, marker='v', color='blue', markersize=8)
            ax.text(start_time, y_pos + 0.5, marker_name, color='blue', fontsize=8, rotation=45)

    # 6. Formatta l'asse Y con i nomi dei task
    ax.set_yticks([i for i in range(len(tasks))])
    ax.set_yticklabels([task for task, i in sorted(task_to_y.items(), key=lambda item: item[1])])
    ax.set_ylabel('Task / Thread')

    # 7. Formatta l'asse X (Tempo)
    ax.set_xlabel('Tempo Assoluto (secondi)')
    ax.set_title('Monitor Real-Time di Schedulazione della CPU')
    ax.grid(True, axis='x', linestyle='--', alpha=0.7)

    # 8. Crea una legenda personalizzata
    legend_patches = [
        mpatches.Patch(color='tab:green', label='RUN (In Esecuzione)'),
        mpatches.Patch(color='tab:red', label='PREEMPT (Interrotto/In Attesa)'),
        mpatches.Patch(color='lightgrey', label='SLEEP (Inattivo/Riposo)'),
        plt.Line2D([0], [0], marker='v', color='w', markerfacecolor='blue', markersize=10, label='MARKER')
    ]
    ax.legend(handles=legend_patches, loc='upper right')

    # 9. Ottimizza i margini e mostra il grafico
    plt.tight_layout()
    
    # Permette lo zoom interattivo
    print("[+] Generazione grafico completata. Usa gli strumenti della finestra per zoomare.")
    plt.show()

if __name__ == "__main__":
    main()