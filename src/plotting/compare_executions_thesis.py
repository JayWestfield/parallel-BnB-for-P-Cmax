import random
import matplotlib.pyplot as plt
import numpy as np
import sys
from util import read_data_times
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
RUN = [
    "Stability Range",
    "Fingerprint",
    "Run 5",
]

def initialize_subplots(rows, cols, title):
    fig, axs = plt.subplots(rows, cols, figsize=(8, 4))
    # fig.suptitle(title)
    plt.subplots_adjust(wspace=0.3, hspace=0.3)  # Weniger Abstand zwischen den Subplots
    return fig, axs

def save_plots(fig, filename):
    plt.tight_layout()
    fig.savefig(filename)


def plot_runtime_comparison(ax, data1, data2, num_thread_configs):
    # print("num_thread_configs", num_thread_configs)
    solved_by_1_timeout_2 = []
    solved_by_2_timeout_1 = []
    both_solved = []
    border_lo=0.001
    border_hi=200
    timeout=100
    jitter_scale = 0.08  # Scale of the jitter
    middle = 10 ** ((np.log10(border_hi) + np.log10(timeout)) / 2)
    for (name1, times1), (name2, times2) in zip(data1, data2):
        runtime1 = times1[num_thread_configs]
        runtime2 = times2[num_thread_configs]
        if (name1 != name2):
            print("error compare 2 different benchmarks")
        if runtime1 < timeout and runtime2 >= timeout:
            jitter = 10 ** (np.log10(middle) + random.uniform(-jitter_scale, jitter_scale))
            solved_by_1_timeout_2.append((runtime1, jitter))
        elif runtime2 < timeout and runtime1 >= timeout:
            jitter = 10 ** (np.log10(middle) + random.uniform(-jitter_scale, jitter_scale))
            solved_by_2_timeout_1.append((jitter, runtime2))
            # print("solved by 2 timeout 1", name1, runtime1, runtime2)
        elif runtime1 < timeout and runtime2 < timeout:
            both_solved.append((runtime1, runtime2))
    # Plot the points
    if both_solved:
        x_both, y_both = zip(*both_solved)
        ax.scatter(x_both, y_both, alpha=0.6, label="Both solved", color="#377eb8", marker="o", s=5)

    if solved_by_1_timeout_2:
        x_1, y_1 = zip(*solved_by_1_timeout_2)
        ax.scatter(x_1, y_1, alpha=0.6, color="#4daf4a", marker="^", s=5)

    if solved_by_2_timeout_1:
        x_2, y_2 = zip(*solved_by_2_timeout_1)
        ax.scatter(x_2, y_2, alpha=0.6, label="Solved by Run 2, Timeout Run 1", color="#e41a1c", s=5)

    # Add diagonal line for reference (y = x)
    ax.plot([border_lo, border_hi], [border_lo, border_hi], 'black', alpha=0.3, linestyle="--")

    # Add timeout lines
    ax.plot([border_lo, timeout], [timeout, timeout], 'black', alpha=1)
    ax.plot([timeout, timeout], [border_lo, timeout], 'black', alpha=1)

    # Fill timeout regions
    ax.fill_between([border_lo, timeout], [timeout, timeout], [border_hi, border_hi], alpha=0.3, color='gray', zorder=0)
    ax.fill_between([timeout, border_hi], [border_lo, border_lo], [timeout, timeout], alpha=0.3, color='gray', zorder=0)

    # Set log scale and labels
    ax.set_xscale('log')
    ax.set_yscale('log')
    ax.set_xlim(border_lo, border_hi)
    ax.set_ylim(border_lo, border_hi)
    ax.set_xlabel(f'Runtime {RUN[0]} ({2**num_thread_configs} Threads) (s)')
    ax.set_ylabel(f'Runtime {RUN[1]} ({2**num_thread_configs} Threads) (s)')
    # ax.set_title('Runtime Comparison')
    ax.grid(True)
    xticks = [tick for tick in ax.get_xticks() if border_lo <= tick <= border_hi]
    yticks = [tick for tick in ax.get_yticks() if border_lo <= tick <= border_hi]
    ax.set_xticks(xticks)
    ax.set_yticks(yticks)
    ax.set_box_aspect(1)

    # Add legend
    # ax.legend()
    # Add legend





def plot_cumulative_times_comparison(ax, data1, data2, num_thread_configs):

    i =  num_thread_configs
    i2 = num_thread_configs
    # Bereite die Laufzeiten für die erste Ausführung vor
    times1 = np.sort([times[i] for name, times in data1 if times[i] != float('inf')])
    # Bereite die Laufzeiten für die zweite Ausführung vor
    times2 = np.sort([times[i2] for name, times in data2 if times[i2] != float('inf')])
    
    # Plot der kumulierten Verteilung für beide Ausführungen
    ax.plot(times1, np.arange(1, len(times1) + 1), label=f'{RUN[0]} - {2**i} Threads', color=colors[0])
    ax.plot(times2, np.arange(1, len(times2) + 1), label=f'{RUN[1]} - {2**i2} Threads', color=colors[1])
    ax.plot([times1[-1], 60], [len(times1), len(times1)], color=colors[0])
    ax.plot([times2[-1], 60], [len(times2), len(times2)], color=colors[1])

    # ax.set_ylim(bottom=130)
    # ax.set_xlim(xmin=0.0001)
    ax.set_ylim(ymin=550)
    # ax.set_xscale('log')  # Set x-axis to log scale
    ax.set_xlabel('Time (s)')
    ax.set_ylabel('Solved Instances')
    # ax.set_title('Solved Instances vs Time')
    # ax.axis('equal')
    ax.set_box_aspect(1)
    # ax.set_aspect('equal', 'box')
    # ax.autoscale(enable=True, axis='y', tight=True)
    handles, labels = ax.get_legend_handles_labels()
    # Umkehren der Reihenfolge
    # ax.legend(handles[::-1], labels[::-1], loc='lower right') 
    ax.legend(loc='lower right')
    ax.grid(True)

def calculate_speedups(data1, data2):
    num_thread_configs = len(data1[0][1])

    speedups = {f'{2**i}': [] for i in range(num_thread_configs)}
    
    for (name1, times1), (name2, times2) in zip(data1, data2):
        if len(times1) == len(times2) and times1[0] != float('inf') and times2[0] != float('inf') and times1[0] >= 0.1 and times2[0] >= 0.1:
            for i in range(0, num_thread_configs):
                if times1[i] != float('inf') and times2[i] != float('inf'):
                    speedup = times1[i] / times2[i]
                    if (speedup > 1.5 *(2 ** (i)) or times1[0] < 0.1 or times2[0] < 0.1): # filter speedups
                        continue
                    else:
                        speedups[f'{2**i}'].append(speedup)
    
    return speedups

def calculate_speedup_statistics(ax, data1, data2):
    # Initialisiere Dictionaries
    speedups = {f'{2**i}': [] for i in range(len(data1[0][1]))}
    not_considered = {f'{2**i} Threads': [0, 0] for i in range(len(data1[0][1]))}
    overall1 = {f'{2**i}': [] for i in range(len(data1[0][1]))}
    overall2 = {f'{2**i}': [] for i in range(len(data1[0][1]))}
    # Erstelle Dictionaries für die Laufzeiten von Run 1 und Run 2
    times1_dict = {name: times for name, times in data1}
    times2_dict = {name: times for name, times in data2}

    # Finde Instanzen, die in beiden Runs vorhanden sind
    common_instances = set(times1_dict.keys()).intersection(times2_dict.keys())

    # Berechne Speedups für die gemeinsamen Instanzen
    for name in common_instances:
        times1 = times1_dict[name]
        times2 = times2_dict[name]
        for i in range(0, len(times1)):
            if times1[i] != float('inf') and times2[i] != float('inf') and times1[i] >= 0.1 and times2[i] >= 0.1:
                speedup =  times1[i] / times2[i]
                speedups[f'{2**i}'].append(speedup)
                overall1[f'{2**i}'].append(times1[i])
                overall2[f'{2**i}'].append(times2[i])
            elif times1[i] == float('inf') and times2[i] != float('inf'):
                not_considered[f'{2**i} Threads'][0] += 1
            elif times1[i] != float('inf') and times2[i] == float('inf'):
                not_considered[f'{2**i} Threads'][1] += 1
    # Berechne die Speedup-Median und -Mittelwert
    median_speedups = {}
    geometric_means = {}
    overall_speedups = {}
    overall_sums1 = {}
    overall_sums2 = {}
    for threads, values in overall1.items():
        overall_sums1[threads] = np.sum(values)
    for threads, values in overall2.items():
        overall_sums2[threads] = np.sum(values)
    for threads, values in overall_sums1.items():
        overall_speedups[threads] = values / overall_sums2.get(threads)

    for threads, values in speedups.items():
        if values:
            median_speedups[threads] = np.median(values)
            geometric_means[threads] = gmean(values)  # Calculate geometric mean
        else:
            median_speedups[threads] = float('nan')
            geometric_means[threads] = float('nan')


    
    ax.set_title("Speedup of Run 2 compared to Run 1", fontsize=14)
    table_data = [
        [threads, f"{median_speedups[threads]:.3f}", f"{geometric_means[threads]:.3f}", f"{overall_speedups[threads]:.3f}"]
        for threads in speedups.keys()
    ]
    column_labels = ["Threads", "Median", "Geometric", "Overall"]
    ax.axis('off')  # Deaktiviere die Achsen
    table = ax.table(cellText=table_data, colLabels=column_labels, loc='center', cellLoc='center')
    # table.auto_set_column_width(col=list(range(len(column_labels) +2)))
    ax.axis('off')

    



def main(file1, file2, output_filepath):
    data1 = read_data_times(file1)
    data2 = read_data_times(file2)

    fig, axs = initialize_subplots(1, 2, "Compare run 1 and run 2")
    speedups = calculate_speedups(data1, data2)
    plot_threads = len(data1[0][1]) - 1
    # Beispiel für Subplots

    plot_cumulative_times_comparison(axs[ 0], data1, data2, plot_threads)
    plot_runtime_comparison(axs[1], data1, data2, plot_threads)
    # calculate_speedup_statistics(axs[2], data1, data2)


    save_plots(fig, output_filepath)

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <file1> <file2> ")
        sys.exit(1)
    
    file1 = sys.argv[1]
    file2 = sys.argv[2]
    output_filepath = 'plots/local.pdf'
    if (len(sys.argv) >= 4):
        output_filepath = sys.argv[3]
    main(file1, file2, output_filepath)
