# CS 6210 Assignment 4
## Author: Zebin Guo
## GTID: 904054219

This is the README file for the GTStore project. The project is a key-value store system that supports data partitioning, data replication, and data consistency. The project is implemented in C++.

Please kindly find the purpose of each bash file in the report (test case and performance analysis sections). The bash files are used to run the test cases and two analysis scripts.

The source code is in src folder. The functionalities of each file are as follows:
- client.cpp: the client library
- gtstore.hpp: the header file that defines the GTStore service as well as clients (may consider move client part out?).
- manager.cpp: the manager that manages the storage nodes and clients.
- storage.cpp: the storage nodes that store key-value pairs.
- test_app.cpp: the client driver application that interacts with the GTStore service.

You can use ```bash (file_name.sh)``` to run the test cases and performance analysis. Again, details can be found in the report.