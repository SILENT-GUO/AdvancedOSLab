#ifndef SERVER_H
#define SERVER_H

#include <mqueue.h>
#include "common_api.h"


/* linked list for client storage */
typedef struct client_node {
    client_t client;
    struct client_node *next;
    struct client_node *prev;
} client_node_t;



/* Global variables */
const char *register_client_queue = REGISTER_CLIENT_QUEUE;
int is_exit_register_client_server = 0;
mqd_t register_client_queue_fd;
int num_shared_memory = 0;
int *max_per_shared_memory_size_ptr = NULL;
pthread_mutex_t global_mutex;
client_node_t *client_list_head_node = NULL;
pthread_t register_client_thread;



/* Function declarations */
void register_client_to_server(client_register_message_entry_t client_register_message_entry);
void *register_client_handler(void *arg);
void initialize_register_client_server(void);
void unregister_client_from_server(client_register_message_entry_t client_register_message_entry); // this is the handling function triggered by client
void unregister_client_directly(int pid); // this is the function called by server when exiting
void append_client_node_to_tail(client_node_t *node);
void start_worker_threads(client_t *client);
void *worker_thread_handler(void *arg);
void handle_message(client_message_info_t *message_info, client_t *client);
int compress_file(char *original_file_path, char *compressed_file_path);
client_node_t *find_current_client_node(int pid);
void remove_client_node(client_node_t *node);
void remove_worker_threads(client_t *client);
void exit_server();

#endif /* SERVER_H */
