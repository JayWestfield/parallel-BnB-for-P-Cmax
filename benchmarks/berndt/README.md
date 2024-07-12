
# P||Cmax Scheduling Competition

You can find an example instance `p_cmax-n5-m3-maxsize50.txt` in this directory.
A possible corresponding solution file is given as `solution-p_cmax-n5-m3-maxsize50.txt`.

## Generating Benchmarks

Execute the following command to generate some benchmark instances:

`bash generate-benchmark-suite.sh`

Please tweak the parameters in that script if you find the arising instances to be too easy or too difficult for sensible performance tests of your approach (see "evaluation remarks" below).

## Validating Solutions

You can use the script `validate-solution.py` to check a solution output by your approach. This script does **not** check for optimality - it only checks for a sound schedule. Run the script as follows:

`python3 validate-solution.py INSTANCE_FILE SOLUTION_FILE`

If you just want a pretty print of a (correct) schedule, you can filter and "beautify" the output a little bit. For example:

`python3 validate-solution.py p_cmax-n5-m3-maxsize50.txt solution-p_cmax-n5-m3-maxsize50.txt|grep -E "^(t|[0-9]+) "|sed 's/, //g'|column -t`

## Evaluation Remarks

The submissions will be evaluated with respect to PAR-2 scores at a per-instance time limit of 300s. The benchmark set is chosen in such a way that all approches time out for some of the most difficult instances.
