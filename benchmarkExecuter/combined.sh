#!/bin/sh


# Check if arguments are provided, otherwise use default values
THREADS=${1:-64}
TIMEOUT=${2:-30}
CONFIG=${3:-200}
BENCHMARK_TYPE=${4:-50}



# List of benchmark names
BENCHMARKS="berndt cnf frangioni graph huebner laupichler lawrinenko lehmann planted schreiber anni"

# Loop through each benchmark and execute the command
for BENCHMARK in $BENCHMARKS; do
  ./build/full_refactored $THREADS $TIMEOUT $CONFIG benchmarks $BENCHMARK benchmarks/sampledBenchmarks/${BENCHMARK}-${BENCHMARK_TYPE}.txt
done