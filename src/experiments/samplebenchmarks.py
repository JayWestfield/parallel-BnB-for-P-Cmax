import sys
import random

def sample_lines(file_path, sample_size=100, seed=42):

    try:
        with open(file_path, 'r') as file:
            lines = file.readlines()

        if len(lines) <= sample_size:
            sampled_lines = lines
        else:
            # Set the random seed for reproducibility
            random.seed(seed)
            sampled_indices = random.sample(range(len(lines)), sample_size)
            sampled_lines = [lines[i] for i in sorted(sampled_indices)]

        for line in sampled_lines:
            name = line.split()[0]
            print(name)

    except FileNotFoundError:
        print(f"Error: File not found at path '{file_path}'")
    except Exception as e:
        print(f"An error occurred: {e}")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python script.py <file_path> [<sample_size>]")
    else:
        file_path = sys.argv[1]
        if (len(sys.argv) == 3):
            sample_size = int(sys.argv[2])
            sample_lines(file_path, sample_size)
        else: sample_lines(file_path)