#ifndef COMMON_API_H
#define COMMON_API_H

#include <mqueue.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/mman.h>

#define NUM_WORKER_THREADS_PER_CLIENT 10
#define REGISTER_CLIENT_QUEUE "/register_client_queue"

enum client_unregister {
    UNREGISTERED
};

typedef struct unregister_client_message{
    enum client_unregister status;
} unregister_client_message_t;

enum client_register_command {
    REGISTER,
    UNREGISTER
};

enum client_service_request {
    COMPRESS
};

/* Struct for client message entries */
typedef struct client_register_message_entry {
    int client_pid;
    enum client_register_command command;
    char send_queue_name[100];
    char recv_queue_name[100];
    char shm_name[100];
} client_register_message_entry_t;

/* Struct for client */
typedef struct client {
    pid_t pid;
    mqd_t send_q;
    mqd_t recv_q;
    char send_queue_name[100];
    char recv_queue_name[100];
    char *shared_memory_address;
    size_t shared_memory_size;
    char shm_name[100];
    int is_worker_thread_active;
    pthread_t workers[NUM_WORKER_THREADS_PER_CLIENT]; // worker threads
} client_t;

typedef struct client_message_info {
    int shared_memory_entry_index;
    enum client_service_request request;
} client_message_info_t;



typedef struct shared_memory_entry {
    char original_file_path[100];
    char compressed_file_path[100];
    pthread_mutex_t mutex;
    int is_used;
    int is_done;
} shared_memory_entry_t;

#endif