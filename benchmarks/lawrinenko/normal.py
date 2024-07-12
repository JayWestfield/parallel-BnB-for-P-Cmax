
import sys
import random

if len(sys.argv) < 5:
    print(f"Usage: {sys.argv[0]} n-jobs n-machines mean s-div [seed]")
    exit(0)

n = int(sys.argv[1])
m = int(sys.argv[2])
mean = int(sys.argv[3])
s_div = int(sys.argv[4])
if len(sys.argv) >= 5:
    random.seed(int(sys.argv[5]))

print(f"p p_cmax {n} {m}")
jobdescription = ""
i = 0
while i < n:
    jobsize = int(random.normalvariate(mean, s_div))
    if jobsize <= 0:
        continue
    jobdescription += str(jobsize) + " "
    i+=1
print(jobdescription + "0")
