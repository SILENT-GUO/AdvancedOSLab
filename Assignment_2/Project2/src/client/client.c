#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include "tinyfile/client.h"
#include <errno.h>

void open_message_queues(){
    int pid = getpid();
    char client_message_send_queue_name[100];
    char client_message_recv_queue_name[100];

    client.pid = pid;

    sprintf(client_message_send_queue_name, "%s%d", CLIENT_MESSAGE_SEND_QUEUE_PREFIX, pid);
    sprintf(client_message_recv_queue_name, "%s%d", CLIENT_MESSAGE_RECV_QUEUE_PREFIX, pid);

    strcpy(client.send_queue_name, client_message_send_queue_name);
    strcpy(client.recv_queue_name, client_message_recv_queue_name);

    struct mq_attr client_send_message_queue_attr, client_recv_message_queue_attr;
    client_send_message_queue_attr.mq_maxmsg = 10;
    client_recv_message_queue_attr.mq_maxmsg = 10;
    client_send_message_queue_attr.mq_msgsize = sizeof(client_message_info_t);
    client_recv_message_queue_attr.mq_msgsize = sizeof(unregister_client_message_t);
    client.send_q = mq_open(client.send_queue_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, &client_send_message_queue_attr);
    if(client.send_q == (mqd_t) -1){
        perror("ERROR: mq_open(client.send_q) failed in open_message_queues()\n");
        exit(EXIT_FAILURE);
    }
    // TODO: need another data structure for receiving messages
    client.recv_q = mq_open(client.recv_queue_name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR, &client_recv_message_queue_attr);
    if(client.recv_q == (mqd_t) -1){
        fprintf(stderr, "ERROR: mq_open(client.recv_q) failed in open_message_queues()\n");
        exit(EXIT_FAILURE);
    }

    printf("Client message queues opened\n");
}

void open_shared_memory(){
    int pid = getpid();
    char client_shared_memory_name[100];
    sprintf(client_shared_memory_name, "%s%d", CLIENT_SHARED_MEMORY_NAME_PREFIX, pid);
    strcpy(client.shm_name, client_shared_memory_name);
    printf("Client shared memory name is %s\n", client.shm_name);

    printf("Now waiting for the server to set the max_per_shared_memory_size\n");
    while(*max_per_shared_memory_size_ptr == -1){
        // wait until the server sets the max_per_shared_memory_size
        printf("Waiting for the server to set the max_per_shared_memory_size\n");
        sleep(1);
    }
    printf("max_per_shared_memory_size_ptr is set to %d\n", *max_per_shared_memory_size_ptr);

    // now we can set up the shared memory on client and read the shared memory stats for server

    int fd = shm_open(client.shm_name, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
    if(fd == -1){
        fprintf(stderr, "ERROR: shm_open(client.shm_name) failed in open_shared_memory()\n");
        exit(EXIT_FAILURE);
    }
    size_t num_shared_memory_entries = *max_per_shared_memory_size_ptr / sizeof(shared_memory_entry_t);

    client.shared_memory_size = sizeof(shared_memory_entry_t) * num_shared_memory_entries;
    printf("Client shared memory size is %ld and size of shared_memory_entry_t is %ld\n", client.shared_memory_size, sizeof(shared_memory_entry_t));
    if (ftruncate(fd, client.shared_memory_size) == -1) {
        perror("ftruncate failed");
        exit(EXIT_FAILURE);
    }


    client.shared_memory_address = mmap(NULL, client.shared_memory_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(client.shared_memory_address == MAP_FAILED){
        fprintf(stderr, "ERROR: mmap() failed in open_shared_memory()\n");
        exit(EXIT_FAILURE);
    }

    // copy the shared memory entries
    shared_memory_entry_t shared_memory_entry;
    shared_memory_entry.is_done = false;
    shared_memory_entry.is_used = false;
    for(int i = 0; i < num_shared_memory_entries; ++i){
        memcpy(client.shared_memory_address + i * sizeof(shared_memory_entry_t), &shared_memory_entry, sizeof(shared_memory_entry_t));
        // initialize the mutex lock in each shared entry object
        shared_memory_entry_t *entry = (shared_memory_entry_t *) (client.shared_memory_address + i * sizeof(shared_memory_entry_t));
        pthread_mutex_init(&entry->mutex, NULL);
    }
    close(fd);
}

void copy_client_info_to_register_entry(){
    
    client_register_message_entry_c.client_pid = client.pid;
    client_register_message_entry_c.command = REGISTER;
    strcpy(client_register_message_entry_c.send_queue_name, client.send_queue_name);
    strcpy(client_register_message_entry_c.recv_queue_name, client.recv_queue_name);
    strcpy(client_register_message_entry_c.shm_name, client.shm_name);
    printf("Now printing the client_register_message_entry_c\n");
    printf("client_pid: %d\n", client_register_message_entry_c.client_pid);
    printf("command: %d\n", client_register_message_entry_c.command);
    printf("send_queue_name: %s\n", client_register_message_entry_c.send_queue_name);
    printf("recv_queue_name: %s\n", client_register_message_entry_c.recv_queue_name);
}

void initialize_client(){
    register_client_queue_fd_c = mq_open(register_client_queue_c, O_WRONLY); // only send messages
    if(register_client_queue_fd_c == (mqd_t) -1){
        fprintf(stderr, "ERROR: mq_open(register_client_queue_c) failed in initialize_client()\n");
        exit(EXIT_FAILURE);
    }
    open_message_queues();
    open_shared_memory();
    copy_client_info_to_register_entry();

    // send the register message to the server
    if(mq_send(register_client_queue_fd_c, (char *) &client_register_message_entry_c, sizeof(client_register_message_entry_t), 0) == -1){
        fprintf(stderr, "ERROR: mq_send(register_client_queue_fd_c) failed in initialize_client()\n");
        exit(EXIT_FAILURE);
    }

    // init the mutex
    pthread_mutex_init(&global_mutex_c, NULL);

}

int find_and_set_shared_memory_entry_index(char *shared_memory_address, char* file_path){
    for(int i = 0; i < *max_per_shared_memory_size_ptr / sizeof(shared_memory_entry_t); ++i){
        shared_memory_entry_t *entry = (shared_memory_entry_t *) (shared_memory_address + i * sizeof(shared_memory_entry_t));
        pthread_mutex_lock(&entry->mutex);
        if(!entry->is_used){
            entry->is_used = 1;
            entry->is_done = 0;
            strcpy(entry->original_file_path, file_path);

            char base_path[256];
            const char *input_dir = "/input/";
            const char *output_dir = "/output/";
            char *input_pos = strstr(file_path, input_dir);
            
            if (input_pos == NULL) {
                fprintf(stderr, "Error: 'input' folder not found in the file path - %s\n", file_path);
                return -1;
            }
            
            size_t base_len = input_pos - file_path;
            strncpy(base_path, file_path, base_len);
            base_path[base_len] = '\0';
            const char *file_name = input_pos + strlen(input_dir); 
            char output_folder[256];
            strcpy(output_folder, base_path);
            strcat(output_folder, output_dir);

            struct stat st = {0};
            if (stat(output_folder, &st) == -1) {
                if (mkdir(output_folder, 0700) == -1) {
                    fprintf(stderr, "Error: Could not create output directory - %s\n", strerror(errno));
                    return -1;
                }
            }

            strcpy(entry->compressed_file_path, output_folder);
            strcat(entry->compressed_file_path, file_name);
            strcat(entry->compressed_file_path, "_compressed");

            pthread_mutex_unlock(&entry->mutex);
            return i;
        }

        pthread_mutex_unlock(&entry->mutex);
    }
    return -1;
}

void wait_response_from_server(int shared_memory_entry_index){
    shared_memory_entry_t *entry = (shared_memory_entry_t *) (client.shared_memory_address + shared_memory_entry_index * sizeof(shared_memory_entry_t));
    while(1){
        pthread_mutex_lock(&entry->mutex);
        // printf("Waiting for the server to finish the compression\n");
        if(entry->is_done){
            entry->is_used = 0;
            entry->is_done = 0;
            pthread_mutex_unlock(&entry->mutex);
            break;
        }
        pthread_mutex_unlock(&entry->mutex);

        //sleep for 0.1 second
        usleep(100);
    }
}

void communicate_file_sync(char *file_path){
    client_message_info_t client_message_info;
    client_message_info.request = COMPRESS;

    pthread_mutex_lock(&global_mutex_c);
    client_message_info.shared_memory_entry_index = find_and_set_shared_memory_entry_index(client.shared_memory_address, file_path);
    pthread_mutex_unlock(&global_mutex_c);
    if(client_message_info.shared_memory_entry_index == -1){
        fprintf(stderr, "ERROR: No shared memory entry available in communicate_file_sync()\n");
        exit(EXIT_FAILURE);
    }
    // then send the message to the server via send_q
    if(mq_send(client.send_q, (char *) &client_message_info, sizeof(client_message_info_t), 0) == -1){
        fprintf(stderr, "ERROR: mq_send(client.send_q) failed in communicate_file_sync()\n");
        exit(EXIT_FAILURE);
    }

    // sync operation: wait for the server to finish the compression
    wait_response_from_server(client_message_info.shared_memory_entry_index);
}

void communicate_file_async(char *file_path, int *shared_memory_entry_index){
    client_message_info_t client_message_info;
    client_message_info.request = COMPRESS;

    pthread_mutex_lock(&global_mutex_c);
    client_message_info.shared_memory_entry_index = find_and_set_shared_memory_entry_index(client.shared_memory_address, file_path);
    pthread_mutex_unlock(&global_mutex_c);
    if(client_message_info.shared_memory_entry_index == -1){
        // wait in a loop until a shared memory is available
        while(client_message_info.shared_memory_entry_index == -1 && file_path != NULL){
            pthread_mutex_lock(&global_mutex_c);
            client_message_info.shared_memory_entry_index = find_and_set_shared_memory_entry_index(client.shared_memory_address, file_path);
            pthread_mutex_unlock(&global_mutex_c);
        }
    }

    // then send the message to the server via send_q
    if(mq_send(client.send_q, (char *) &client_message_info, sizeof(client_message_info_t), 0) == -1){
        fprintf(stderr, "ERROR: mq_send(client.send_q) failed in communicate_file_async()\n");
        exit(EXIT_FAILURE);
    }

    *shared_memory_entry_index = client_message_info.shared_memory_entry_index;

}

void communicate_file_async_wait(int shared_memory_entry_index){
    wait_response_from_server(shared_memory_entry_index);
}

void exit_client(){
    printf("Exiting the client\n");
    client_register_message_entry_c.command = UNREGISTER;
    if(mq_send(register_client_queue_fd_c, (char *) &client_register_message_entry_c, sizeof(client_register_message_entry_t), 0) == -1){
        fprintf(stderr, "ERROR: mq_send(register_client_queue_fd_c) failed in exit_client()\n");
        exit(EXIT_FAILURE);
    }

    // waiting for the server to finish the unregister process
    printf("Waiting for the server to finish the unregister process\n");
    while(1){
        unregister_client_message_t unregister_client_message;
        usleep(100);
        if(mq_receive(client.recv_q, (char *) &unregister_client_message, sizeof(unregister_client_message_t), NULL) == -1){
            printf("No message received in exit_client()\n");
            continue;
        }
        if(unregister_client_message.status == UNREGISTERED){
            break;
        }
    }

    mq_close(register_client_queue_fd_c);
    mq_close(client.send_q);
    mq_close(client.recv_q);
    mq_unlink(client.send_queue_name);
    mq_unlink(client.recv_queue_name);

    munmap(client.shared_memory_address, client.shared_memory_size);
    shm_unlink(client.shm_name);
    pthread_mutex_destroy(&global_mutex_c);
}

void *pthread_async_wait(void *arg){
    int shared_memory_entry_index = *((int *) arg);
    wait_response_from_server(shared_memory_entry_index);
    return NULL;
}

int main(int argc, char **argv){
    static struct option long_options[] = {
        {"file", required_argument, 0, 'f'},
        {"files", required_argument, 0, 'l'},
        {"state", required_argument, 0, 's'},
        {0, 0, 0, 0} // signify the end of options list
    };

    char file_path[MAX_FILE_PATH_SIZE], file_list_path[MAX_FILE_PATH_SIZE];

    char call_method = ' '; // 'a' for async, 's' for sync

    if(0){
        shm_unlink("/shared_memory_size");
    }else{
        // !!!! must run server first to set the shared memory !!!!
        int shm_fd_c_shm_size_c = shm_open("/shared_memory_size", O_CREAT |  O_RDWR, S_IRUSR | S_IWUSR);
        if(shm_fd_c_shm_size_c == -1){
            perror("ERROR: shm_open(/shared_memory_size) failed in main()\n");
            exit(EXIT_FAILURE);
        }
        max_per_shared_memory_size_ptr = (int *) mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_c_shm_size_c, 0);
        if (max_per_shared_memory_size_ptr == MAP_FAILED) {
            perror("ERROR: mmap() failed in main()\n");
            exit(EXIT_FAILURE);
        }
    }

    int c, option_index = 0, is_file_list = 0;
    while((c = getopt_long(argc, argv, "f:l:s:", long_options, &option_index)) != -1){
        switch(c){
            case 'f': // file path
                printf("--file param detected with value %s\n", optarg);
                strcpy(file_path, optarg);
                break;

            case 'l': // file list path
                printf("--files param detected with value %s\n", optarg);
                is_file_list = 1;
                strcpy(file_list_path, optarg);
                break;

            case 's': // state
                printf("--state param detected with value %s\n", optarg);
                if(strcmp(optarg, "ASYNC") == 0){
                    call_method = 'a';
                }else if(strcmp(optarg, "SYNC") == 0){
                    call_method = 's';
                }else{
                    printf("Invalid state value\n");
                }
                break;
            default:
                printf("Invalid option\n");
                break;
        }
    }
    // first examine of call_method is 'a' or 's'
    if(call_method != 'a' && call_method != 's'){
        printf("Invalid state value\n");
        exit(EXIT_FAILURE);
    }

    initialize_client();

    int num_files = 0;
    struct timespec start, end;

    if(!is_file_list){
        // we first examine if the file exists
        FILE *file = fopen(file_path, "r");
        if (file == NULL) {
            perror("ERROR: File open failed");
            fprintf(stderr, "File %s does not exist\n", file_path);
            exit(EXIT_FAILURE);
        } else {
            fclose(file);
        }
        clock_gettime(CLOCK_MONOTONIC, &start);
        if(call_method == 'a'){
            int required_shared_memory_index;
            communicate_file_async(file_path, &required_shared_memory_index);
            communicate_file_async_wait(required_shared_memory_index);
        }else{ // sync operation
            communicate_file_sync(file_path);
        }
        clock_gettime(CLOCK_MONOTONIC, &end);
        num_files++;

    }else{
        // TODO: read the file list and send the files to the server
        printf("File list path is %s\n", file_list_path);
        FILE *file_list = fopen(file_list_path, "r");
        if (file_list == NULL) {
            perror("ERROR: File open failed");
            fprintf(stderr, "File %s does not exist\n", file_list_path);
            exit(EXIT_FAILURE);
        }
        // handle the file list line by line
        char line[100];
        char *file_path_list[100];
        clock_gettime(CLOCK_MONOTONIC, &start);
        while(fgets(line, 100, file_list) != NULL){
            // remove the newline character
            line[strcspn(line, "\n")] = 0;
            if(call_method == 'a'){
                // for async operation, we create a pthread to wait for the server to finish the compression
                // then after sending all requests, we wait for the pthread to finish by pthread_join
                // we first collect the number of files in this while loop
                
                file_path_list[num_files] = malloc(sizeof(char) * 100);
                strcpy(file_path_list[num_files], line);
            }else{
                communicate_file_sync(line);
            }
            num_files++;
        }
        clock_gettime(CLOCK_MONOTONIC, &end);
        fclose(file_list);
        // now we do async operation
        if(call_method == 'a'){
            int shared_memory_entry_index_list[100];
            pthread_t threads[num_files];
            clock_gettime(CLOCK_MONOTONIC, &start);
            for(int i = 0; i < num_files; ++i){
                communicate_file_async(file_path_list[i], &shared_memory_entry_index_list[i]);
                pthread_create(&threads[i], NULL, pthread_async_wait, &shared_memory_entry_index_list[i]);
            }

            for(int i = 0; i < num_files; ++i){
                pthread_join(threads[i], NULL);
            }
            clock_gettime(CLOCK_MONOTONIC, &end);
            
        }
    }

    exit_client();

    munmap(max_per_shared_memory_size_ptr, sizeof(int));

    long seconds = end.tv_sec - start.tv_sec;
    long ns = end.tv_nsec - start.tv_nsec;
    if(end.tv_nsec < start.tv_nsec){
        --seconds;
        ns += 1000000000;
    }

    FILE *fp = fopen("client_log.csv", "a");
    if (fp == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }
    fprintf(fp, "%s,%d,%s,%ld,%ld\n", call_method == 'a' ? "ASYNC" : "SYNC", num_files, is_file_list ? file_list_path : file_path, seconds, ns);
    fclose(fp);
    return 0;
}