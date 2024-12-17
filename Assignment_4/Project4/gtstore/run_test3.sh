#/bin/bash

make clean
make

# Launch the GTStore Manager
./bin/manager --nodes 3 --rep 2 &
sleep 5

# Launch couple GTStore Storage Nodes
./bin/storage &
sleep 5
./bin/storage &
sleep 5
./bin/storage &
STORAGE3_PID=$!
echo "Storage node 3 started with PID: $STORAGE3_PID"
sleep 5

# Launch the client testing app
./bin/test_app --put key1 --val value1

./bin/test_app --put key2 --val value2

./bin/test_app --put key3 --val value3

./bin/test_app --put key4 --val value4

./bin/test_app --put key5 --val value5

./bin/test_app --put key6 --val value6

./bin/test_app --put key7 --val value7

./bin/test_app --put key8 --val value8

./bin/test_app --put key9 --val value9

# Kill the last storage node
echo "Killing the last storage node with PID: $STORAGE3_PID"
kill $STORAGE3_PID
wait $STORAGE3_PID 2>/dev/null
echo "Storage node 3 terminated."
sleep 3

./bin/test_app --get key1

./bin/test_app --get key2

./bin/test_app --get key3

./bin/test_app --get key4

./bin/test_app --get key5

./bin/test_app --get key6

./bin/test_app --get key7

./bin/test_app --get key8

./bin/test_app --get key9

