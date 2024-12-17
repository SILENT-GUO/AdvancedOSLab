#include "gtstore.hpp"

void perform_put(int client_id, const string& key, const vector<string>& value) {
    // cout << "Client " << client_id << ": Performing PUT operation for key: " << key << "\n";

    GTStoreClient client;
    client.init(client_id);

    client.put(key, value);

    client.finalize();
}

void perform_get(int client_id, const string& key) {
    // cout << "Client " << client_id << ": Performing GET operation for key: " << key << "\n";

    GTStoreClient client;
    client.init(client_id);

    client.get(key);

    client.finalize();
}

int main(int argc, char** argv) {
    if (argc < 3) {
        cerr << "Usage: ./client --put <key> --val <value1,value2,...> or ./client --get <key>\n";
        return 1;
    }

    string operation = string(argv[1]);
    int client_id = getpid(); // Use process ID as client ID for simplicity

    if (operation == "--put") {
        if (argc < 5 || string(argv[3]) != "--val") {
            cerr << "Usage for PUT: ./client --put <key> --val <value1,value2,...>\n";
            return 1;
        }

        string key = string(argv[2]);
        string values_str = string(argv[4]);
        vector<string> value;

        // Split the values by commas
        size_t pos = 0;
        while ((pos = values_str.find(',')) != string::npos) {
            value.push_back(values_str.substr(0, pos));
            values_str.erase(0, pos + 1);
        }
        if (!values_str.empty()) {
            value.push_back(values_str);
        }

        perform_put(client_id, key, value);
    } else if (operation == "--get") {
        if (argc < 3) {
            cerr << "Usage for GET: ./client --get <key>\n";
            return 1;
        }

        string key = string(argv[2]);
        perform_get(client_id, key);
    } else {
        cerr << "Invalid operation. Use --put or --get.\n";
        return 1;
    }

    return 0;
}
