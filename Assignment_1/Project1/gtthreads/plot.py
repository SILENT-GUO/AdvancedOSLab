import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

# Function to create 2x2 grid plots for execution time with CPU time, wait time, and exec time
def plot_stacked_bars(csv_file_lb, csv_file_no_lb, title_prefix):
    # Read both CSV files into DataFrames
    df_lb = pd.read_csv(csv_file_lb, sep='\t')
    df_no_lb = pd.read_csv(csv_file_no_lb, sep='\t')

    # Add a new column to extract matrix size from 'group_name'
    df_lb['matrix_size'] = df_lb['group_name'].apply(lambda x: int(x.split('_m_')[-1]))
    df_no_lb['matrix_size'] = df_no_lb['group_name'].apply(lambda x: int(x.split('_m_')[-1]))

    # Order the groups by c_25, c_50, c_75, and c_100
    order = ['c_25', 'c_50', 'c_75', 'c_100']
    
    # Define unique matrix sizes from the dataset
    matrix_sizes = sorted(df_lb['matrix_size'].unique())

    # Load Balancing Plot (2x2 grid)
    fig, axs = plt.subplots(2, 2, figsize=(12, 10))
    fig.suptitle(f'{title_prefix} - Load Balancing')

    for i, matrix_size in enumerate(matrix_sizes):
        # Filter data for the current matrix size
        df_lb_filtered = df_lb[df_lb['matrix_size'] == matrix_size]

        # Ensure the data is ordered by the correct group names
        df_lb_filtered['group_prefix'] = df_lb_filtered['group_name'].apply(lambda x: x.split('_m_')[0])
        df_lb_filtered = df_lb_filtered.set_index('group_prefix').reindex(order).reset_index()

        # Extract group names, mean times, etc.
        group_names_lb = df_lb_filtered['group_name']
        cpu_times_lb = df_lb_filtered['mean_cpu_time(us)']
        wait_times_lb = df_lb_filtered['mean_wait_time(us)']
        exec_times_lb = df_lb_filtered['mean_exec_time(us)']

        # Create a range for the x-axis (group numbers)
        x_axis = np.arange(len(group_names_lb))  # Generate index values

        # Define width of the bars
        bar_width = 0.35

        # Plot Load Balancing - Stacked Bar Chart in the correct subplot
        ax = axs[i // 2, i % 2]  # Access the subplot
        ax.bar(x_axis - bar_width/3, cpu_times_lb, width=bar_width/3, label="CPU Time (LB)", color='green', bottom=0)
        ax.bar(x_axis, wait_times_lb, width=bar_width/3, label="Wait Time (LB)", color='orange', bottom=0)
        ax.bar(x_axis + bar_width/3, exec_times_lb, width=bar_width/3, label="Execution Time (LB)", color='purple', bottom=0)
        ax.set_xlabel('Group Name')
        ax.set_ylabel('Time (us)')
        ax.set_title(f'Matrix {matrix_size}')
        ax.set_xticks(x_axis)
        ax.set_xticklabels(group_names_lb, rotation=45)
        ax.legend()

    plt.tight_layout(rect=[0, 0, 1, 0.96])  # Adjust layout to not overlap with title
    plt.savefig(f'{title_prefix}_load_balancing_2x2.jpg')
    plt.close()

    # No Load Balancing Plot (2x2 grid)
    fig, axs = plt.subplots(2, 2, figsize=(12, 10))
    fig.suptitle(f'{title_prefix} - No Load Balancing')

    for i, matrix_size in enumerate(matrix_sizes):
        # Filter data for the current matrix size
        df_no_lb_filtered = df_no_lb[df_no_lb['matrix_size'] == matrix_size]

        # Ensure the data is ordered by the correct group names
        df_no_lb_filtered['group_prefix'] = df_no_lb_filtered['group_name'].apply(lambda x: x.split('_m_')[0])
        df_no_lb_filtered = df_no_lb_filtered.set_index('group_prefix').reindex(order).reset_index()

        # Extract group names, mean times, etc.
        group_names_no_lb = df_no_lb_filtered['group_name']
        cpu_times_no_lb = df_no_lb_filtered['mean_cpu_time(us)']
        wait_times_no_lb = df_no_lb_filtered['mean_wait_time(us)']
        exec_times_no_lb = df_no_lb_filtered['mean_exec_time(us)']

        # Create a range for the x-axis (group numbers)
        x_axis = np.arange(len(group_names_no_lb))  # Generate index values

        # Plot No Load Balancing - Stacked Bar Chart in the correct subplot
        ax = axs[i // 2, i % 2]  # Access the subplot
        ax.bar(x_axis - bar_width/3, cpu_times_no_lb, width=bar_width/3, label="CPU Time (No LB)", color='blue', bottom=0)
        ax.bar(x_axis, wait_times_no_lb, width=bar_width/3, label="Wait Time (No LB)", color='red', bottom=0)
        ax.bar(x_axis + bar_width/3, exec_times_no_lb, width=bar_width/3, label="Execution Time (No LB)", color='purple', bottom=0)
        ax.set_xlabel('Group Name')
        ax.set_ylabel('Time (us)')
        ax.set_title(f'Matrix {matrix_size}')
        ax.set_xticks(x_axis)
        ax.set_xticklabels(group_names_no_lb, rotation=45)
        ax.legend()

    plt.tight_layout(rect=[0, 0, 1, 0.96])  # Adjust layout to not overlap with title
    plt.savefig(f'{title_prefix}_no_load_balancing_2x2.jpg')
    plt.close()

# Compare both cumulative files and save the stacked bar plots as jpg files
plot_stacked_bars('Cumulative_output_lb.csv', 'Cumulative_output_no_lb.csv', 'Comparison')
