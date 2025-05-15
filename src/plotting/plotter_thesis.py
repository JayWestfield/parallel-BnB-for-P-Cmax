import matplotlib.pyplot as plt
import numpy as np
import sys
import matplotlib.gridspec as gridspec
from scipy.stats import gmean  # Import geometric mean function

colors = [
    "#E69F00",  # orange
    "#56B4E9",  # sky blue
    "#009E73",  # bluish green
    "#F0E442",  # yellow
    "#0072B2",  # blue
    "#D55E00",  # vermilion
    "#CC79A7",  # reddish purple
    "#F4A300",  # burnt orange
    "#7A7A7A",  # gray
    "#3B9C9C",  # teal
    "#000000",  # black
]

def add_tick_1(current_yticks):
    target_tick = 1
    # print(current_yticks)
    current_yticks = [round(float(tick), 2) for tick in current_yticks if tick >= 0]
    if target_tick not in current_yticks:
        current_yticks.append(target_tick)    
    # print(sorted(current_yticks))
    return sorted(current_yticks)

def parse_difficulty(value):
    difficulty_map = {
        '0': 'trivial',
        '1': 'lptOpt',
        '2': 'lowerBoundOptimal',
        '3': 'improvedlowerBoundOptimal',
        '4': 'full'
    }
    return difficulty_map.get(value, 'unknown')

def read_data(filepath):
    data = []
    numberRuns = 0
    with open(filepath, 'r') as file:
        for line in file:
            if line.startswith("nohup") or line.startswith("Start Experiment with"):
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
                    # print(entry)
                    if entry.__contains__(','):
                        time_str, nodes_str, bound_times_str, difficulty_str = entry.split(',')
                    else:
                        time_str = entry
                        nodes_str = '0'
                        bound_times_str = '{}'
                        difficulty_str = '0'
                    time = float(time_str)
                    nodes = int(nodes_str)
                    bound_times = [] if bound_times_str.strip() == '{}' else list(map(float, bound_times_str.strip('{}').split(';')))                    
                    difficulty = parse_difficulty(difficulty_str)
                times_info.append((time, nodes, bound_times, difficulty))
            numberRuns = max(numberRuns, len(times_info))
            while len(times_info) < numberRuns:
                time = float('inf')
                nodes = None
                difficulty = None
                bound_times = None
                times_info.append((time, nodes, bound_times, difficulty))
            data.append((name, times_info))
    return data

def read_data_times(filepath):
    data = []
    with open(filepath, 'r') as file:
        for line in file:
            if line.startswith("nohup") or line.startswith("Start Experiment with"):
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
                    if entry.__contains__(','):
                        times.append(float(entry.split(',')[0]))
                    else: 
                        times.append(float(entry))
            while len(times) < 4:
                times.append(float('inf'))  # Unendlich für canceled
            data.append((name, times))
    return data

def initialize_subplots(rows, cols, all):
    height = 4
    if all:
        height = 9
    fig, axs = plt.subplots(rows, cols, figsize=(14.5, height))
    # fig.suptitle(title)
    # plt.subplots_adjust(hspace=0.5)  # Abstand zwischen Zeilen
    return fig, axs

def save_plots(fig, filename="plots/all_plots_in_one.png"):
    plt.tight_layout(rect=[0, 0, 1, 1])
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

        ax.plot(times, np.arange(1, len(times) + 1), label=f'{indexToThreads(i)} Threads', color=colors[i])
        ax.plot([times[-1], 60], [len(times), len(times)], color=colors[i])
    ax.set_xlabel('Time (s)')
    ax.set_ylabel('Solved Instances')
    ax.set_title('Solved Instances vs. Time')
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
    ax.grid(True)

def plot_weak_scaling(ax, data, scaling=np.median, total_speedup=False):
    num_thread_configs = len(data[0][1])
    all_single_thread_times = [times[0] for name, times in data if times[0] != float('inf')]
    # time_thresholds = np.linspace(min(all_single_thread_times), max(all_single_thread_times), 100)
    # print(time_thresholds)
    scaling_name = scaling.__name__ if hasattr(scaling, '__name__') else "custom_scaling"
    if total_speedup:
        scaling_name = "Total Speedup"
    time_thresholds = sorted(set(times[0] for name, times in data if times[0] != float('inf')))
    title = f'Weak Scaling ({scaling_name})'
    for i in range(1, num_thread_configs):
        speedups = []
        for threshold in time_thresholds:
            # only compare those values where no cancellation happened
            filtered_times = [ times for _, times in data if times[0] >= threshold and times[i] != float('inf') and times[0] != float('inf') ]
            if len(filtered_times) < 20:
                speedups.append(np.nan)
                continue
            if total_speedup:
                # Calculate total speedup by summing single-threaded and multi-threaded times
                total_single_thread_time = sum(
                    times[0] for times in filtered_times
                )
                total_multi_thread_time = sum(
                    times[i] for times in filtered_times
                )
                if total_multi_thread_time > 0:
                    speedup = total_single_thread_time / total_multi_thread_time
                    speedups.append(speedup)
                else:
                    speedups.append(np.nan)
            else:
                filtered_speedups = [
                    times[0] / times[i]
                    for times in filtered_times
                ]
                # if (threshold >= 5 and threshold < 8): print(np.sort(filtered_speedups))
                if filtered_speedups and len(filtered_speedups) > 10:
                    median_speedup = scaling(filtered_speedups)
                    # median_speedup = np.average(filtered_speedups.)
                    speedups.append(median_speedup)
                else:
                    speedups.append(np.nan)  # For cases where no instances meet the threshold
        ax.plot(time_thresholds, speedups, label=f'{indexToThreads(i)} Threads', color=colors[i])
        
    ax.set_yticks(add_tick_1(ax.get_yticks()))
    ax.set_xlabel('Minimum Runtime Single Threaded (s)', fontsize=14)
    ax.set_ylabel(scaling_name, fontsize=14)
    ax.set_title(title, fontsize=14)
    handles, labels = ax.get_legend_handles_labels()
    # Umkehren der Reihenfolge
    ax.legend(handles[::-1], labels[::-1], fontsize=9) 

    ax.grid(True)
    ax.set_box_aspect(1)


def plot_speedup_statistics(ax, data):
    num_thread_configs = len(data[0][1])
    speedups_per_thread = {f'{indexToThreads(i)} Threads': [] for i in range(1, num_thread_configs)}

    for name, times in data:
        for i in range(1, num_thread_configs):
            if times[i] != float('inf') and times[0] != float('inf'):
                speedup = times[0] / times[i]
                # if (speedup > indexToThreads(i) * 1.5): # filter speedups
                #     continue
                if (times[0] > 0.1):
                    continue
                else:
                    speedups_per_thread[f'{indexToThreads(i)} Threads'].append(speedup)

    median_speedups = {}
    mean_speedups = {}
    geometric_means = {}

    for threads, speedups in speedups_per_thread.items():
        # print(speedups)
        if speedups:  # Überprüfen, ob die Liste nicht leer ist
            median_speedups[threads] = np.median(speedups)
            mean_speedups[threads] = np.mean(speedups)
            geometric_means[threads] = gmean(speedups)  # Calculate geometric mean
        else:
            median_speedups[threads] = float('nan')
            mean_speedups[threads] = float('nan')
            geometric_means[threads] = float('nan')

    # Textdarstellung der Ergebnisse
    textstr = "Median der Speedups:\n"
    for threads, median in median_speedups.items():
        textstr += f"{threads}: {median:.3f}\n"

    textstr = "Geometric Mean:\n"
    for threads, gmeanl in geometric_means.items():
        textstr += f"{threads}: {gmeanl:.3f}\n"
    # not relevant
    # textstr += "\nArithmetisches Mittel der Speedups:\n"
    # for threads, mean in mean_speedups.items():
    #     textstr += f"{threads}: {mean:.3f}\n"

    # Positioniere den Text im Subplot
    ax.text(0.1, 0.5, textstr, fontsize=14, verticalalignment='center', transform=ax.transAxes)

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
    # print(ax.get_yticks())
    # print(add_tick_1(ax.get_yticks()))
    ax.set_yticks(add_tick_1(ax.get_yticks()))
    ax.set_yscale('log')

    # Erstelle den Boxplot
    ax.boxplot(speedups_data, tick_labels=speedups_per_thread.keys())
    ax.set_xlabel('Anzahl der Threads')
    ax.set_ylabel('Speedup')
    ax.set_title('Verteilung der Speedups', fontsize=14)
    ax.grid(True)

def plot_speedup_boxplot_filtered(ax, data, min_time=0.01):
    num_thread_configs = len(data[0][1])
    speedups_per_thread = {f'{indexToThreads(i)}': [] for i in range(1, num_thread_configs)}

    for name, times in data:
        if times[0] >= min_time:
            for i in range(1, num_thread_configs):
                if times[i] != float('inf'):
                    speedup = times[0] / times[i]
                    if (speedup > 2 ** (i + 1)): # filter speedups
                        continue
                    else:
                        speedups_per_thread[f'{indexToThreads(i)}'].append(speedup)

    # Sammle die Speedups in einer Liste für den Boxplot
    speedups_data = [speedups_per_thread[threads] for threads in speedups_per_thread]
    ax.set_yscale('log')

    # Erstelle den Boxplot
    ax.boxplot(speedups_data, tick_labels=speedups_per_thread.keys())
    
    ax.set_xlabel('Threads', fontsize=14)
    ax.set_ylabel('Speedup', fontsize=14)
    ax.set_title(f'Speedup distibution (time > {min_time} s)', fontsize=14)
    ax.grid(True)
    ax.set_box_aspect(1)



from matplotlib.colors import LinearSegmentedColormap

def plot_cumulative_times_filtered(ax, data, min_time=0.31):
    num_thread_configs = len(data[0][1])
    ymin = 0
    for i in range(num_thread_configs):
        # Filtern der Laufzeiten größer als min_time
        times = np.sort([times[i] for name, times in data])
        filtered_times = [times[i] for i in range(len(times)) if times[i] != float('inf')]
        # colors = [(1, 1, 1), (0, 0.2, 0.4), (0, 0.4, 0.8), (0, 0.6, 1), (0.4, 0.8, 1), (0.6, 0.9, 1)]
        cmap = LinearSegmentedColormap.from_list('custom_cmap', colors, N=256)
        ax.plot(times, np.arange(1, len(times) + 1), label=f'{indexToThreads(i)} Threads', color=colors[i])
        ax.plot([filtered_times[-1], 60], [len(filtered_times), len(filtered_times)], color=colors[i])
        # print( len(filtered_times))
        for t in times:
            if i == 0 and  t < min_time:
                ymin = ymin + 1
    
    # print(ymin)
    ax.set_xlim(xmin=min_time)
    ax.set_ylim(ymin=ymin)
 
    ax.set_xlabel('Time (s)', fontsize=14)
    ax.set_ylabel('Solved Instances', fontsize=14)
    ax.set_title('Solved Instances vs. Time', fontsize=14)
    ax.legend()
    handles, labels = ax.get_legend_handles_labels()
    # Umkehren der Reihenfolge
    ax.legend(handles[::-1], labels[::-1])  # Legende mit umgekehrter Reihenfolge erstellen

    ax.grid(True)
    ax.set_box_aspect(1)



def plot_speedup_vs_nodes_ratio(ax, data, min_time=0.1):
    num_thread_configs = len(data[0][1])
    
    for i in range(1, num_thread_configs):
        speedup_vs_nodes = []
        # if i == 40 or i == 48 or i == 56 or i == 24:
        #     continue
        for name, infos in data:
            base_time = infos[0][0]
            base_nodes = infos[0][1]
            if base_time != float('inf') and infos[i][0] != float('inf') and base_nodes is not None and base_nodes != 0 and base_time > min_time:
                speedup = base_time / infos[i][0]
                nodes_ratio = infos[i][1] / base_nodes if infos[i][1] is not None else 1
                speedup_vs_nodes.append((speedup, nodes_ratio))
        
        if speedup_vs_nodes:
            speedups, nodes_ratios = zip(*speedup_vs_nodes)
            ax.scatter(nodes_ratios, speedups, alpha=0.5, label=f'{indexToThreads(i)} Threads', s=5, color=colors[i])
    
    ax.set_xlabel('Visited Nodes Ratio', fontsize=14)
    ax.set_ylabel('Speedup', fontsize=14)
    # ax.set
    # ax.set_xlim(0,10)
    ax.set_yticks(add_tick_1(ax.get_yticks()))
    # ax.set_ylim(0,20)
    ax.set_yscale('log')
    ax.set_xscale('log')

    ax.set_title(f'Speedup vs. Visited Nodes Ratio (time > {min_time} s)', fontsize=14)
    handles, labels = ax.get_legend_handles_labels()
    # Umkehren der Reihenfolge
    ax.legend(handles[::-1], labels[::-1]) 
    ax.grid(True)
    ax.set_box_aspect(1)




def plot_speedup_vs_nodes_ratio_heatmap_min_time(ax, data, num_threads, min_time=0.1):
    num_thread_configs = len(data[0][1])
    
    if num_threads > num_thread_configs - 1:
        raise ValueError("Die angegebene Anzahl an Threads ist zu groß für die verfügbaren Konfigurationen.")
    
    speedup_vs_nodes = []

    for name, infos in data:
        base_time = infos[0][0]
        base_nodes = infos[0][1]
        if base_time != float('inf') and infos[num_threads][0] != float('inf') and base_nodes is not None and base_nodes != 0 and base_time > min_time:
            speedup = base_time / infos[num_threads][0]
            nodes_ratio = infos[num_threads][1] / base_nodes if infos[num_threads][1] is not None else 1
            speedup_vs_nodes.append((speedup, nodes_ratio))

    if speedup_vs_nodes:
        speedups, nodes_ratios = zip(*speedup_vs_nodes)
        heatmap_data, xedges, yedges = np.histogram2d(nodes_ratios, speedups, bins=(100, 50), range=[[0, max(nodes_ratios)], [0, max(speedups)]])

        extent = [xedges[0], xedges[-1], yedges[0], yedges[-1]]
        cmap = LinearSegmentedColormap.from_list('custom_cmap', colors, N=256)
        im = ax.imshow(heatmap_data.T, extent=extent, origin='lower', cmap='cividis' , aspect='auto') # 'viridis' 
        cbar = plt.colorbar(im, ax=ax)
        # sns.heatmap(heatmap_data.T,  cmap=cmap, ax=ax, cbar=True, cbar_kws={'label': 'Dichte'}, vmin=0)
        ax.grid(True, which='both', linestyle='--', linewidth=0.5)
        ax.set_yticks(add_tick_1(ax.get_yticks()))
        ax.set_xticks(np.arange(0, max(nodes_ratios) + 1, step=1))

        ax.set_xlabel('Verhältnis der besuchten Knoten')
        ax.set_ylabel('Speedup')
        # ax.set_xlim(left=0)
        # ax.set_ylim(bottom=0, top=12)
        ax.set_title(f'Speedup vs. Verhältnis der besuchten Knoten ({indexToThreads(num_threads)} Threads)')
        # ax.invert_yaxis()  # Y-Achse invertieren
        ax.grid(True)

def plot_all_in_one(data, ComplexData, plotpath, title):
    # all = True
    all = False
    rows = 1
    if (all):
        rows = 2
    fig, axs = initialize_subplots(rows, 3,  all)
    min_time = 0.5

    # plot_cumulative_times(axs[0, 0], data)
    # plot_weak_scaling_with_uncertainty(axs[1, 0], data)
    # plot_speedup_statistics(axs[0, 2], data)
    # plot_speedup_boxplot(axs[0, 1], data)
    # plot_boxplot_times(axs[1, 1], data)
    # plot_weak_scaling(axs[1, 1], data)
    if all:
        plot_cumulative_times_filtered(axs[0,0], data, min_time)
        plot_speedup_boxplot_filtered(axs[0,1], data, min_time)
        plot_speedup_vs_nodes_ratio(axs[0,2], ComplexData, min_time)


    if all:
        plot_weak_scaling(axs[1, 0], data, scaling=np.median)
        plot_weak_scaling(axs[1, 1], data, scaling=gmean)
        plot_weak_scaling(axs[ 1, 2], data, total_speedup=True)
    else:
        plot_weak_scaling(axs[ 0], data, scaling=np.median)
        plot_weak_scaling(axs[ 1], data, scaling=gmean)
        plot_weak_scaling(axs[  2], data, total_speedup=True)
    # plot_weak_scaling(axs[9, 2], data, 3)

    # plot_speedup_vs_nodes_ratio_heatmap(axs[2], data)
    # Anzeigen des Plots
    # fig.subplots_adjust(hspace=1)  # Hier den Abstand erhöhen
    fig.tight_layout()
    save_plots(fig, plotpath)

 
def main(filepath, plotpath, title=""):
    data = read_data_times(filepath)
    ComplexData = read_data(filepath)
    plot_all_in_one(data, ComplexData, plotpath, title)

def indexToThreads(index):
    if (index <= 10): 
        return 2 ** index
    
    return 16 + 8 * (index - 4)

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <path_to_file>")
        sys.exit(1)
    
    filepath = sys.argv[1]
    plotpath = 'plots/local.pdf'
    if len(sys.argv) >= 3:
        plotpath = sys.argv[2]
    if len(sys.argv) >= 4:
        title = sys.argv[3]
    main(filepath, plotpath, title)
