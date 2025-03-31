import os
import subprocess
import sys
from threading import Timer

def check_solution(basepath, benchmark_name, instance_name, result):
    opt_file_path = os.path.join(basepath, f"opt-known-instances-{benchmark_name}.txt")
    try:
        with open(opt_file_path, "r") as f:
            for line in f:
                parts = line.strip().split()
                if len(parts) != 2:
                    continue
                known_instance, known_solution = parts
                if known_instance == instance_name:
                    return result == known_solution
    except FileNotFoundError:
        print(f"Error: Optimal solutions file '{opt_file_path}' not found.")
        sys.exit(1)
    return False
if len(sys.argv) != 5:
    print("Usage: python3 run_benchmarks.py <timeout> <basepath> <benchmark_name> <instances_file>")
    sys.exit(1)

timeout_sec = int(sys.argv[1])
basepath = sys.argv[2]
benchmark_name = sys.argv[3]
instances_file = sys.argv[4]

def run(cmd, timeout_sec=20):
    process = subprocess.Popen(cmd, stdout=subprocess.PIPE)
    timer = Timer(timeout_sec, lambda : (process.terminate()))
    out = b""
    try:
        timer.start()
        for c in iter(lambda: process.stdout.read(1), b""):
            out += c
    finally:
        timer.cancel()
        return out.decode("ascii")

# Read instance filenames from the provided file
try:
    with open(instances_file, "r") as f:
        instance_names = [line.strip() for line in f.readlines()]
except FileNotFoundError:
    print(f"Error: File '{instances_file}' not found.")
    sys.exit(1)
option = ["-cdsm"]
# Run each instance
for instance_name in instance_names:
    if not instance_name:  # Skip empty lines
        continue
    print(f"{instance_name} (", end="", flush=True)
    cmd = ['./external/pcmax/target/release/p_cmax_solver', os.path.join(basepath,benchmark_name, instance_name)] + option
    # print({"cmd": cmd})
    out = run(cmd, timeout_sec)        
    # print(out)

    if "solution found" in out:
        placeSol = out.find("solution found ")
        result = out[placeSol + 15:].split()[0]
        if ( not check_solution(basepath, benchmark_name, instance_name, result)):
            print("error_wrong_makespan)")
        place = out.find("time ")
        result = out[place + 5:].split()[0]
        print(result + ")", flush=True)
    else:
        print("canceled)")
    # if stdout:
    #     print(stdout)
    # if stderr:
    #     print(stderr)

