import matplotlib.pyplot as plt
import numpy as np
import csv
from io import StringIO

# Data provided as a multi-line string
data = """Mode,NumFiles,FileArgument,Sec,NSec
SYNC,1,./bin/input/Medium.pdf,0,1750630
ASYNC,1,./bin/input/Medium.pdf,0,1687322
SYNC,80,./bin/input/filelist.txt,0,44120271
ASYNC,80,./bin/input/filelist.txt,0,53853240
SYNC,1,./bin/input/Medium.pdf,0,1396455
ASYNC,1,./bin/input/Medium.pdf,0,2294319
SYNC,80,./bin/input/filelist.txt,0,45467800
ASYNC,80,./bin/input/filelist.txt,0,30072203
SYNC,1,./bin/input/Medium.pdf,0,1405011
ASYNC,1,./bin/input/Medium.pdf,0,2126998
SYNC,80,./bin/input/filelist.txt,0,48516663
ASYNC,80,./bin/input/filelist.txt,0,20711798
SYNC,1,./bin/input/Medium.pdf,0,1864370
ASYNC,1,./bin/input/Medium.pdf,0,3047461
SYNC,80,./bin/input/filelist.txt,0,44228016
ASYNC,80,./bin/input/filelist.txt,0,19526940
SYNC,1,./bin/input/Medium.pdf,0,1503260
ASYNC,1,./bin/input/Medium.pdf,0,2326786
SYNC,80,./bin/input/filelist.txt,0,46869939
ASYNC,80,./bin/input/filelist.txt,0,18170473
SYNC,1,./bin/input/Medium.pdf,0,1923459
ASYNC,1,./bin/input/Medium.pdf,0,1926151
SYNC,80,./bin/input/filelist.txt,0,45588315
ASYNC,80,./bin/input/filelist.txt,0,18917703"""

# Shared memory sizes corresponding to each set
shared_memory_sizes = [256, 512, 1024, 2048, 4096, 8192]

# Read the data into a list of dictionaries
reader = csv.DictReader(StringIO(data))
data_list = list(reader)

# Initialize data structures
conditions = ['SYNC_1', 'ASYNC_1', 'SYNC_80', 'ASYNC_80']
condition_data = {condition: {'shm_sizes': [], 'avg_times': []} for condition in conditions}

# Process the data
num_sets = len(shared_memory_sizes)
rows_per_set = 4  # As per the user's description

for i in range(num_sets):
    shm_size = shared_memory_sizes[i]
    # Get the data for this set
    set_data = data_list[i * rows_per_set:(i + 1) * rows_per_set]
    for row in set_data:
        mode = row['Mode']
        num_files = int(row['NumFiles'])
        sec = int(row['Sec'])
        nsec = int(row['NSec'])
        time_elapsed = sec + nsec / 1e9  # Total time in seconds
        avg_time = time_elapsed / num_files  # Average time per file
        if mode == 'SYNC' and num_files == 1:
            condition = 'SYNC_1'
        elif mode == 'ASYNC' and num_files == 1:
            condition = 'ASYNC_1'
        elif mode == 'SYNC' and num_files == 80:
            condition = 'SYNC_80'
        elif mode == 'ASYNC' and num_files == 80:
            condition = 'ASYNC_80'
        else:
            continue  # Skip if condition not recognized
        condition_data[condition]['shm_sizes'].append(shm_size)
        condition_data[condition]['avg_times'].append(avg_time)

# Generate the four figures
for condition in conditions:
    plt.figure()
    shm_sizes = condition_data[condition]['shm_sizes']
    avg_times = condition_data[condition]['avg_times']
    plt.plot(shm_sizes, avg_times, marker='o')
    plt.xlabel('Shared Memory Size')
    plt.ylabel('Average Time per File (s)')
    plt.title(f'Time vs Shared Memory Size for {condition}')
    plt.grid(True)
    # Save the figure as a JPG file with the condition in the filename
    plt.savefig(f'{condition}_time_vs_shm_size.jpg', format='jpg')
    plt.close()  # Close the figure to free memory

# Generate the fifth figure comparing the four conditions at 1024 shm_size
shm_size_of_interest = 1024
avg_times_at_1024 = []

for condition in conditions:
    # Find the index where shm_size == 1024
    try:
        index = condition_data[condition]['shm_sizes'].index(shm_size_of_interest)
        avg_time = condition_data[condition]['avg_times'][index]
    except ValueError:
        avg_time = None  # Shared memory size not found for this condition
    avg_times_at_1024.append(avg_time)

# Plotting the comparison
plt.figure()
x_pos = np.arange(len(conditions))
plt.bar(x_pos, avg_times_at_1024, align='center', alpha=0.7)
plt.xticks(x_pos, conditions)
plt.ylabel('Average Time per File (s)')
plt.title(f'Comparison at Shared Memory Size {shm_size_of_interest}')
plt.grid(True, axis='y')
# Save the figure as a JPG file
plt.savefig(f'Comparison_at_shm_size_{shm_size_of_interest}.jpg', format='jpg')
plt.close()  # Close the figure to free memory
