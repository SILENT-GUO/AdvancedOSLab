import matplotlib.pyplot as plt

# Throughput data
replicas = [1, 3, 5]
throughput = [6666, 2857, 1666]

# Plot the data
plt.figure(figsize=(8, 6))
plt.plot(replicas, throughput, marker='o', linestyle='-', linewidth=2)

# Add labels and title
plt.xlabel("Number of Replicas", fontsize=12)
plt.ylabel("Throughput (Ops/s)", fontsize=12)
plt.title("Throughput Results (Ops/s) vs Number of Replicas", fontsize=14)

# Annotate data points
for x, y in zip(replicas, throughput):
    plt.text(x, y, f"{y}", fontsize=10, ha='center', va='bottom')

# Grid and show
plt.grid(True, linestyle='--', alpha=0.7)
plt.tight_layout()

# Save the plot as an image
plt.savefig("throughput_vs_replicas.png")

# Show the plot
plt.show()
