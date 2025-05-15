#!/bin/sh
export CPUPROFILE_FREQUENCY=1000  # Set the frequency to 1000 Hz (adjust as needed)
rm -f ./profiling_results/profile_*.prof
export $(dbus-launch)
./build/RefactoredBnB_custom 20306 10 6 benchmarks/lawrinenko/p_cmax-class2-n60-m24-minsize20-maxsize100-seed18094.txt 165
google-pprof --callgrind ./build/RefactoredBnB_custom ./profiling_results/profile_*.prof > ./profiling_results/transformed_gperf.out
kcachegrind ./profiling_results/transformed_gperf.out