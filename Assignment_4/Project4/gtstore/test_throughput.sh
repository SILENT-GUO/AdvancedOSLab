#!/bin/bash

make clean
make

OUTPUT_FILE="throughput_results.txt"
echo "Throughput Results (Ops/s) against Number of Replicas" > $OUTPUT_FILE
echo "===================================" >> $OUTPUT_FILE

# Function to measure throughput
measure_throughput() {
    local replicas=$1

    echo "Starting throughput test with $replicas replicas."

    # Launch the GTStore Manager with 7 nodes and specified replicas
    ./bin/manager --nodes 7 --rep $replicas &
    MANAGER_PID=$!
    sleep 3

    # Launch 7 GTStore Storage Nodes
    STORAGE_PIDS=()
    for i in {1..7}; do
        ./bin/storage &
        STORAGE_PIDS+=($!)
        sleep 1
    done

    echo "Performing 200k operations (50/50 read/write mix)..."
    START_TIME=$(date +%s%N)

    # Perform 20k operations (50% writes, 50% reads)
    for i in $(seq 1 10000); do
        key="key$((i % 1000))" # Use a small range of keys to simulate contention
        ./bin/test_app --put $key --val "0" &
        ./bin/test_app --get $key &
    done

    # wait # Wait for all operations to complete
    END_TIME=$(date +%s%N)

    # Calculate throughput
    ELAPSED_TIME=$((($END_TIME - $START_TIME) / 1000000000)) # Convert to seconds
    THROUGHPUT=$((20000 / $ELAPSED_TIME)) # Ops/s

    echo "Throughput with $replicas replicas: $THROUGHPUT Ops/s"
    echo "$replicas replicas: $THROUGHPUT Ops/s" >> $OUTPUT_FILE

    # Clean up processes
    echo "Stopping storage nodes and manager for replicas=$replicas."
    for pid in ${STORAGE_PIDS[@]}; do
        kill $pid
        wait $pid 2>/dev/null
    done
    kill $MANAGER_PID
    wait $MANAGER_PID 2>/dev/null
}

# Test for 1, 3, and 5 replicas
for replicas in 1 3 5; do
    measure_throughput $replicas
done

echo "Throughput test completed. Results saved to $OUTPUT_FILE."
