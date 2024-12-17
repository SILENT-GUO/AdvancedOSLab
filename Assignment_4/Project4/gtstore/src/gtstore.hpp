#ifndef GTSTORE
#define GTSTORE

#include <string>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <unordered_map> // solely used for KV pair storage
#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>

#define MAX_KEY_BYTE_PER_REQUEST 20
#define MAX_VALUE_BYTE_PER_REQUEST 1000
#define MANAGER_NODES_SOCKET_PATH "/tmp/gtstore_manager_nodes_socket"
#define MANAGER_NODES_SOCKET_BUFFER_SIZE 1024
#define MANAGER_CLIENT_SOCKET_PATH "/tmp/gtstore_manager_client_socket"
#define MANAGER_CLIENT_SOCKET_BUFFER_SIZE 1024
#define NODE_CLIENT_SOCKET_PATH_PREFIX "/tmp/gtstore_node_client_socket_"
#define NODE_CLIENT_SOCKET_BUFFER_SIZE 1100 // larger than 1024 + 20 
#define NODE_NODE_SOCKET_PATH_PREFIX "/tmp/gtstore_node_node_socket_"
#define NODE_NODE_SOCKET_BUFFER_SIZE 1100

using namespace std;

typedef vector<string> val_t;

class GTStoreClient {
		private:
				int client_id;
				val_t value;
		public:
				void init(int id);
				void finalize();
				val_t get(string key);
				bool put(string key, val_t value);
				void notify_manager_of_dead_nodes(const vector<int>& dead_nodes);
};

class GTStoreManager {
		public:
				void init();
				void run();
				void initialize_manager_nodes_socket();
				void create_manager_client_socket();
				void send_node_replica_node_fds();
};

class GTStoreStorage {
		public:
				void init();
				void connect_to_manager();
				void run();
				void parse_node_connection_msg(string msg);
				void store_key_value(string key, string value);
};

#endif
