import os

directory = os.fsencode("./bench/franca_frangioni/INSTANCES")

problems = [os.fsdecode(x) for x in os.listdir(directory)]
problems.sort()



for filename in problems:
    with open("./bench/franca_frangioni/INSTANCES/" + filename) as r:
        lines = r.readlines()
        if len(lines) != 3:
            continue
        lines = [x.strip() for x in lines]
        with open("./bench/franca_frangioni/standardised/"+filename, "w") as f:
            f.write("p p_cmax %s %s\n" % (lines[1], lines[0]) )
            
            out = lines[2].split()
            out.append("0")
            f.write(" ".join(out))
            

