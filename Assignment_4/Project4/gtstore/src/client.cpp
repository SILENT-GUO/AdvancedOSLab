#include "gtstore.hpp"

int num_replicas = -1;
vector<string> node_address_map;
vector<int> dead_node_ids;
vector<int> node_fd_list;
string hash_function = "";

void GTStoreClient::init(int id)
{
    // cout << "Inside GTStoreClient::init() for client " << id << "\n";
    client_id = id;
    int client_fd;
    char buffer[MANAGER_CLIENT_SOCKET_BUFFER_SIZE];

    // Create socket
    if ((client_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
        perror("socket error");
        exit(1);
    }

    struct sockaddr_un manager_client_socket_address;
    memset(&manager_client_socket_address, 0, sizeof(manager_client_socket_address));
    manager_client_socket_address.sun_family = AF_UNIX;
    strcpy(manager_client_socket_address.sun_path, MANAGER_CLIENT_SOCKET_PATH);

    // Connect to manager
    if (connect(client_fd, (struct sockaddr *)&manager_client_socket_address, sizeof(manager_client_socket_address)) == -1)
    {
        perror("connect error");
        close(client_fd);
        exit(1);
    }

    // Send initialization request
    const char *msg = "INITIALIZE_CLIENT";
    if (send(client_fd, msg, strlen(msg), 0) == -1)
    {
        perror("send error");
        close(client_fd);
        exit(1);
    }

    // Receive hashing function and node map
    int n = recv(client_fd, buffer, MANAGER_CLIENT_SOCKET_BUFFER_SIZE, 0);
    if (n < 0)
    {
        perror("recv error");
        close(client_fd);
        exit(1);
    }
    buffer[n] = '\0';

    // Parse response
    string response(buffer);
    // cout << "Received message from manager: " << response << endl;
    size_t num_replicas_pos = response.find("NUM_REPLICAS:");
    size_t hash_function_pos = response.find(";HASH_FUNCTION:");
    size_t nodes_pos = response.find(";NODES:");

    if (num_replicas_pos != string::npos && hash_function_pos != string::npos && nodes_pos != string::npos) {
        // Extract num_replicas
        string num_replicas_str = response.substr(num_replicas_pos + strlen("NUM_REPLICAS:"), hash_function_pos - (num_replicas_pos + strlen("NUM_REPLICAS:")));
        num_replicas = stoi(num_replicas_str);
        // cout << "Number of replicas: " << num_replicas << endl;

        // Extract hash function
        hash_function = response.substr(hash_function_pos + strlen(";HASH_FUNCTION:"), nodes_pos - (hash_function_pos + strlen(";HASH_FUNCTION:")));
        // cout << "Client initialized with hash function: " << hash_function << endl;

        // Extract nodes data
        string nodes_data = response.substr(nodes_pos + strlen(";NODES:"));
        size_t pos = 0;
        dead_node_ids.clear();
        node_address_map.clear(); // Clear existing data if any

        while ((pos = nodes_data.find(",")) != string::npos) {
            string node_pair = nodes_data.substr(0, pos);
            size_t colon_pos = node_pair.find(":");
            if (colon_pos != string::npos) {
                string node_id_str = node_pair.substr(0, colon_pos);
                string node_info = node_pair.substr(colon_pos + 1);
                int node_id = stoi(node_id_str);

                if (node_info == "DEAD") {
                    // Node is dead
                    dead_node_ids.push_back(node_id);
                } else {
                    // Node is alive
                    // Ensure the node_address_map is large enough
                    if (node_id >= node_address_map.size()) {
                        node_address_map.resize(node_id + 1);
                    }
                    node_address_map[node_id] = node_info;
                }
            }
            nodes_data.erase(0, pos + 1);
        }
        // Handle the last node pair
        if (!nodes_data.empty()) {
            size_t colon_pos = nodes_data.find(":");
            if (colon_pos != string::npos) {
                string node_id_str = nodes_data.substr(0, colon_pos);
                string node_info = nodes_data.substr(colon_pos + 1);
                int node_id = stoi(node_id_str);

                if (node_info == "DEAD") {
                    // Node is dead
                    dead_node_ids.push_back(node_id);
                } else {
                    // Node is alive
                    if (node_id >= node_address_map.size()) {
                        node_address_map.resize(node_id + 1);
                    }
                    node_address_map[node_id] = node_info;
                }
            }
        }

        // Output the node map
        // cout << "Node map:" << endl;
        // for (uint i = 0; i < node_address_map.size(); i++) {
        //     if (!node_address_map[i].empty()) {
        //         cout << i << ": " << node_address_map[i] << endl;
        //     }
        // }

        // Output the dead nodes
        // if (!dead_node_ids.empty()) {
        //     cout << "Dead nodes:" << endl;
        //     for (int dead_node_id : dead_node_ids) {
        //         cout << dead_node_id << endl;
        //     }
        // }
    } else {
        cout << "Error: Invalid response from manager\n";
    }

    close(client_fd);
}

val_t GTStoreClient::get(string key)
{
    // cout << "Inside GTStoreClient::get() for client: " << client_id << " key: " << key << "\n";
    val_t values; // val_t is typically a vector<string>

    // Hash the key to find the initial target node
    int node_id;
    if (hash_function == "ADD-ASCII")
    {
        int sum = 0;
        for (size_t i = 0; i < key.size(); i++)
        {
            sum += key[i];
        }
        node_id = sum % node_address_map.size();
    }
    else
    {
        cout << "Error: Invalid/Undefined hash function\n";
        return {"ERROR"};
    }

    // then scan through the dead nodes
    // and try to connect to the next node
    bool is_dead = false;
    int current_num_replicas = num_replicas;
    while(!is_dead && current_num_replicas > 0) {
        for(int dead_node_id : dead_node_ids) {
            if(dead_node_id == node_id) {
                node_id = (node_id + 1) % node_address_map.size();
                is_dead = true;
                current_num_replicas--;
                break;
            }
        }
        if(!is_dead) {
            break;
        }
    }

    if(current_num_replicas == 0) {
        cout << "Error: All replicas are dead\n";
        return {"ERROR"};
    }


    vector<int> dead_nodes;
    int attempts = 0;
    bool success = false;

    while (attempts < current_num_replicas)
    {
        string node_address = node_address_map[node_id];
        // cout << "Attempting to connect to node " << node_id << " at address " << node_address << endl;

        // Create socket
        int node_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (node_fd == -1)
        {
            perror("socket error");
            dead_nodes.push_back(node_id);
            node_id = (node_id + 1) % node_address_map.size();
            attempts++;
            continue;
        }

        struct sockaddr_un node_socket_address;
        memset(&node_socket_address, 0, sizeof(node_socket_address));
        node_socket_address.sun_family = AF_UNIX;
        strcpy(node_socket_address.sun_path, node_address.c_str());

        // Attempt to connect to the node
        if (connect(node_fd, (struct sockaddr *)&node_socket_address, sizeof(node_socket_address)) == -1)
        {
            perror(("connect error to node: " + node_address).c_str());
            close(node_fd);
            dead_nodes.push_back(node_id);
            node_id = (node_id + 1) % node_address_map.size();
            attempts++;
            continue;
        }

        // cout << "Connected to node " << node_id << " at address " << node_address << endl;

        // Send GET request
        string msg = "GET:" + key;
        // cout << "Sending message " << msg << " to node_fd " << node_fd << endl;
        if (send(node_fd, msg.c_str(), msg.length(), 0) == -1)
        {
            perror("send error");
            close(node_fd);
            dead_nodes.push_back(node_id);
            node_id = (node_id + 1) % node_address_map.size();
            attempts++;
            continue;
        }

        // Receive response from node
        char buffer[NODE_CLIENT_SOCKET_BUFFER_SIZE];
        int n = recv(node_fd, buffer, NODE_CLIENT_SOCKET_BUFFER_SIZE, 0);
        if (n <= 0)
        {
            perror("recv error");
            close(node_fd);
            dead_nodes.push_back(node_id);
            node_id = (node_id + 1) % node_address_map.size();
            attempts++;
            continue;
        }
        buffer[n] = '\0';
        // cout << "Received message from node: " << buffer << endl;
        string received_msg(buffer);

        // Close the node socket
        close(node_fd);

        // Handle the response from the node
        if (received_msg == "KEY_NOT_FOUND")
        {
            // cout << "Key not found on node\n";
            return {"ERROR"};
        }
        else
        {
            cout << "Received message[GET]: " << received_msg << endl;

            // Extract the value string from the message
            size_t first_comma_pos = received_msg.find(",");
            size_t second_comma_pos = received_msg.find(",", first_comma_pos + 1);
            if (first_comma_pos == string::npos || second_comma_pos == string::npos)
            {
                cout << "Invalid response format\n";
                return {"ERROR"};
            }

            string value_str = received_msg.substr(first_comma_pos + 2, second_comma_pos - first_comma_pos - 2);
            // cout << "Extracted value string: " << value_str << endl;

            // Split the value string into a vector of strings
            size_t pos = 0;
            while ((pos = value_str.find(" ")) != string::npos)
            {
                string val = value_str.substr(0, pos);
                if (!val.empty())
                {
                    values.push_back(val); // Add each value to the vector
                }
                value_str.erase(0, pos + 1);
            }

            // Handle the last value
            if (!value_str.empty())
            {
                values.push_back(value_str);
            }

            // Debug: Print the extracted values
            // cout << "Extracted values as vector: ";
            // for (const auto &val : values)
            // {
            //     cout << val << " ";
            // }
            // cout << endl;

            success = true;
            break;
        }
    }

    if (!success)
    {
        cout << "Failed to retrieve key from any replica nodes.\n";
        // Notify the manager about dead nodes
        if (!dead_nodes.empty())
        {
            notify_manager_of_dead_nodes(dead_nodes);
        }
        return {"ERROR"};
    }

    // If any dead nodes were found, notify the manager
    if (!dead_nodes.empty())
    {
        notify_manager_of_dead_nodes(dead_nodes);
    }

    return values; // Return the extracted values
}

bool GTStoreClient::put(string key, val_t value)
{
    string print_value = "";
    for (uint i = 0; i < value.size(); i++)
    {
        print_value += value[i] + " ";
    }
    // cout << "Inside GTStoreClient::put() for client: " << client_id << " key: " << key << " value: " << print_value << "\n";

    // Hash the key to find the initial target node
    int node_id;
    if (hash_function == "ADD-ASCII")
    {
        int sum = 0;
        for (uint i = 0; i < key.size(); i++)
        {
            sum += key[i];
        }
        node_id = sum % node_address_map.size();
    }
    else
    {
        cout << "Error: Invalid/Undefined hash function\n";
        return false;
    }

    bool is_dead = false;
    int current_num_replicas = num_replicas;
    while(!is_dead && current_num_replicas > 0) {
        for(int dead_node_id : dead_node_ids) {
            if(dead_node_id == node_id) {
                node_id = (node_id + 1) % node_address_map.size();
                is_dead = true;
                current_num_replicas--;
                break;
            }
        }
        if(!is_dead) {
            break;
        }
    }

    if(current_num_replicas == 0) {
        cout << "All replicas are dead. Cannot perform PUT operation.\n";
        return false;
    }


    vector<int> dead_nodes;
    int attempts = 0;
    bool success = false;

    while (attempts < current_num_replicas)
    {
        string node_address = node_address_map[node_id];
        // cout << "Attempting to connect to node " << node_id << " at address " << node_address << endl;

        // Create socket
        int node_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (node_fd == -1)
        {
            perror("socket error");
            dead_nodes.push_back(node_id);
            node_id = (node_id + 1) % node_address_map.size();
            attempts++;
            continue;
        }

        struct sockaddr_un node_socket_address;
        memset(&node_socket_address, 0, sizeof(node_socket_address));
        node_socket_address.sun_family = AF_UNIX;
        strcpy(node_socket_address.sun_path, node_address.c_str());

        // Attempt to connect to the node
        if (connect(node_fd, (struct sockaddr *)&node_socket_address, sizeof(node_socket_address)) == -1)
        {
            perror(("connect error to node: " + node_address).c_str());
            close(node_fd);
            dead_nodes.push_back(node_id);
            node_id = (node_id + 1) % node_address_map.size();
            attempts++;
            continue;
        }

        // cout << "Connected to node " << node_id << " at address " << node_address << endl;

        // Send key-value pair
        string msg = "PUT:" + key + " " + print_value;
        // cout << "Sending message " << msg << " to node_fd " << node_fd << endl;
        if (send(node_fd, msg.c_str(), msg.length(), 0) == -1)
        {
            perror("send error");
            close(node_fd);
            dead_nodes.push_back(node_id);
            node_id = (node_id + 1) % node_address_map.size();
            attempts++;
            continue;
        }

        // Receive acknowledgment from node
        char buffer[NODE_CLIENT_SOCKET_BUFFER_SIZE];
        int n = recv(node_fd, buffer, NODE_CLIENT_SOCKET_BUFFER_SIZE, 0);
        if (n <= 0)
        {
            perror("recv error");
            close(node_fd);
            dead_nodes.push_back(node_id);
            node_id = (node_id + 1) % node_address_map.size();
            attempts++;
            continue;
        }
        buffer[n] = '\0';
        cout << "Received message[PUT]: " << buffer << endl;

        // Close the node socket
        close(node_fd);

        success = true;
        break;
    }

    if (!success)
    {
        cout << "Failed to connect to any replica nodes for PUT operation.\n";
        // Notify the manager about dead nodes
        if (!dead_nodes.empty())
        {
            notify_manager_of_dead_nodes(dead_nodes);
        }
        return false;
    }

    // If any dead nodes were found, notify the manager
    if (!dead_nodes.empty())
    {
        notify_manager_of_dead_nodes(dead_nodes);
    }

    return true;
}

void GTStoreClient::notify_manager_of_dead_nodes(const vector<int> &dead_nodes)
{
    // cout << "Notifying manager about dead nodes.\n";

    int client_fd;
    char buffer[MANAGER_CLIENT_SOCKET_BUFFER_SIZE];

    // Create socket
    if ((client_fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
        perror("socket error");
        return;
    }

    struct sockaddr_un manager_client_socket_address;
    memset(&manager_client_socket_address, 0, sizeof(manager_client_socket_address));
    manager_client_socket_address.sun_family = AF_UNIX;
    strcpy(manager_client_socket_address.sun_path, MANAGER_CLIENT_SOCKET_PATH);

    // Connect to manager
    if (connect(client_fd, (struct sockaddr *)&manager_client_socket_address, sizeof(manager_client_socket_address)) == -1)
    {
        perror("connect error");
        close(client_fd);
        return;
    }

    // Send dead nodes information
    string msg = "DEAD_NODES:";
    for (size_t i = 0; i < dead_nodes.size(); ++i)
    {
        msg += to_string(dead_nodes[i]);
        if (i < dead_nodes.size() - 1)
        {
            msg += ",";
        }
    }
    // cout << "Sending message to manager: " << msg << endl;

    if (send(client_fd, msg.c_str(), msg.length(), 0) == -1)
    {
        perror("send error");
        close(client_fd);
        return;
    }

    // Optionally receive acknowledgment from manager
    int n = recv(client_fd, buffer, MANAGER_CLIENT_SOCKET_BUFFER_SIZE, 0);
    if (n < 0)
    {
        perror("recv error");
        close(client_fd);
        return;
    }
    buffer[n] = '\0';
    // cout << "Received message from manager: " << buffer << endl;

    close(client_fd);
}

void GTStoreClient::finalize()
{
    // cout << "Inside GTStoreClient::finalize() for client " << client_id << "\n";
    // No need to close node sockets since we connect and disconnect as needed
}
