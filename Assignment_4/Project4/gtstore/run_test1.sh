#/bin/bash

make clean
make

# Launch the GTStore Manager
./bin/manager --nodes 1 --rep 1 &
sleep 5

# Launch couple GTStore Storage Nodes
./bin/storage &
sleep 5


# Launch the client testing app
# Usage: ./bin/test_app <test> <client_id>
./bin/test_app --put key1 --val value1

./bin/test_app --get key1

./bin/test_app --put key1 --val value2

./bin/test_app --put key2 --val value3

./bin/test_app --put key3 --val value4

./bin/test_app --get key1

./bin/test_app --get key2

./bin/test_app --get key3
