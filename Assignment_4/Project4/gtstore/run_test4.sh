#!/bin/bash

make clean
make

# Launch the GTStore Manager with 7 nodes and replication factor 3
./bin/manager --nodes 7 --rep 3 &
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

# Insert around 20 keys across multiple servers
for i in {1..20}; do
    ./bin/test_app --put "key$i" --val "value$i"
done

# Overwrite at least 3 keys
./bin/test_app --put "key2" --val "new_value2"
./bin/test_app --put "key10" --val "new_value10"
./bin/test_app --put "key15" --val "new_value15"

# Kill 2 servers (e.g., the last two storage nodes)
for i in {6..7}; do
    echo "Killing storage node with PID: ${STORAGE_PIDS[$i-1]}"
    kill ${STORAGE_PIDS[$i-1]}
    wait ${STORAGE_PIDS[$i-1]} 2>/dev/null
    echo "Storage node $i terminated."
done
sleep 5

# Query the 20 keys to ensure the most recent values are retrieved
for i in {1..20}; do
    ./bin/test_app --get "key$i"
done

# # Optional: Clean up remaining processes
# echo "Stopping remaining storage nodes and manager."
# for i in {1..5}; do
#     kill ${STORAGE_PIDS[$i-1]}
#     wait ${STORAGE_PIDS[$i-1]} 2>/dev/null
# done
# kill $MANAGER_PID
# wait $MANAGER_PID 2>/dev/null
# echo "All processes terminated."
