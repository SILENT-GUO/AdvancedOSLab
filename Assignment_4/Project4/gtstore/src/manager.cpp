#include "gtstore.hpp"
#include <sys/socket.h>
#include <sys/un.h>
#include <getopt.h>

int num_nodes = 0, num_rep = 0;
vector<string> node_address_map; //index is node_id, value is node_address
vector<int> dead_node_ids;

void GTStoreManager::initialize_manager_nodes_socket() {
	// cout << "Inside GTStoreManager::initialize_manager_nodes_socket()\n";
	int manager_fd, node_fd;
	int current_unconnected_nodes = num_nodes;
	char buffer[MANAGER_NODES_SOCKET_BUFFER_SIZE];

	//create socket
	if ((manager_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		perror("socket error");
		exit(1);
	}
	unlink(MANAGER_NODES_SOCKET_PATH);

	struct sockaddr_un manager_node_socket_address;
	memset(&manager_node_socket_address, 0, sizeof(manager_node_socket_address));
	manager_node_socket_address.sun_family = AF_UNIX;
	strcpy(manager_node_socket_address.sun_path, MANAGER_NODES_SOCKET_PATH);

	//bind socket
	if (bind(manager_fd, (struct sockaddr *)&manager_node_socket_address, sizeof(manager_node_socket_address)) == -1) {
		perror("bind error");
		close(manager_fd);
		exit(1);
	}

	//listen
	if (listen(manager_fd, 10) == -1) {
		perror("listen error");
		close(manager_fd);
		exit(1);
	}

	// cout << "Manager node socket created and listened\n";

	//accept
	while((node_fd = accept(manager_fd, NULL, NULL)) != -1) {
		// cout << "Accepted connection from node\n";
		int n = recv(node_fd, buffer, MANAGER_NODES_SOCKET_BUFFER_SIZE, 0);
		if (n < 0) {
			perror("recv error");
			close(node_fd);
			exit(1);
		}
		buffer[n] = '\0';
		// cout << "Received message from node: " << buffer << endl;
		string received_msg(buffer);
		if(received_msg.find("NODE_CLIENT_SOCKET_PATH:") != string::npos) {
			int node_id = node_address_map.size();
			string node_client_socket_path = received_msg.substr(received_msg.find(":") + 1);
			node_address_map.push_back(node_client_socket_path);
			// cout << "New key-value pair added to node_id_path_map: " << node_id << " -> " << node_client_socket_path << endl;
			string msg = "Node connected to manager:" + to_string(node_id);
			send(node_fd, msg.c_str(), msg.length(), 0);
			current_unconnected_nodes--;
		}else{
			// cout << "Invalid connection message received from node\n";
		}
		close(node_fd);
		if(current_unconnected_nodes == 0) {
			break;
		}
	}

	if(node_fd == -1) {
		perror("accept error");
		close(manager_fd);
		exit(1);
	}
	if(current_unconnected_nodes > 0) {
		cout << "Error: Not all nodes connected to manager\n";
		close(manager_fd);
		exit(1);
	}else{
		// cout << "All nodes connected to manager, exit manager node socket\n";
	}

	close(manager_fd);
	unlink(MANAGER_NODES_SOCKET_PATH);
	return;
}

void GTStoreManager::send_node_replica_node_fds() {
    // cout << "Inside GTStoreManager::send_node_replica_node_fds()\n";

    for (size_t i = 0; i < node_address_map.size(); i++) {
        vector<string> replica_paths;
        for (int j = 1; j <= num_rep - 1; j++) {
			// cout << "i: " << i << " j: " << j << " node_address_map.size(): " << node_address_map.size() << endl;
            int replica_index = (node_address_map.size() + i + j) % node_address_map.size();
            replica_paths.push_back(NODE_NODE_SOCKET_PATH_PREFIX + to_string(replica_index));
        }

        string message = "NUM_REPLICAS:" + to_string(num_rep) + ";";
        message += "REPLICA_PATHS:";
        for (const auto &path : replica_paths) {
            message += path + ",";
        }
        message.pop_back(); // Remove trailing comma

        int node_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (node_fd == -1) {
            perror("socket error");
            exit(1);
        }

        struct sockaddr_un node_address;
        memset(&node_address, 0, sizeof(node_address));
        node_address.sun_family = AF_UNIX;
        strcpy(node_address.sun_path, node_address_map[i].c_str());

        int retry_count = 5;
        while (retry_count--) {
            if (connect(node_fd, (struct sockaddr *)&node_address, sizeof(node_address)) == 0) {
                break;
            }
            perror("connect error");
            sleep(1);
        }

        if (retry_count < 0) {
            cerr << "Failed to connect to node: " << node_address_map[i] << endl;
            close(node_fd);
            exit(1);
        }

        if (send(node_fd, message.c_str(), message.size(), 0) == -1) {
            perror("send error");
        } else {
            // cout << "Sent message to node " << i << ": " << message << endl;
        }

        close(node_fd);
    }
}


void GTStoreManager::init() {
	// cout << "Inside GTStoreManager::init()\n";
	initialize_manager_nodes_socket();
	send_node_replica_node_fds();
}

void GTStoreManager::create_manager_client_socket() {
    // cout << "Inside GTStoreManager::create_manager_client_socket()\n";
    int manager_fd, client_fd;
    char buffer[MANAGER_CLIENT_SOCKET_BUFFER_SIZE];

    // Create socket
    if ((manager_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket error");
        exit(1);
    }

    unlink(MANAGER_CLIENT_SOCKET_PATH);

    struct sockaddr_un manager_client_socket_address;
    memset(&manager_client_socket_address, 0, sizeof(manager_client_socket_address));
    manager_client_socket_address.sun_family = AF_UNIX;
    strcpy(manager_client_socket_address.sun_path, MANAGER_CLIENT_SOCKET_PATH);

    // Bind socket
    if (bind(manager_fd, (struct sockaddr *)&manager_client_socket_address, sizeof(manager_client_socket_address)) == -1) {
        perror("bind error");
        close(manager_fd);
        exit(1);
    }

    // Listen
    if (listen(manager_fd, 10) == -1) {
        perror("listen error");
        close(manager_fd);
        exit(1);
    }

    // cout << "Manager client socket created and listening\n";

    // Accept connections
    while ((client_fd = accept(manager_fd, NULL, NULL)) != -1) {
        // cout << "Accepted connection from client\n";
        int n = recv(client_fd, buffer, MANAGER_CLIENT_SOCKET_BUFFER_SIZE, 0);
        if (n < 0) {
            perror("recv error");
            close(client_fd);
            continue;
        }
        buffer[n] = '\0';
        // cout << "Received message from client: " << buffer << endl;
        string received_msg(buffer);

        if (received_msg == "INITIALIZE_CLIENT") {
            // Serialize node_id_path_map and hashing function
            string response = "NUM_REPLICAS:" + to_string(num_rep) + ";HASH_FUNCTION:ADD-ASCII;NODES:";
            for (auto i = 0u; i < node_address_map.size(); i++) {
                response += to_string(i) + ":" + node_address_map[i] + ",";
            }
            for(int dead_node_id : dead_node_ids) {
                response += to_string(dead_node_id) + ":DEAD,";
            }
            response.pop_back(); // Remove trailing comma

            // Send response to client
            send(client_fd, response.c_str(), response.size(), 0);
            // cout << "Sent hashing function and node map to client\n";

        } else if (received_msg.find("DEAD_NODES:") == 0) {
            // Handle dead nodes notification
            // cout << "Processing dead nodes notification from client\n";
            string dead_nodes_str = received_msg.substr(strlen("DEAD_NODES:"));
            vector<int> dead_node_ids;

            // Parse the node IDs
            size_t pos = 0;
            while ((pos = dead_nodes_str.find(",")) != string::npos) {
                string node_id_str = dead_nodes_str.substr(0, pos);
                int node_id = stoi(node_id_str);
                dead_node_ids.push_back(node_id);
                dead_nodes_str.erase(0, pos + 1);
            }
            // Add the last node ID
            if (!dead_nodes_str.empty()) {
                int node_id = stoi(dead_nodes_str);
                dead_node_ids.push_back(node_id);
            }

            for (int node_id : dead_node_ids) {
                cout << "Node " << node_id << " reported as dead\n";
				// try to remove the dead node from the node_address_map
				if(node_id < node_address_map.size()) {
					dead_node_ids.push_back(node_id);
				}
            }

            // Optionally, send acknowledgment to the client
            string ack_msg = "DEAD_NODES_RECEIVED";
            send(client_fd, ack_msg.c_str(), ack_msg.length(), 0);

        } else {
            cout << "Unknown message received from client: " << received_msg << endl;
            // Optionally, send an error message back to the client
            string error_msg = "ERROR: Unknown message";
            send(client_fd, error_msg.c_str(), error_msg.length(), 0);
        }
        close(client_fd);
    }

    // Close the manager socket
    close(manager_fd);
    unlink(MANAGER_CLIENT_SOCKET_PATH);
}

void GTStoreManager::run() {
	// cout << "Inside GTStoreManager::run()\n";
	create_manager_client_socket();
}

int main(int argc, char **argv) {
    int opt;

    // Define long options
    struct option long_options[] = {
        {"nodes", required_argument, 0, 'n'},
        {"rep", required_argument, 0, 'r'},
        {0, 0, 0, 0} // End of options
    };

    // Parse command-line arguments
    while ((opt = getopt_long(argc, argv, "", long_options, NULL)) != -1) {
        switch (opt) {
        case 'n': // --nodes
            num_nodes = atoi(optarg);
            break;
        case 'r': // --rep
            num_rep = atoi(optarg);
            break;
        default:
            cerr << "Usage: " << argv[0] << " --nodes <number> --rep <number>" << endl;
            exit(EXIT_FAILURE);
        }
    }

    // Validate parsed values
    if (num_nodes <= 0 || num_rep <= 0) {
        cerr << "Error: Both --nodes and --rep must be positive integers." << endl;
        cerr << "Usage: " << argv[0] << " --nodes <number> --rep <number>" << endl;
        exit(EXIT_FAILURE);
    }

    cout << "Parsed arguments:" << endl;
    cout << "Nodes: " << num_nodes << endl;
    cout << "Replicas: " << num_rep << endl;
	// node_address_map.resize(num_nodes);

	GTStoreManager manager;
	manager.init();
	manager.run();
	
}
