#!/bin/bash

make clean
make

# Launch the GTStore Manager with 7 nodes and 1 replica
./bin/manager --nodes 7 --rep 1 &
MANAGER_PID=$!
echo "Manager started with PID: $MANAGER_PID"
sleep 5

# Launch 7 GTStore Storage Nodes
STORAGE_PIDS=()
for i in {1..7}; do
    ./bin/storage &
    STORAGE_PIDS+=($!)
    echo "Storage node $i started with PID: ${STORAGE_PIDS[$i-1]}"
    sleep 3
done

# Perform 100k inserts and capture the node ID from success messages
echo "Performing 100k inserts..."
declare -A node_key_count

for i in $(seq 1 100000); do
    output=$(./bin/test_app --put "key$i" --val "value$i" 2>/dev/null)
    if [[ $output =~ OK,\ SERVER_([0-9]+) ]]; then
        node_id=${BASH_REMATCH[1]}
        ((node_key_count[$node_id]++))
    fi
done
echo "100k inserts completed."

# Generate histogram
echo "Generating histogram..."
HISTOGRAM_FILE="loadbalance_histogram.txt"
echo "Histogram: Number of Keys vs. Node ID" > $HISTOGRAM_FILE
echo "===================================" >> $HISTOGRAM_FILE
for node_id in "${!node_key_count[@]}"; do
    echo "Node $node_id: ${node_key_count[$node_id]} keys" >> $HISTOGRAM_FILE
done

# Display the histogram
cat $HISTOGRAM_FILE

# Clean up processes
echo "Stopping storage nodes and manager."
for pid in ${STORAGE_PIDS[@]}; do
    kill $pid
    wait $pid 2>/dev/null
done
kill $MANAGER_PID
wait $MANAGER_PID 2>/dev/null

echo "All processes terminated. Histogram saved to $HISTOGRAM_FILE."
