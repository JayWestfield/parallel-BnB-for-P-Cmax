import matplotlib.pyplot as plt
import numpy as np
from util import read_data_times

def plot_cumulative_times(data, single_thread_data, plotpath):
    num_thread_configs = len(data[0][1])
    # Extract and sort single-threaded execution times
    single_thread_times = np.sort([times[0] for name, times in single_thread_data if times[0] != float('inf')])

    # Check for mismatched configurations
    for name, times in data:
        if len(times) != num_thread_configs:
            print(f"Instance {name} has mismatched thread configurations: {len(times)}")

    # Create the plot
    plt.figure(figsize=(10, 6))

    # Plot cumulative times for each thread configuration
    for i in range(num_thread_configs):
        times = np.sort([times[i] for name, times in data if times[i] != float('inf')])
        plt.plot(times, np.arange(1, len(times) + 1), label=f'{indexToThreads(i)} Threads')

    # Plot single-threaded reference line
    plt.plot(single_thread_times, np.arange(1, len(single_thread_times) + 1), label='1 Thread (Reference)', color='black', linestyle='--')

    # Add labels, legend, and grid
    plt.xlabel('Time (s)')
    plt.ylabel('Solved Instances')
    plt.title('Solved Instances vs. Time')
    plt.legend()
    plt.grid(True)
    # plt.xlim(0.1)
    plt.ylim(500)
    # plt.set_ylim(ymin=ymin)

    # Save the plot to the specified path
    plt.savefig(plotpath, bbox_inches='tight')
    plt.close()

def indexToThreads(index):
    if (index <= 10): 
        return 2 ** index
    
    return 16 + 8 * (index - 4)
def main(filepathRef, filepathPar,  plotpath):
    print(f"Reading reference data from {filepathRef}")
    dataRef = read_data_times(filepathRef)
    dataPar = read_data_times(filepathPar)
    plot_cumulative_times(dataPar, dataRef, plotpath)
    print(f"Plot saved to {plotpath}")

if __name__ == "__main__":
    import sys
    if len(sys.argv) != 4:
        print("Usage: python3 sequentialCompare.py <reference_file> <parallel_file> <output_plot>")
    else:
        filepathRef = sys.argv[1]
        filepathPar = sys.argv[2]
        plotpath = sys.argv[3]
        main(filepathRef, filepathPar, plotpath)