import matplotlib.pyplot as plt
import numpy as np
import sys
import matplotlib.gridspec as gridspec

def parse_difficulty(value):
    difficulty_map = {
        '0': 'trivial',
        '1': 'lptOpt',
        '2': 'lowerBoundOptimal',
        '3': 'full'
    }
    return difficulty_map.get(value, 'unknown')

def read_data(filepath):
    data = []
    with open(filepath, 'r') as file:
        for line in file:
            if line.startswith("nohup"):
                continue
            parts = line.split()
            name = parts[0]
            times_info = []
            for entry in parts[1:]:
                entry = entry.strip('()')
                if entry.startswith("error_wrong_makespan_of") or entry.startswith("canceled"):
                    time = float('inf')
                    nodes = None
                    difficulty = None
                    bound_times = None
                else:
                    time_str, nodes_str, bound_times_str, difficulty_str = entry.split(',')
                    time = float(time_str)
                    nodes = int(nodes_str)
                    bound_times = list(map(float, bound_times_str.strip('{}').split(';')))
                    difficulty = parse_difficulty(difficulty_str)
                times_info.append((time, nodes, bound_times, difficulty))
            data.append((name, times_info))
    return data

def read_data_times(filepath):
    data = []
    with open(filepath, 'r') as file:
        for line in file:
            if line.startswith("nohup"):
                continue
            parts = line.split()
            name = parts[0]
            times = []
            for entry in parts[1:]:
                entry = entry.strip('()')
                if entry.startswith("canceled"):
                    times.append(float('inf'))  # Unendlich für canceled
                elif entry.startswith("error_wrong_makespan_of"):
                    times.append(float('inf'))  # Unendlich für Fehler
                else:
                    times.append(float(entry.split(',')[0]))
            data.append((name, times))
    return data

def initialize_subplots(rows, cols, title):
    fig, axs = plt.subplots(rows, cols, figsize=(18, 12))
    fig.suptitle(title)
    return fig, axs

def save_plots(fig, filename="plots/all_plots_in_one.png"):
    plt.tight_layout(rect=[0, 0, 1, 0.95])
    fig.savefig(filename)

def plot_cumulative_times(ax, data):
    num_thread_configs = len(data[0][1])
    # just in case a computation failed and not even canceled has been logged added a print to find taht isntance
    for name, times in data:
        if (len(times) != num_thread_configs):
            print(name)
            print(len(times))
    for i in range(num_thread_configs):
        times = np.sort([times[i] for name, times in data if times[i] != float('inf')])
        ax.plot(times, np.arange(1, len(times) + 1), label=f'{indexToThreads(i)} Threads')
    
    ax.set_xlabel('Zeit (s)')
    ax.set_ylabel('Anzahl der Instanzen')
    ax.set_title('Laufzeit-Verteilung')
    ax.legend()
    ax.grid(True)

def plot_speedups(ax, data):
    num_thread_configs = len(data[0][1])
    speedups = {f'{indexToThreads(i)} Threads': [] for i in range(1, num_thread_configs)}
    
    for name, times in data:
        for i in range(1, num_thread_configs):
            if times[i] != float('inf') and times[0] != float('inf'):
                speedup = times[0] / times[i]
                if (speedup > indexToThreads(i) * 1.5): # filter speedups
                    continue
                else:
                    speedups[f'{indexToThreads(i)} Threads'].append(speedup)
    
    for label, values in speedups.items():
        ax.plot(sorted(values), np.arange(1, len(values) + 1), label=label)
    
    ax.set_xlabel('Speedup')
    ax.set_ylabel('Anzahl der Instanzen')
    ax.set_title('Speedup-Verteilung')
    ax.legend()
    ax.grid(True)

def plot_speedup_statistics(ax, data):
    num_thread_configs = len(data[0][1])
    speedups_per_thread = {f'{indexToThreads(i)} Threads': [] for i in range(1, num_thread_configs)}

    for name, times in data:
        for i in range(1, num_thread_configs):
            if times[i] != float('inf') and times[0] != float('inf'):
                speedup = times[0] / times[i]
                if (speedup > indexToThreads(i) * 1.5): # filter speedups
                    continue
                else:
                    speedups_per_thread[f'{indexToThreads(i)} Threads'].append(speedup)

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

    # not relevant
    # textstr += "\nArithmetisches Mittel der Speedups:\n"
    # for threads, mean in mean_speedups.items():
    #     textstr += f"{threads}: {mean:.3f}\n"

    # Positioniere den Text im Subplot
    ax.text(0.1, 0.5, textstr, fontsize=12, verticalalignment='center', transform=ax.transAxes)

    ax.axis('off')  # Deaktiviert die Achsen für dieses Subplot

def plot_speedup_boxplot(ax, data):
    num_thread_configs = len(data[0][1])
    speedups_per_thread = {f'{indexToThreads(i)} Threads': [] for i in range(1, num_thread_configs)}

    for name, times in data:
        for i in range(1, num_thread_configs):
            if times[i] != float('inf') and times[0] != float('inf'):
                speedup = times[0] / times[i]
                if (speedup > 2 ** (i + 1)): # filter speedups
                    continue
                else:
                    speedups_per_thread[f'{indexToThreads(i)} Threads'].append(speedup)

    # Sammle die Speedups in einer Liste für den Boxplot
    speedups_data = [speedups_per_thread[threads] for threads in speedups_per_thread]

    # Erstelle den Boxplot
    ax.boxplot(speedups_data, tick_labels=speedups_per_thread.keys())
    ax.set_xlabel('Anzahl der Threads')
    ax.set_ylabel('Speedup')
    ax.set_title('Verteilung der Speedups')
    ax.grid(True)

def plot_speedup_boxplot_filtered(ax, data, min_time=0.01):
    num_thread_configs = len(data[0][1])
    speedups_per_thread = {f'{indexToThreads(i)} Threads': [] for i in range(1, num_thread_configs)}

    for name, times in data:
        if times[0] >= min_time and all(t >= min_time for t in times):  # Bedingung: Laufzeit mindestens 0.1 s für alle Konfigurationen
            for i in range(1, num_thread_configs):
                if times[i] != float('inf'):
                    speedup = times[0] / times[i]
                    if (speedup > 2 ** (i + 1)): # filter speedups
                        continue
                    else:
                        speedups_per_thread[f'{indexToThreads(i)} Threads'].append(speedup)

    # Sammle die Speedups in einer Liste für den Boxplot
    speedups_data = [speedups_per_thread[threads] for threads in speedups_per_thread]

    # Erstelle den Boxplot
    ax.boxplot(speedups_data, tick_labels=speedups_per_thread.keys())
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
    
    ax.boxplot(times_data, tick_labels=[f'{indexToThreads(i)} Threads' for i in range(num_thread_configs)])
    ax.set_xlabel('Anzahl der Threads')
    ax.set_ylabel('Laufzeiten (s)')
    ax.set_title('Verteilung der Laufzeiten')
    ax.grid(True)

def plot_histogram_times(ax, data):
    num_thread_configs = len(data[0][1])
    
    for i in range(num_thread_configs):
        valid_times = [times[i] for name, times in data if times[i] != float('inf')]
        ax.hist(valid_times, bins=100, alpha=0.5, label=f'{indexToThreads(i)} Threads')  # Erhöhte Anzahl von Bins
    
    ax.set_xlabel('Laufzeiten (s)')
    ax.set_ylabel('Häufigkeit')
    ax.set_title('Histogramm der Laufzeiten')
    ax.legend()
    ax.grid(True)


def plot_canceled_jobs(ax, data):
    num_thread_configs = len(data[0][1])
    canceled_jobs = {f'{indexToThreads(i)} Threads': 0 for i in range(num_thread_configs)}
    total_jobs = {f'{indexToThreads(i)} Threads': 0 for i in range(num_thread_configs)}
    
    for name, times in data:
        for i in range(num_thread_configs):
            total_jobs[f'{indexToThreads(i)} Threads'] += 1
            if times[i] == float('inf'):
                canceled_jobs[f'{indexToThreads(i)} Threads'] += 1

    # Textdarstellung der gecancelten Jobs und deren Anteil
    textstr = "Completed Jobs:\n"
    for threads in canceled_jobs:
        total = total_jobs[threads]
        canceled = canceled_jobs[threads]
        ratio = canceled / total if total > 0 else 0
        textstr += f"{threads}: {total - canceled} of  {total}Jobs completed within the timeout ({(1 - ratio):.2%})\n"
    
    # Positioniere den Text im Subplot
    ax.text(0.1, 0.5, textstr, fontsize=12, verticalalignment='center', transform=ax.transAxes)

    ax.axis('off')  # Deaktiviert die Achsen für dieses Subplot

def plot_cumulative_times_filtered(ax, data, min_time=0.01):
    num_thread_configs = len(data[0][1])
    ymin = 0
    for i in range(num_thread_configs):
        # Filtern der Laufzeiten größer als min_time
        times = np.sort([times[i] for name, times in data])
        ax.plot(times, np.arange(1, len(times) + 1), label=f'{indexToThreads(i)} Threads')
        for t in times:
            if i == 1 and  t < min_time:
                ymin = ymin + 1
    

    ax.set_xlim(xmin=min_time)
    ax.set_ylim(ymin=ymin)
    ax.set_xlabel('Zeit (s)')
    ax.set_ylabel('Anzahl der Instanzen')
    ax.set_title(f'Laufzeit-Verteilung (Laufzeiten > {min_time} s)')
    ax.legend()
    ax.grid(True)

def plot_speedup_statistics_filtered(ax, data,  min_time=0.01):
    num_thread_configs = len(data[0][1])
    speedups_per_thread = {f'{indexToThreads(i)} Threads': [] for i in range(1, num_thread_configs)}

    for name, times in data:
        if all(time > min_time for time in times[:num_thread_configs]):
            for i in range(1, num_thread_configs):
                if times[i] != float('inf') and times[0] != float('inf') :
                    speedup = times[0] / times[i]
                    speedups_per_thread[f'{indexToThreads(i)} Threads'].append(speedup)

    median_speedups = {}
    mean_speedups = {}
    
    for threads, speedups in speedups_per_thread.items():
        if speedups:  # Überprüfen, ob die Liste nicht leer ist
            median_speedups[threads] = np.median(speedups)
            mean_speedups[threads] = np.mean(speedups)
        else:
            median_speedups[threads] = float('nan')
            mean_speedups[threads] = float('nan')

    textstr = f"Gefilterte Statistiken (nur Instanzen  > {min_time}):\n\n"
    # Textdarstellung der Ergebnisse
    textstr += "Median der Speedups:\n"
    for threads, median in median_speedups.items():
        textstr += f"{threads}: {median:.3f}\n"

    # irrelevant
    # textstr += "\nArithmetisches Mittel der Speedups:\n"
    # for threads, mean in mean_speedups.items():
    #     textstr += f"{threads}: {mean:.3f}\n"

    # Positioniere den Text im Subplot
    ax.text(0.1, 0.5, textstr, fontsize=12, verticalalignment='center', transform=ax.transAxes)

    ax.axis('off')  # Deaktiviert die Achsen für dieses Subplot

def plot_nodes_ratio_boxplot(ax, data):
    num_thread_configs = len(data[0][1])
    node_ratios_per_thread = {f'{indexToThreads(i)} Threads': [] for i in range(1, num_thread_configs)}
    
    for name, infos in data:
        base_nodes = infos[0][1]
        if base_nodes is None or base_nodes == 0:
            continue
        
        for i in range(1, num_thread_configs):
            if infos[i][1] is not None:
                node_ratio = infos[i][1] / base_nodes
                # if node_ratio > 50:
                #     continue
                node_ratios_per_thread[f'{indexToThreads(i)} Threads'].append(node_ratio)
    
    ax.boxplot(node_ratios_per_thread.values(), labels=node_ratios_per_thread.keys())
    ax.set_xlabel('Anzahl der Threads')
    ax.set_ylabel('Verhältnis der besuchten Knoten (relativ zu 1 Thread)')
    ax.set_title('Verhältnis der besuchten Knoten je Thread-Konfiguration')
    ax.grid(True)

def plot_speedup_vs_nodes_ratio(ax, data):
    num_thread_configs = len(data[0][1])
    
    for i in range(1, num_thread_configs):
        speedup_vs_nodes = []
        
        for name, infos in data:
            base_time = infos[0][0]
            base_nodes = infos[0][1]
            if base_time != float('inf') and infos[i][0] != float('inf') and base_nodes is not None and base_nodes != 0:
                speedup = base_time / infos[i][0]
                nodes_ratio = infos[i][1] / base_nodes if infos[i][1] is not None else 1
                speedup_vs_nodes.append((speedup, nodes_ratio))
        
        if speedup_vs_nodes:
            speedups, nodes_ratios = zip(*speedup_vs_nodes)
            ax.scatter(nodes_ratios, speedups, alpha=0.5, label=f'{indexToThreads(i)} Threads', s=5)
    
    ax.set_xlabel('Verhältnis der besuchten Knoten')
    ax.set_ylabel('Speedup')
    # ax.set_xlim(0,50)
    # ax.set_ylim(0,8)

    ax.set_title('Speedup vs. Verhältnis der besuchten Knoten')
    ax.legend()
    ax.grid(True)
from scipy.interpolate import interp1d

def plot_relative_array_values_per_instance(ax, data, interp_length=100):
    num_thread_configs = len(data[0][1])
    all_relative_values = []

    for name, times_info in data:
        for i in range(num_thread_configs):
            if i != 1:
                continue
            time, nodes, array_values, difficulty = times_info[i]
            if time != float('inf') and len(array_values) > 1:
                total_sum = sum(array_values)
                relative_values = [val / total_sum for val in array_values]
                x_vals = np.linspace(0, 1, len(array_values))
                interpolator = interp1d(x_vals, relative_values, kind='linear', fill_value='extrapolate')
                interp_x_vals = np.linspace(0, 1, interp_length)
                interp_relative_values = interpolator(interp_x_vals)
                all_relative_values.append(interp_relative_values)
    
    if all_relative_values:
        all_relative_values = np.array(all_relative_values)
        avg_values = np.mean(all_relative_values, axis=0)
        std_values = np.std(all_relative_values, axis=0)
        
        ax.plot(interp_x_vals, avg_values, label='Durchschnitt', color='blue', linewidth=2)
        ax.fill_between(interp_x_vals, avg_values - std_values, avg_values + std_values, color='blue', alpha=0.2)

    ax.set_xlabel('Normierte Position im Array')
    ax.set_ylabel('Relativer Anteil')
    ax.set_title('Verlauf der Laufzeiten pro Bound pro Instanz')
    ax.legend()
    ax.grid(True)

def plot_last_bounds_time_distribution(ax, data, num_bounds=4):
    num_thread_configs = len(data[0][1])
    time_ratios_per_bound = [[] for _ in range(num_bounds)]

    for name, times_info in data:
        for i in range(num_thread_configs):
            if i != 2:
                continue
            time, nodes, array_values, difficulty = times_info[i]
            if time != float('inf') and len(array_values) >= num_bounds:
                for j in range(1, num_bounds + 1):
                    bound_time = array_values[-j]
                    time_ratio = bound_time / time
                    time_ratios_per_bound[num_bounds - j].append(time_ratio)

    # Creating box plots for each of the last bounds
    ax.boxplot(time_ratios_per_bound, vert=True, patch_artist=True, 
               boxprops=dict(facecolor='skyblue', color='blue'),
               whiskerprops=dict(color='blue'),
               capprops=dict(color='blue'),
               flierprops=dict(color='blue', markeredgecolor='blue'),
               medianprops=dict(color='red'))

    ax.set_xticklabels([f'{i} Bound' for i in range(num_bounds, 0, -1)])
    ax.set_ylabel('Prozentualer Anteil')
    ax.set_title('Prozentualer Zeitanteil der letzten Bounds')
    ax.grid(True)


def plot_all_in_one(data, ComplexData, plotpath):
    fig, axs = initialize_subplots(4, 3, "Analyse der Laufzeiten und Speedups")
    min_time = 0.1
    plot_cumulative_times(axs[0, 0], data)
    plot_speedups(axs[1, 0], data)
    plot_speedup_statistics(axs[0, 2], data)
    plot_speedup_boxplot(axs[0, 1], data)
    # plot_boxplot_times(axs[1, 1], data)
    plot_last_bounds_time_distribution(axs[1, 1], ComplexData)
    plot_canceled_jobs(axs[1,2], data)
    plot_cumulative_times_filtered(axs[2,0], data, min_time)
    plot_speedup_boxplot_filtered(axs[2,1], data, min_time)
    plot_speedup_statistics_filtered(axs[2,2],data, min_time)
    plot_nodes_ratio_boxplot(axs[3,0], ComplexData)
    plot_speedup_vs_nodes_ratio(axs[3,1], ComplexData)
    plot_relative_array_values_per_instance(axs[3,2], ComplexData)
    save_plots(fig, plotpath)
    
def main(filepath, plotpath):
    data = read_data_times(filepath)
    ComplexData = read_data(filepath)

    plot_all_in_one(data, ComplexData, plotpath)

def indexToThreads(index):
    if (index <= 4): 
        return 2 ** index
    
    return 16 + 8 * (index - 4)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <path_to_file>")
        sys.exit(1)
    
    filepath = sys.argv[1]
    if len(sys.argv) == 3:
        plotpath = sys.argv[2]
    main(filepath, plotpath)
