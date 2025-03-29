#!/bin/sh


# Check if arguments are provided, otherwise use default values
TIMEOUT=${1:-30}  # Default: 180


ulimit -s 65536  # Set stack size to 32 MB

# List of benchmark names
BENCHMARKS="berndt cnf frangioni graph huebner laupichler lawrinenko lehmann planted schreiber anni"

# Loop through each benchmark and execute the command
for BENCHMARK in $BENCHMARKS; do
  echo 'Start Experiment with timeout:' $TIMEOUT 'for benchmark: ' $BENCHMARK 
  python3 ./src/experiments/ref_run_benchmarks.py $TIMEOUT ./benchmarks $BENCHMARK ./benchmarks/sampledBenchmarks/${BENCHMARK}-50.txt
done