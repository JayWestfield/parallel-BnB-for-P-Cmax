import matplotlib.pyplot as plt
import numpy as np
import sys

def read_data(filepath):
    data = []
    with open(filepath, 'r') as file:
        for line in file:
            parts = line.split()
            name = parts[0]
            times = []
            for entry in parts[1:]:
                if entry == "canceled":
                    times.append(float('inf'))  # Unendlich für canceled
                elif entry.startswith("error_wrong_makespan_of"):
                    times.append(float('inf'))  # Unendlich für Fehler
                else:
                    times.append(float(entry))
            data.append((name, times))
    return data

def initialize_subplots(rows, cols, title):
    fig, axs = plt.subplots(rows, cols, figsize=(18, 12))
    fig.suptitle(title)
    return fig, axs

def save_plots(fig, filename="plots/all_plots_in_one.png"):
    plt.tight_layout(rect=[0, 0, 1, 0.95])
    fig.savefig(filename)
    plt.show()

def plot_cumulative_times(ax, data):
    num_thread_configs = len(data[0][1])
    for i in range(num_thread_configs):
        times = np.sort([times[i] for name, times in data if times[i] != float('inf')])
        ax.plot(times, np.arange(1, len(times) + 1), label=f'{2**i} Threads')
    
    ax.set_xlabel('Zeit (s)')
    ax.set_ylabel('Anzahl der Instanzen')
    ax.set_title('Laufzeit-Verteilung')
    ax.legend()
    ax.grid(True)

def plot_speedups(ax, data):
    num_thread_configs = len(data[0][1])
    speedups = {f'{2**i} Threads': [] for i in range(1, num_thread_configs)}
    
    for name, times in data:
        for i in range(1, num_thread_configs):
            if times[i] != float('inf') and times[0] != float('inf'):
                speedup = times[0] / times[i]
                speedups[f'{2**i} Threads'].append(speedup)
    
    for label, values in speedups.items():
        ax.plot(sorted(values), np.arange(1, len(values) + 1), label=label)
    
    ax.set_xlabel('Speedup')
    ax.set_ylabel('Anzahl der Instanzen')
    ax.set_title('Speedup-Verteilung')
    ax.legend()
    ax.grid(True)

def plot_speedup_statistics(ax, data):
    num_thread_configs = len(data[0][1])
    speedups_per_thread = {f'{2**i} Threads': [] for i in range(1, num_thread_configs)}

    for name, times in data:
        for i in range(1, num_thread_configs):
            if times[i] != float('inf') and times[0] != float('inf'):
                speedup = times[0] / times[i]
                speedups_per_thread[f'{2**i} Threads'].append(speedup)

    median_speedups = {}
    mean_speedups = {}
    
    for threads, speedups in speedups_per_thread.items():
        if speedups:  # Überprüfen, ob die Liste nicht leer ist
            median_speedups[threads] = np.median(speedups)
            mean_speedups[threads] = np.mean(speedups)
        else:
            median_speedups[threads] = float('nan')
            mean_speedups[threads] = float('nan')

    # Textdarstellung der Ergebnisse
    textstr = "Median der Speedups:\n"
    for threads, median in median_speedups.items():
        textstr += f"{threads}: {median:.3f}\n"

    textstr += "\nArithmetisches Mittel der Speedups:\n"
    for threads, mean in mean_speedups.items():
        textstr += f"{threads}: {mean:.3f}\n"

    # Positioniere den Text im Subplot
    ax.text(0.1, 0.5, textstr, fontsize=12, verticalalignment='center', transform=ax.transAxes)

    ax.axis('off')  # Deaktiviert die Achsen für dieses Subplot

def plot_speedup_boxplot(ax, data):
    num_thread_configs = len(data[0][1])
    speedups_per_thread = {f'{2**i} Threads': [] for i in range(1, num_thread_configs)}

    for name, times in data:
        for i in range(1, num_thread_configs):
            if times[i] != float('inf') and times[0] != float('inf'):
                speedup = times[0] / times[i]
                speedups_per_thread[f'{2**i} Threads'].append(speedup)

    # Sammle die Speedups in einer Liste für den Boxplot
    speedups_data = [speedups_per_thread[threads] for threads in speedups_per_thread]

    # Erstelle den Boxplot
    ax.boxplot(speedups_data, labels=speedups_per_thread.keys())
    ax.set_xlabel('Anzahl der Threads')
    ax.set_ylabel('Speedup')
    ax.set_title('Verteilung der Speedups')
    ax.grid(True)

def plot_speedup_boxplot_filtered(ax, data, min_time=0.01):
    num_thread_configs = len(data[0][1])
    speedups_per_thread = {f'{2**i} Threads': [] for i in range(1, num_thread_configs)}

    for name, times in data:
        if times[0] >= min_time and all(t >= min_time for t in times):  # Bedingung: Laufzeit mindestens 0.1 s für alle Konfigurationen
            for i in range(1, num_thread_configs):
                if times[i] != float('inf'):
                    speedup = times[0] / times[i]
                    speedups_per_thread[f'{2**i} Threads'].append(speedup)

    # Sammle die Speedups in einer Liste für den Boxplot
    speedups_data = [speedups_per_thread[threads] for threads in speedups_per_thread]

    # Erstelle den Boxplot
    ax.boxplot(speedups_data, labels=speedups_per_thread.keys())
    ax.set_xlabel('Anzahl der Threads')
    ax.set_ylabel('Speedup')
    ax.set_title(f'Verteilung der Speedups (nur Instanzen > {min_time} s)')
    ax.grid(True)


def plot_boxplot_times(ax, data):
    num_thread_configs = len(data[0][1])
    times_data = []
    
    for i in range(num_thread_configs):
        valid_times = [times[i] for name, times in data if times[i] != float('inf')]
        times_data.append(valid_times)
    
    ax.boxplot(times_data, labels=[f'{2**i} Threads' for i in range(num_thread_configs)])
    ax.set_xlabel('Anzahl der Threads')
    ax.set_ylabel('Laufzeiten (s)')
    ax.set_title('Verteilung der Laufzeiten')
    ax.grid(True)

def plot_histogram_times(ax, data):
    num_thread_configs = len(data[0][1])
    
    for i in range(num_thread_configs):
        valid_times = [times[i] for name, times in data if times[i] != float('inf')]
        ax.hist(valid_times, bins=100, alpha=0.5, label=f'{2**i} Threads')  # Erhöhte Anzahl von Bins
    
    ax.set_xlabel('Laufzeiten (s)')
    ax.set_ylabel('Häufigkeit')
    ax.set_title('Histogramm der Laufzeiten')
    ax.legend()
    ax.grid(True)


def plot_canceled_jobs(ax, data):
    num_thread_configs = len(data[0][1])
    canceled_jobs = {f'{2**i} Threads': 0 for i in range(num_thread_configs)}
    total_jobs = {f'{2**i} Threads': 0 for i in range(num_thread_configs)}
    
    for name, times in data:
        for i in range(num_thread_configs):
            total_jobs[f'{2**i} Threads'] += 1
            if times[i] == float('inf'):
                canceled_jobs[f'{2**i} Threads'] += 1

    # Textdarstellung der gecancelten Jobs und deren Anteil
    textstr = "Gecancelte Jobs:\n"
    for threads in canceled_jobs:
        total = total_jobs[threads]
        canceled = canceled_jobs[threads]
        ratio = canceled / total if total > 0 else 0
        textstr += f"{threads}: {canceled} von {total} Jobs gecancelt ({ratio:.2%})\n"
    
    # Positioniere den Text im Subplot
    ax.text(0.1, 0.5, textstr, fontsize=12, verticalalignment='center', transform=ax.transAxes)

    ax.axis('off')  # Deaktiviert die Achsen für dieses Subplot

def plot_cumulative_times_filtered(ax, data, min_time=0.01):
    num_thread_configs = len(data[0][1])
    for i in range(num_thread_configs):
        # Filtern der Laufzeiten größer als min_time
        times = np.sort([times[i] for name, times in data if times[i] != float('inf') and times[i] > min_time])
        ax.plot(times, np.arange(1, len(times) + 1), label=f'{2**i} Threads')
    
    ax.set_xlabel('Zeit (s)')
    ax.set_ylabel('Anzahl der Instanzen')
    ax.set_title(f'Laufzeit-Verteilung (Laufzeiten > {min_time} s)')
    ax.legend()
    ax.grid(True)


def plot_all_in_one(data):
    fig, axs = initialize_subplots(3, 3, "Analyse der Laufzeiten und Speedups")
    min_time = 0.1
    plot_cumulative_times(axs[0, 0], data)
    plot_speedups(axs[0, 1], data)
    plot_speedup_statistics(axs[0, 2], data)
    plot_speedup_boxplot(axs[1, 0], data)
    plot_boxplot_times(axs[1, 1], data)
    plot_histogram_times(axs[1, 2], data)
    plot_canceled_jobs(axs[2,0], data)
    plot_cumulative_times_filtered(axs[2,1], data, min_time)
    plot_speedup_boxplot_filtered(axs[2,2], data, min_time)
    save_plots(fig)
    
def main(filepath):
    data = read_data(filepath)
    plot_all_in_one(data)

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <path_to_file>")
        sys.exit(1)
    
    filepath = sys.argv[1]
    main(filepath)
