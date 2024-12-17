#include "gtstore.hpp"

string node_client_socket_path = NODE_CLIENT_SOCKET_PATH_PREFIX + to_string(getpid());
string node_id = "";
int num_rep = -1;
vector<string> replica_node_addresses;

unordered_map<string, vector<string> > storage_key_value_map;

void GTStoreStorage::store_key_value(string key, string value_list) {
    // parse the value_list by space
    vector<string> values;
    size_t pos = 0;
    while ((pos = value_list.find(" ")) != string::npos) {
        values.push_back(value_list.substr(0, pos));
        value_list.erase(0, pos + 1);
    }
    if (!value_list.empty()) {
        values.push_back(value_list);
    }

    // store the key-value pair
    storage_key_value_map[key] = values;
}

void GTStoreStorage::parse_node_connection_msg(string buffer){
    size_t num_replicas_pos = buffer.find("NUM_REPLICAS:");
    size_t replica_paths_pos = buffer.find("REPLICA_PATHS:");
    
     if (num_replicas_pos != std::string::npos && replica_paths_pos != std::string::npos) {
        size_t num_start = num_replicas_pos + std::string("NUM_REPLICAS:").length();
        size_t num_end = buffer.find(';', num_start);
        std::string num_rep_str = buffer.substr(num_start, num_end - num_start);
        //global variable copy
        num_rep = std::atoi(num_rep_str.c_str());

        // Parse REPLICA_PATHS
        size_t paths_start = replica_paths_pos + std::string("REPLICA_PATHS:").length();
        std::string paths = buffer.substr(paths_start);
        
        // Split paths by commas
        size_t pos = 0;
        while ((pos = paths.find(',')) != std::string::npos) {
            replica_node_addresses.push_back(paths.substr(0, pos));
            paths.erase(0, pos + 1);
        }
        // Add the last path
        if (!paths.empty()) {
            replica_node_addresses.push_back(paths);
        }
    }

    // Verify the extracted values
    // cout << "Number of Replicas: " << num_rep << endl;
    // cout << "Replica Node Addresses: " << endl;
    // for (const auto &address : replica_node_addresses) {
    //     cout << address << endl;
    // }
}

void GTStoreStorage::connect_to_manager() {
    // cout << "Inside GTStoreStorage::connect_to_manager()\n";
    int manager_fd;
    char buffer[MANAGER_NODES_SOCKET_BUFFER_SIZE];

    // Create socket to connect to the manager
    if ((manager_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket error");
        exit(1);
    }

    struct sockaddr_un manager_node_socket_address;
    memset(&manager_node_socket_address, 0, sizeof(manager_node_socket_address));
    manager_node_socket_address.sun_family = AF_UNIX;
    strcpy(manager_node_socket_address.sun_path, MANAGER_NODES_SOCKET_PATH);

    // Connect to the manager
    if (connect(manager_fd, (struct sockaddr *)&manager_node_socket_address, sizeof(manager_node_socket_address)) == -1) {
        perror("connect error");
        close(manager_fd);
        exit(1);
    }

    // cout << "Connected to manager\n";

    // Send the node client socket path to the manager
    string msg = "NODE_CLIENT_SOCKET_PATH:" + node_client_socket_path;
    send(manager_fd, msg.c_str(), msg.length(), 0);

    // Receive the manager's response
    int n = recv(manager_fd, buffer, MANAGER_NODES_SOCKET_BUFFER_SIZE, 0);
    if (n < 0) {
        perror("recv error");
        close(manager_fd);
        exit(1);
    }
    buffer[n] = '\0';
    // cout << "Received message from manager: " << buffer << endl;
    string received_msg(buffer);

    

    // Parse response: "Node connected to manager:" + node_id
    if (received_msg.find("Node connected to manager:") != string::npos) {
        node_id = received_msg.substr(received_msg.find(":") + 1);
        // cout << "Node connected to manager with id: " << node_id << endl;
    } else {
        cout << "Invalid connection message received from manager\n";
        close(manager_fd);
        return;
    }

    close(manager_fd);
	// cout << "manager_fd closed\n";
	/*------------------------------------------------------------------------*/
	/*------------------------------------------------------------------------*/
    // Open the node client socket path to listen for manager's final message
	/*------------------------------------------------------------------------*/
	/*------------------------------------------------------------------------*/
    int node_fd;
    if ((node_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket error");
        exit(1);
    }

    struct sockaddr_un node_client_socket_address;
    memset(&node_client_socket_address, 0, sizeof(node_client_socket_address));
    node_client_socket_address.sun_family = AF_UNIX;
    strcpy(node_client_socket_address.sun_path, node_client_socket_path.c_str());

    // Unlink and bind the socket
    unlink(node_client_socket_path.c_str());
    if (bind(node_fd, (struct sockaddr *)&node_client_socket_address, sizeof(node_client_socket_address)) == -1) {
        perror("bind error");
        close(node_fd);
        exit(1);
    }

    // Listen for connections
    if (listen(node_fd, 10) == -1) {
        perror("listen error");
        close(node_fd);
        exit(1);
    }

    // cout << "Node client socket is listening on path: " << node_client_socket_path << endl;

    // Accept the manager's final message
    int conn_fd;
    if ((conn_fd = accept(node_fd, NULL, NULL)) == -1) {
        perror("accept error");
        close(node_fd);
        exit(1);
    }

    n = recv(conn_fd, buffer, MANAGER_NODES_SOCKET_BUFFER_SIZE, 0);
    if (n < 0) {
        perror("recv error");
        close(conn_fd);
        close(node_fd);
        exit(1);
    }
    buffer[n] = '\0';
    // cout << "Received final message from manager: " << buffer << endl;

    string final_msg(buffer);
    parse_node_connection_msg(final_msg);

    // Parse the final message as required
    close(conn_fd);
    close(node_fd);
	// cout << "Node client socket closed\n";
	unlink(node_client_socket_path.c_str());
}

void GTStoreStorage::init() {
	// cout << "Inside GTStoreStorage::init()\n";
	connect_to_manager();
	//initialize_storage_nodes_client_socket();
}


void GTStoreStorage::run() {
    // Ignore SIGPIPE signals to prevent the process from exiting unexpectedly
    signal(SIGPIPE, SIG_IGN);

    // Create node client socket using node_client_socket_path
    // cout << "Inside GTStoreStorage::run()\n";
    int node_fd, node_node_fd;
    char buffer[NODE_CLIENT_SOCKET_BUFFER_SIZE];
    char node_buffer[NODE_NODE_SOCKET_BUFFER_SIZE];

    // Create socket for client connections
    if ((node_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket error");
        exit(1);
    }

    // Create socket for node-to-node connections
    if ((node_node_fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0)) == -1) {
        perror("socket error");
        exit(1);
    }

    // Set up node client socket address
    struct sockaddr_un node_client_socket_address;
    memset(&node_client_socket_address, 0, sizeof(node_client_socket_address));
    node_client_socket_address.sun_family = AF_UNIX;
    strcpy(node_client_socket_address.sun_path, node_client_socket_path.c_str());

    unlink(node_client_socket_path.c_str());
    // Bind client socket
    if (bind(node_fd, (struct sockaddr *)&node_client_socket_address, sizeof(node_client_socket_address)) == -1) {
        perror("bind error");
        close(node_fd);
        exit(1);
    }

    // Listen on client socket
    if (listen(node_fd, 10) == -1) {
        perror("listen error");
        close(node_fd);
        exit(1);
    }

    // Set up node-to-node socket address
    string node_socket_path = NODE_NODE_SOCKET_PATH_PREFIX + node_id;
    struct sockaddr_un node_node_socket_address;
    memset(&node_node_socket_address, 0, sizeof(node_node_socket_address));
    node_node_socket_address.sun_family = AF_UNIX;
    strcpy(node_node_socket_address.sun_path, node_socket_path.c_str());

    unlink(node_socket_path.c_str());
    // Bind node-to-node socket
    if (bind(node_node_fd, (struct sockaddr *)&node_node_socket_address, sizeof(node_node_socket_address)) == -1) {
        perror("bind error");
        close(node_node_fd);
        exit(1);
    }

    // Listen on node-to-node socket
    if (listen(node_node_fd, 10) == -1) {
        perror("listen error");
        close(node_node_fd);
        exit(1);
    }

    // cout << "Node client socket listening at " << node_client_socket_path << endl;
    // cout << "Node node-to-node socket listening at " << node_socket_path << endl;

    // Main loop to handle client and node connections sequentially
    while (1) {
        // Accept client connections
        // cout << "Waiting for client connection..." << endl;
        int client_fd = accept(node_fd, NULL, NULL);
        if (client_fd >= 0) {
            // cout << "Accepted connection from client\n";
            // Handle client connection
            while (1) {
                int n = recv(client_fd, buffer, NODE_CLIENT_SOCKET_BUFFER_SIZE, 0);
                if (n > 0) {
                    buffer[n] = '\0';
                    // cout << "Received message from client: " << buffer << endl;
                    string received_msg(buffer);
                    // Handle messages
                    if (received_msg.find("PUT:") != string::npos) {
                        // Handle PUT request
                        string key_value = received_msg.substr(received_msg.find(":") + 1);
                        size_t space_pos = key_value.find(" ");
                        string key = key_value.substr(0, space_pos);
                        string value = key_value.substr(space_pos + 1);
                        // cout << "Received key: " << key << " value: " << value << endl;
                        // Store the key-value pair locally
                        store_key_value(key, value);

                        // Send replication messages to other nodes
                        string replication_msg = "REPLICATE:" + key + " " + value;
                        for (const auto& replica_path : replica_node_addresses) {
                            // Send replication message to each replica
                            int replica_fd = socket(AF_UNIX, SOCK_STREAM, 0);
                            if (replica_fd == -1) {
                                perror("socket error");
                                continue;
                            }

                            struct sockaddr_un replica_addr;
                            memset(&replica_addr, 0, sizeof(replica_addr));
                            replica_addr.sun_family = AF_UNIX;
                            strcpy(replica_addr.sun_path, replica_path.c_str());

                            if (connect(replica_fd, (struct sockaddr *)&replica_addr, sizeof(replica_addr)) == -1) {
                                perror("connect error");
                                close(replica_fd);
                                continue;
                            }

                            send(replica_fd, replication_msg.c_str(), replication_msg.length(), 0);
                            // cout << "Sent replication message " << replication_msg << " to replica at " << replica_path << endl;
                            close(replica_fd);
                        }

                        // Send response to client
                        string msg = "OK, SERVER_" + node_id;
                        send(client_fd, msg.c_str(), msg.length(), 0);
                    }
                    else if (received_msg.find("GET:") != string::npos) {
                        // Handle GET request
                        string key = received_msg.substr(received_msg.find(":") + 1);
                        // cout << "Received key: " << key << endl;
                        // Retrieve the value from local storage
                        if (storage_key_value_map.find(key) == storage_key_value_map.end()) {
                            // cout << "Key not found at node" << node_id << endl;
                            string msg = "KEY_NOT_FOUND";
                            send(client_fd, msg.c_str(), msg.length(), 0);
                        } else {
                            vector<string> values = storage_key_value_map[key];
                            // cout << "Retrieved value: ";
                            string value = "";
                            for (const auto& val : values) {
                                // cout << val << " ";
                                value += val + " ";
                            }
                            // Send response to client
                            string msg = key + ", " + value + ", SERVER_" + node_id;
                            send(client_fd, msg.c_str(), msg.length(), 0);
                        }
                    }
                    else if (received_msg.find("CLIENT_CLOSE") != string::npos) {
                        // cout << "Client requested to close the connection\n";
                        break; // Exit the inner loop to accept the next client
                    }
                    else {
                        cout << "Invalid message received from client\n";
                        // Optionally send an error message back to client
                    }
                }
                else if (n == 0) {
                    // Connection closed by client
                    // cout << "Client disconnected\n";
                    break;
                }
                else {
                    perror("recv error");
                    break;
                }
            }
            close(client_fd); // Close the client connection after handling
            // cout << "Client connection closed\n";
        } else {
            perror("accept error on client socket");
            // Depending on the error, you might want to continue or exit
            continue;
        }

        // Accept node-to-node connections
        // cout << "Waiting for node-to-node connection..." << endl;
        int node_conn_fd = accept(node_node_fd, NULL, NULL);
        if (node_conn_fd >= 0) {
            // cout << "Accepted connection from another node\n";
            // Handle node-to-node connection
            while (1) {
                int n = recv(node_conn_fd, node_buffer, NODE_NODE_SOCKET_BUFFER_SIZE, 0);
                if (n > 0) {
                    node_buffer[n] = '\0';
                    // cout << "Received message from node: " << node_buffer << endl;
                    string received_msg(node_buffer);
                    if (received_msg.find("REPLICATE:") != string::npos) {
                        string key_value = received_msg.substr(received_msg.find(":") + 1);
                        size_t space_pos = key_value.find(" ");
                        string key = key_value.substr(0, space_pos);
                        string value = key_value.substr(space_pos + 1);
                        // cout << "Replicated key: " << key << " value: " << value << " at node " << node_id << endl;
                        // Store the key-value pair locally
                        store_key_value(key, value);
                    }
                    else if (received_msg.find("NODE_CLOSE") != string::npos) {
                        // cout << "Node requested to close the connection\n";
                        break; // Exit the inner loop to accept the next node
                    }
                    else {
                        cout << "Invalid message received from node\n";
                    }
                }
                else if (n == 0) {
                    // Connection closed by node
                    // cout << "Node disconnected\n";
                    break;
                }
                else {
                    perror("recv error");
                    break;
                }
            }
            close(node_conn_fd); // Close the node connection after handling
            // cout << "Node connection closed\n";
        } else {
            // perror("accept error on node-to-node socket");
            // Depending on the error, you might want to continue or exit
            continue;
        }
    }

    // Clean up
    close(node_fd);
    close(node_node_fd);
    unlink(node_client_socket_path.c_str());
    unlink(node_socket_path.c_str());
}

int main(int argc, char **argv) {

	GTStoreStorage storage;
	storage.init();
	storage.run();
	
}
