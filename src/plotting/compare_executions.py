import matplotlib.pyplot as plt
import numpy as np
import sys

def read_data(filepath):
    data = []
    with open(filepath, 'r') as file:
        for line in file:
            parts = line.split()
            name = parts[0]
            runs = []
            for entry in parts[1:]:
                entry = entry.strip("()")
                if entry == "canceled":
                    runs.append((float('inf'), 0, 0))
                elif entry.startswith("error_wrong_makespan_of"):
                    times.append(float('inf'))  # Unendlich für Fehler
                else:
                    time, visited_nodes, difficulty = map(float, entry.split(","))
                    runs.append((time, int(visited_nodes), int(difficulty)))
            data.append((name, runs))
    return data

def initialize_subplots(rows, cols, title):
    fig, axs = plt.subplots(rows, cols, figsize=(18, 12))
    fig.suptitle(title)
    return fig, axs

def save_plots(fig, filename):
    plt.tight_layout(rect=[0, 0, 1, 0.95])
    fig.savefig(filename)

def plot_comparison(ax, data1, data2):
    # Implementiere hier die Vergleichslogik für einen Subplot
    pass

def plot_runtime_comparison(ax, data1, data2):
    runtimes1 = [times[0] for _, times in data1]
    runtimes2 = [times[0] for _, times in data2]
    
    ax.scatter(runtimes1, runtimes2, alpha=0.6)
    ax.plot([min(runtimes1), max(runtimes1)], [min(runtimes1), max(runtimes1)], 'r--')  # Diagonale Linie
    ax.set_xlabel('Laufzeit Ausführung 1 (s)')
    ax.set_ylabel('Laufzeit Ausführung 2 (s)')
    ax.set_title('Vergleich der Laufzeiten')
    ax.grid(True)

def plot_speedup_comparison_boxplot(ax, data1, data2):
    num_thread_configs = len(data1[0][1])

    speedups1_per_thread = {f'{2**i} Threads': [] for i in range(1, num_thread_configs)}
    speedups2_per_thread = {f'{2**i} Threads': [] for i in range(1, num_thread_configs)}

    for (name1, times1), (name2, times2) in zip(data1, data2):
        for i in range(1, num_thread_configs):
            if times1[i][0] != float('inf') and times1[0][0] != float('inf'):
                speedup1 = times1[0][0] / times1[i][0]
                speedups1_per_thread[f'{2**i} Threads'].append(speedup1)
            if times2[i][0] != float('inf') and times2[0][0] != float('inf'):
                speedup2 = times2[0][0] / times2[i][0]
                speedups2_per_thread[f'{2**i} Threads'].append(speedup2)

    data_to_plot = []
    positions = []
    labels = []
    
    for idx, threads in enumerate(speedups1_per_thread.keys()):
        if speedups1_per_thread[threads] and speedups2_per_thread[threads]:
            data_to_plot.extend([speedups1_per_thread[threads], speedups2_per_thread[threads]])
            base_pos = idx * 2 + 1
            positions.extend([base_pos - 0.21, base_pos + 0.21]) 
            labels.extend([f'{threads}', f''])

    ax.boxplot(data_to_plot, positions=positions, widths=0.4)
    ax.set_xticks([i + 1 for i in range(len(positions))])
    ax.set_xticklabels(labels, rotation=45, ha='right')
    
    ax.set_xlabel('Threads')
    ax.set_ylabel('Speedup')
    ax.set_title('Speedup-Vergleich (Run 1 vs Run 2)')
    ax.grid(True)







def plot_runtime_diff_distribution(ax, data1, data2):
    runtime_diffs = [
        times1[0][0] - times2[0][0]  # Zugriff auf die Laufzeit
        for (name1, times1), (name2, times2) in zip(data1, data2)
        if times1[0][0] != float('inf') and times2[0][0] != float('inf')
    ]
    
    ax.hist(runtime_diffs, bins=20, alpha=0.7, color='blue')
    ax.set_xlabel('Laufzeitdifferenz (s) (Ausführung 1 - Ausführung 2)')
    ax.set_ylabel('Häufigkeit')
    ax.set_title('Verteilung der Laufzeitdifferenzen')
    ax.grid(True)



def plot_cumulative_times_comparison(ax, data1, data2):
    num_thread_configs = len(data1[0][1])
    i = num_thread_configs - 1
    # Bereite die Laufzeiten für die erste Ausführung vor
    times1 = np.sort([times[i][0] for name, times in data1 if times[i][0] != float('inf')])
    # Bereite die Laufzeiten für die zweite Ausführung vor
    times2 = np.sort([times[i][0] for name, times in data2 if times[i][0] != float('inf')])
    
    # Plot der kumulierten Verteilung für beide Ausführungen
    ax.plot(times1, np.arange(1, len(times1) + 1), label=f'Run 1 - {2**i} Threads', linestyle='-', marker='o')
    ax.plot(times2, np.arange(1, len(times2) + 1), label=f'Run 2 - {2**i} Threads', linestyle='--', marker='x')
    ax.set_xlim(left=0.1)
    ax.set_ylim(bottom=170)

    ax.set_xlabel('Zeit (s)')
    ax.set_ylabel('Anzahl der Instanzen')
    ax.set_title('Laufzeit-Verteilung Vergleich')
    # ax.autoscale(enable=True, axis='y', tight=True)
    ax.legend()
    ax.grid(True)

def plot_all_in_one(data1, data2, output_filepath):
    fig, axs = initialize_subplots(2, 3, "Vergleich der Ausführungen")

    # Beispiel für Subplots
    plot_runtime_comparison(axs[0, 0], data1, data2)
    plot_speedup_comparison_boxplot(axs[0, 1], data1, data2)
    plot_runtime_diff_distribution(axs[0, 2], data1, data2)
    plot_cumulative_times_comparison(axs[1, 0], data1, data2)
    plot_comparison(axs[1, 1], data1, data2)
    plot_comparison(axs[1, 2], data1, data2)

    save_plots(fig, output_filepath)


def main(file1, file2, output_filepath):
    data1 = read_data(file1)
    data2 = read_data(file2)
    plot_all_in_one(data1, data2, output_filepath)

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <file1> <file2> <output_filepath>")
        sys.exit(1)
    
    file1 = sys.argv[1]
    file2 = sys.argv[2]
    output_filepath = sys.argv[3]
    main(file1, file2, output_filepath)
