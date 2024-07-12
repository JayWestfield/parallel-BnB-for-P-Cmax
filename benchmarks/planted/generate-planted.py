
import random
import sys
import math

n = int(sys.argv[1])
m = int(sys.argv[2])
U = int(sys.argv[3])

perturb_ratio = float(sys.argv[4]) if len(sys.argv) > 4 else 0

job_matrix = [[i for t in range(U)] for i in range(m)]

nb_jobs = m

while nb_jobs < n:
    
    # cut each machine AT LEAST ONCE
    rand_i = nb_jobs-m if nb_jobs < 2*m else random.randrange(0, len(job_matrix))
    rand_t = random.randrange(0, len(job_matrix[rand_i]))
    newjob = nb_jobs
    #print(rand_i, rand_t, newjob)
    
    # improper cut if the 1st position of a job was hit
    if rand_t == 0 or job_matrix[rand_i][rand_t-1] != job_matrix[rand_i][rand_t]:
        continue
    
    # overwrite part of old machine with new job
    t = rand_t
    oldjob = job_matrix[rand_i][rand_t]
    while t < len(job_matrix[rand_i]) and job_matrix[rand_i][t] == oldjob:
        job_matrix[rand_i][t] = newjob
        t += 1

    nb_jobs += 1

sizes = dict()
for T in job_matrix:
    for j in T:
        if j not in sizes:
            sizes[j] = 0
        sizes[j] += 1

if perturb_ratio > 0:
    jlist = [key for key in sizes]
    random.shuffle(jlist)
    choice = jlist[0:math.ceil(perturb_ratio*len(sizes))]
    for j in choice:
        sizes[j] += 1

for j in sizes:
    print(sizes[j])
            
