#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "snappy-c/snappy.h"
#include <string.h>
#include "tinyfile/server.h"



void append_client_node_to_tail(client_node_t *node){
    if(client_list_head_node == NULL){
        client_list_head_node = node;
        client_list_head_node->next = NULL;
        client_list_head_node->prev = NULL;
    }else{
        client_node_t *tail = client_list_head_node;
        while(tail->next != NULL){
            tail = tail->next;
        }
        tail->next = node;
        node->prev = tail;
        node->next = NULL;
    }
}

void open_shared_memory(client_register_message_entry_t client_register_message_entry, client_t *client){
    int fd = shm_open(client_register_message_entry.shm_name, O_CREAT |  O_RDWR, S_IRUSR | S_IWUSR);
    if(fd == -1){
        fprintf(stderr, "ERROR: shm_open(client.shm_name) failed in open_shared_memory()\n");
        exit(EXIT_FAILURE);
    }

    memcpy(client->shm_name, client_register_message_entry.shm_name, 100);
    
    struct stat shared_memory_stats;
    fstat(fd, &shared_memory_stats);
    client->shared_memory_size = shared_memory_stats.st_size;
    printf("Shared memory size is %ld\n", client->shared_memory_size);
    client->shared_memory_address = mmap(NULL, client->shared_memory_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    printf("Shared memory address is %p\n", client->shared_memory_address);
    if(client->shared_memory_address == MAP_FAILED){
        fprintf(stderr, "ERROR: mmap() failed in open_shared_memory()\n");
        exit(EXIT_FAILURE);
    }
    close(fd);
    
}

int compress_file(char *input_file, char *output_file) {
    FILE *fp_in = fopen(input_file, "rb");
    if (!fp_in) {
        fprintf(stderr, "Failed to open input file %s\n", input_file);
        return -1;
    }
    if(fseek(fp_in, 0, SEEK_END) != 0){
        fprintf(stderr, "ERROR: fseek() failed\n");
        exit(EXIT_FAILURE);
    }
    long buffer_size = ftell(fp_in);
    if(buffer_size == -1){
        fprintf(stderr, "ERROR: ftell() failed\n");
        exit(EXIT_FAILURE);
    }
    printf("File size is %ld\n", buffer_size);

    if(fseek(fp_in, 0, SEEK_SET) != 0){
        fprintf(stderr, "ERROR: fseek() failed\n");
        exit(EXIT_FAILURE);
    }

    char *source_file_buffer = malloc(sizeof(char) * (buffer_size + 1));
    if(!source_file_buffer){
        fprintf(stderr, "ERROR: malloc() failed\n");
        exit(EXIT_FAILURE);
    }
    size_t source_file_buffer_size = fread(source_file_buffer, sizeof(char), buffer_size, fp_in);

    if(ferror(fp_in) != 0){
        fprintf(stderr, "ERROR: fread() failed\n");
        exit(EXIT_FAILURE);
    }
    fclose(fp_in);

    printf("File size is %ld\n", source_file_buffer_size);

    // compress the file
    size_t compressed_length = snappy_max_compressed_length(source_file_buffer_size);
    printf("Compressed length is %ld\n", compressed_length);
    char *compressed_file_buffer = malloc(sizeof(char) * compressed_length);
    struct snappy_env env;
    snappy_init_env(&env);
    snappy_compress(&env, source_file_buffer, source_file_buffer_size, compressed_file_buffer, &compressed_length);
    snappy_free_env(&env);
    printf("snappy_compress() done\n");

    // write the compressed file to the output file
    FILE *fp_out = fopen(output_file, "wb");
    if(!fp_out){
        fprintf(stderr, "Failed to open output file %s\n", output_file);
        return -1;
    }
    fprintf(fp_out, "%s", compressed_file_buffer);
    fclose(fp_out);

    free(source_file_buffer);
    free(compressed_file_buffer);

    printf("File compression finished\n");

    return 0;

}

void handle_message(client_message_info_t *message_info, client_t *client){
    // first find the shared entry in the shared memory
    shared_memory_entry_t *shared_memory_entry = (shared_memory_entry_t *) (client->shared_memory_address + message_info->shared_memory_entry_index * sizeof(shared_memory_entry_t));
    char *original_file_path = shared_memory_entry->original_file_path;
    char *compressed_file_path = shared_memory_entry->compressed_file_path;
    if(message_info->request == COMPRESS){
        printf("Compress request received, start compressing the file\n");
        if(compress_file(original_file_path, compressed_file_path) == -1){
            fprintf(stderr, "ERROR: compress_file() failed in handle_message()\n");
        }
        // signal the finish
        pthread_mutex_lock(&shared_memory_entry->mutex);
        shared_memory_entry->is_done = true;
        pthread_mutex_unlock(&shared_memory_entry->mutex);
        printf("File compressed\n");
    }else{
        fprintf(stderr, "ERROR: Unknown request received in handle_message()\n");
    }
}

void *worker_thread_handler(void *arg) {
    client_t *client = (client_t *)arg;
    char *message_buffer = malloc(sizeof(client_message_info_t));
    client_message_info_t *message_info;
    printf("Worker thread goes into while loop\n");
    // keep trying receiving messages from the client queue
    while(client->is_worker_thread_active){
        if(mq_receive(client->send_q, message_buffer, sizeof(client_message_info_t), NULL) == -1){
            // no message received
            continue;
        }
        printf("Worker thread received a message with content\n");
        message_info = (client_message_info_t *) message_buffer;
        printf("Worker thread received a message, the request is %d and the shared memory index is %d\n", message_info->request, message_info->shared_memory_entry_index);
        printf("Original file path is %s\n", ((shared_memory_entry_t *) (client->shared_memory_address + message_info->shared_memory_entry_index * sizeof(shared_memory_entry_t)))->original_file_path);
        printf("Compressed file path is %s\n", ((shared_memory_entry_t *) (client->shared_memory_address + message_info->shared_memory_entry_index * sizeof(shared_memory_entry_t)))->compressed_file_path);
        // handle the message
        handle_message(message_info, client);
    }

    return NULL;
    
}

void start_worker_threads(client_t *client){
    client->is_worker_thread_active = 1;
    for(int i = 0; i < NUM_WORKER_THREADS_PER_CLIENT; ++i){
        if(pthread_create(&client->workers[i], NULL, worker_thread_handler, client) != 0){
            fprintf(stderr, "ERROR: pthread_create() failed in start_worker_threads()\n");
            exit(EXIT_FAILURE);
        }
    }
}

void register_client_to_server(client_register_message_entry_t client_register_message_entry){
    client_t client;
    client.pid = client_register_message_entry.client_pid;

    client.send_q = mq_open(client_register_message_entry.send_queue_name, O_RDWR);
    if(client.send_q == (mqd_t) -1){
        fprintf(stderr, "ERROR: mq_open(client.send_q) failed in register_client_to_server()\n");
        exit(EXIT_FAILURE);
    }

    client.recv_q = mq_open(client_register_message_entry.recv_queue_name, O_RDWR);
    if(client.recv_q == (mqd_t) -1){
        fprintf(stderr, "ERROR: mq_open(client.recv_q) failed in register_client_to_server()\n");
        exit(EXIT_FAILURE);
    }

    // create shared memory
    open_shared_memory(client_register_message_entry, &client);
    
    // start worker threads
    start_worker_threads(&client);

    client_node_t *new_client_node = malloc(sizeof(client_node_t));
    new_client_node->client = client;
    append_client_node_to_tail(new_client_node);


    printf("Client with pid %d registered\n", client_register_message_entry.client_pid);

}

client_node_t *find_current_client_node(int client_pid){
    client_node_t *current = client_list_head_node;
    while(current != NULL){
        if(current->client.pid == client_pid){
            return current;
        }
        current = current->next;
    }
    return NULL;
}

void remove_client_node(client_node_t *node){
    if(node->prev == NULL){
        client_list_head_node = node->next;
    }else{
        node->prev->next = node->next;
    }
    if(node->next != NULL){
        node->next->prev = node->prev;
    }
    
}

void remove_worker_threads(client_t *client){
    
    printf("Worker threads are being cancelled\n");
    for(int i = 0; i < NUM_WORKER_THREADS_PER_CLIENT; ++i){
        if (client->workers[i] != 0) {  // Ensure the thread was initialized
            // Cancel the thread
            if(pthread_cancel(client->workers[i]) != 0){
                perror("ERROR: pthread_cancel() failed in remove_worker_threads()\n");
            } else {
                // Optionally, join the thread to ensure it has terminated properly
                if (pthread_join(client->workers[i], NULL) != 0) {
                    perror("ERROR: pthread_join() failed after cancel in remove_worker_threads()\n");
                }
            }
        }
    }
}

void unregister_client_from_server(client_register_message_entry_t client_register_message_entry) {
    client_node_t *node = find_current_client_node(client_register_message_entry.client_pid);
    if(node == NULL){
        fprintf(stderr, "ERROR: Client with pid %d not found in unregister_client_from_server()\n", client_register_message_entry.client_pid);
        return;
    }
    printf("Unregistering client with pid %d\n", client_register_message_entry.client_pid);
    node->client.is_worker_thread_active = 0;

    //send the unregister message to the client via recv_q
    unregister_client_message_t unregister_client_message;
    unregister_client_message.status = UNREGISTERED;
    if(mq_send(node->client.recv_q, (char *) &unregister_client_message, sizeof(unregister_client_message_t), 0) == -1){
        perror("ERROR: mq_send(node->client.recv_q) failed in unregister_client_from_server()\n");
        exit(EXIT_FAILURE);
    }

    remove_client_node(node);
    mq_close(node->client.send_q);
    mq_close(node->client.recv_q);
    munmap(node->client.shared_memory_address, node->client.shared_memory_size);
    printf("Shared memory unmapped\n");

    pthread_mutex_lock(&global_mutex);
    num_shared_memory++;
    pthread_mutex_unlock(&global_mutex);
    printf("Number of shared memories left after removing the client: %d\n", num_shared_memory);
    free(node);
    printf("Client with pid %d unregistered\n", client_register_message_entry.client_pid);

}

void unregister_client_directly(int pid){
    client_register_message_entry_t client_register_message_entry;
    client_register_message_entry.client_pid = pid;
    unregister_client_from_server(client_register_message_entry);
}


void *register_client_handler(void *arg){
    client_register_message_entry_t client_register_message_entry;
    char *recv_buf = malloc(sizeof(client_register_message_entry_t));

    while(!is_exit_register_client_server){
        fflush(stdout);
        if(mq_receive(register_client_queue_fd, recv_buf, sizeof(client_register_message_entry_t), NULL) == -1){
            // no message received
            continue;
        }
        client_register_message_entry = *(client_register_message_entry_t *) recv_buf;

        if(client_register_message_entry.command == REGISTER){
            if(num_shared_memory > 0){
                register_client_to_server(client_register_message_entry);
                
                pthread_mutex_lock(&global_mutex);
                num_shared_memory--;
                pthread_mutex_unlock(&global_mutex);
                printf("Number of shared memories left: %d\n", num_shared_memory);
            }else{
                fprintf(stderr, "ERROR: No shared memory available in register_client_handler()\n");
                // wait in a loop until a shared memory is available
                while(num_shared_memory == 0){
                    usleep(100);
                }
            }
        }else if (client_register_message_entry.command == UNREGISTER){
            printf("Unregister command received\n");
            unregister_client_from_server(client_register_message_entry);
        }else{
            fprintf(stderr, "ERROR: Unknown command received in register_client_handler()\n");
        }

        //as this loop is running consistently, a usleep is added to reduce the CPU usage
        usleep(100);

    }

    return NULL;
}


void initialize_register_client_server(){
    struct mq_attr register_queue_attr;
    register_queue_attr.mq_maxmsg = 10;
    register_queue_attr.mq_msgsize = sizeof(client_register_message_entry_t);

    register_client_queue_fd = mq_open(register_client_queue, O_CREAT | O_RDWR | O_NONBLOCK , S_IRUSR | S_IWUSR, &register_queue_attr);

    if(register_client_queue_fd == (mqd_t) -1){
        fprintf(stderr, "ERROR: mq_open(register_client_queue) failed in initialize_register_client_server()\n");
        exit(EXIT_FAILURE);
    }

    // initialize a pthread to listen to the register_client_queue_fd
    
    if(pthread_create(&register_client_thread, NULL, register_client_handler, NULL)){
        fprintf(stderr, "ERROR: pthread_create(&register_client_thread) failed in initialize_register_client_server()\n");
        exit(EXIT_FAILURE);
    }else{
        printf("Register client thread started\n");
    }
}

void exit_server(){
    client_node_t *current = client_list_head_node;
    while(current != NULL){
        mq_close(current->client.send_q);
        mq_close(current->client.recv_q);
        munmap(current->client.shared_memory_address, current->client.shared_memory_size);
        unregister_client_directly(current->client.pid);
        current = current->next;
    }

    mq_close(register_client_queue_fd);
    mq_unlink(register_client_queue);
    pthread_mutex_destroy(&global_mutex);
    printf("Now joining the register client thread\n");
    pthread_join(register_client_thread, NULL);
}

int main(int argc, char **argv){
    printf("Main function of server.c\n");
    is_exit_register_client_server = 0;
    //parse arguments
    int n_sms = 0;
    int sms_size = 0;

    pthread_mutex_init(&global_mutex, NULL);

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--n_sms") == 0 && i + 1 < argc) {
            n_sms = atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "--sms_size") == 0 && i + 1 < argc) {
            sms_size = atoi(argv[i + 1]);
            i++;
        }
    }
    printf("Number of shared memories: %d\n", n_sms);
    printf("Size of shared memory: %d\n", sms_size);
    num_shared_memory = n_sms;
    // unlink in case the shared memory size is already created and exists
    shm_unlink("/shared_memory_size");
    int shm_fd_c_shm_size = shm_open("/shared_memory_size", O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
    if(shm_fd_c_shm_size == -1){
        perror("ERROR: shm_open(/shared_memory_size) failed in main()\n");
        exit(EXIT_FAILURE);
    }
    
    if(ftruncate(shm_fd_c_shm_size, sizeof(int)) == -1){
        fprintf(stderr, "ERROR: ftruncate(shm_fd_c_shm_size) failed in main()\n");
        exit(EXIT_FAILURE);
    }
    printf("Now setting the shared memory size using shared memory technique\n");
    max_per_shared_memory_size_ptr = (int *) mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_c_shm_size, 0);
    *max_per_shared_memory_size_ptr = sms_size; // set the shared memory size into the shared memory
    printf("Shared memory size set, now initializing the register client server\n");
    // initialize the register client server
    initialize_register_client_server();

    printf("Server initialized\n");
    printf("-----------------\n");
    printf("If you want to exit the server, press 'q' and then 'Enter'\n");

    char input[5];
    while(1){
        if (fgets(input, 2, stdin) == NULL) {
            fprintf(stderr, "Error reading input\n");
        }

        if(input[0] == 'q'){
            is_exit_register_client_server = 1;
            break;
        }
    }

    printf("Exiting server\n");
    munmap(max_per_shared_memory_size_ptr, sizeof(int));
    shm_unlink("/shared_memory_size");

    exit_server();
    return 0;

}