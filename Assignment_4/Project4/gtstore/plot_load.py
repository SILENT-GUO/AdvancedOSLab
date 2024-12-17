import matplotlib.pyplot as plt

# Data: Node IDs and their corresponding number of keys
nodes = [6, 5, 4, 3, 2, 1, 0]
keys = [14290, 14271, 14263, 14271, 14291, 14307, 14307]

# Plot the data as a bar chart
plt.figure(figsize=(8, 6))
plt.bar(nodes, keys, color='skyblue', edgecolor='black')

# Add labels and title
plt.xlabel("Node ID", fontsize=12)
plt.ylabel("Number of Keys", fontsize=12)
plt.title("Histogram: Number of Keys vs. Node ID", fontsize=14)

# Annotate the bars with the key counts
for i, v in enumerate(keys):
    plt.text(nodes[i], v + 50, str(v), ha='center', fontsize=10)

# Adjust x-axis order to match node IDs
plt.xticks(nodes)

# Add grid
plt.grid(axis='y', linestyle='--', alpha=0.7)

# Save the plot as an image
plt.tight_layout()
plt.savefig("keys_vs_node_id_histogram.png")

# Show the plot
plt.show()
