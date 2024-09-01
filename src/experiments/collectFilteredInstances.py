import os
import argparse

def main(base_path):
    output_lines = []
    
    for filename in os.listdir(base_path):
        if filename.startswith("filteredRun"):
            with open(os.path.join(base_path, filename), 'r') as file:
                for line in file:
                    line = line.strip()
                    if line.startswith(("Computed", "nohup", "found")):
                        continue
                    if line.endswith("(canceled)") or line.endswith("0)"):
                        continue
                    instance_name = line.split()[0]
                    output_lines.append(instance_name)
    
    with open(os.path.join(base_path, "filtered_instances.txt"), 'w') as output_file:
        for line in output_lines:
            output_file.write(line + '\n')

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Process some files.")
    parser.add_argument('base_path', type=str, help='Base path to read files from')
    args = parser.parse_args()
    main(args.base_path)
