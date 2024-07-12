
import sys
import random

if len(sys.argv) < 5:
    print(f"Usage: {sys.argv[0]} n-jobs n-machines min-job-size max-job-size [seed]")
    exit(0)

n = int(sys.argv[1])
m = int(sys.argv[2])
minjobsize = int(sys.argv[3])
maxjobsize = int(sys.argv[4])
if len(sys.argv) >= 5:
    random.seed(int(sys.argv[5]))

print(f"p p_cmax {n} {m}")
jobdescription = ""
for i in range(n):
    jobsize = random.randrange(minjobsize, maxjobsize+1)
    assert(jobsize >= 1 and jobsize <= maxjobsize)
    jobdescription += str(jobsize) + " "
print(jobdescription + "0")
