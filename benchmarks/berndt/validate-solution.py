
import sys

if len(sys.argv) < 3:
    print(f"Usage: {sys.argv[0]} instancefile outputfile")
    exit(0)

instancefile = sys.argv[1]
outputfile = sys.argv[2]

# Parse instance file
instancelines = [l.rstrip().split(" ") for l in open(instancefile, 'r').readlines()]
# header line: p p_cmax n m
n = int(instancelines[0][2])
m = int(instancelines[0][3])
# job sizes line
jobsizes = [int(x) for x in instancelines[1][0:-1]]
print(f"Parsed instance {instancefile}: n={n} m={m} sizes={jobsizes}")
assert(len(jobsizes) == n) # broken instance file - number of job sizes does not match n ...

# Parse output file
found_solution = False
for line in open(outputfile, 'r').readlines():
    if not line.startswith("SCHEDULING_SOLUTION"):
        continue
    
    found_solution = True
    words = line.rstrip().split(" ")
    cmax = int(words[1])
    solution = [int(x) for x in words[2:-1]]
    machines = [solution[i] for i in range(0, len(solution), 2)]
    starttimes = [solution[i] for i in range(1, len(solution), 2)]
    print(f"Found solution: Cmax={cmax} machines={machines} starttimes={starttimes}")
    break

# Check schedule
assert(found_solution) # no solution line found!
assert(len(machines) == n) # invalid number of scheduled machines given!
assert(len(starttimes) == n) # invalid number of start times given!
schedule = [[0 for t in range(cmax+1)] for i in range(m)]
for x in range(n):
    machine, start = machines[x]-1, starttimes[x]
    assert(machine >= 0 and machine < len(schedule)) # invalid machine index!
    for t in range(start, start+jobsizes[x]):
        assert(t < len(schedule[machine])) # schedule does not fit in [0, Cmax]!
        assert(schedule[machine][t] == 0) # machine has multiple jobs at the same time!
        schedule[machine][t] = x+1
any_machine_busy_before_cmax = False
for sched in schedule:
    any_machine_busy_before_cmax = any_machine_busy_before_cmax or sched[cmax-1] != 0
    assert(sched[cmax] == 0) # some machine is still working at t=Cmax! (off-by-one?)
assert(any_machine_busy_before_cmax) # all machines are done by Cmax-1, so Cmax is not tight!

# Print schedule
print("Schedule validated (correct, but not necessarily optimal).")
print("t", [x for x in range(cmax+1)])
for i in range(len(schedule)):
    print(str(i+1), schedule[i])
