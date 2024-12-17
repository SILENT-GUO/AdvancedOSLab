#ifndef CLIENT_H
#define CLIENT_H

#include "common_api.h"
#include <mqueue.h>

#define MAX_FILE_PATH_SIZE 200
#define CLIENT_MESSAGE_SEND_QUEUE_PREFIX "/client_message_send_queue_"
#define CLIENT_MESSAGE_RECV_QUEUE_PREFIX "/client_message_recv_queue_"
#define CLIENT_SHARED_MEMORY_NAME_PREFIX "/client_shared_memory_"

// global variables
const char *register_client_queue_c = REGISTER_CLIENT_QUEUE;
mqd_t register_client_queue_fd_c;
client_register_message_entry_t client_register_message_entry_c;
pthread_mutex_t global_mutex_c;
int *max_per_shared_memory_size_ptr;


// function declarations
void initialize_client();
void open_message_queues();
void open_shared_memory();
void copy_client_info_to_register_entry();
void communicate_file_sync(char *file_path);
void communicate_file_async(char *file_path, int *shared_memory_entry_index);
void communicate_file_async_wait(int shared_memory_entry_index);
int find_and_set_shared_memory_entry_index(char *shared_memory_address, char* file_path);
void wait_response_from_server(int shared_memory_entry_index);
void exit_client();
void *pthread_async_wait(void *);
client_t client;

#endif