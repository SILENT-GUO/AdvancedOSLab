#ifndef GTFS
#define GTFS

#include <string>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h> 
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <fcntl.h>  
#include <unordered_map>
#include <vector>
#include <mutex>

using namespace std;

#define PASS "\033[32;1m PASS \033[0m\n"
#define FAIL "\033[31;1m FAIL \033[0m\n"

// GTFileSystem basic data structures 

#define MAX_FILENAME_LEN 255
#define MAX_NUM_FILES_PER_DIR 1024

extern int do_verbose;

typedef struct gtfs {
    string dirname;
    // TODO: Add any additional fields if necessary
} gtfs_t;

typedef struct file {
    string filename;
    int file_length;
    // TODO: Add any additional fields if necessary
    int is_open = 0;
} file_t;

typedef struct write {
    string filename;
    int offset;
    int length;
    char *data;
    // TODO: Add any additional fields if necessary
    string full_file_path;
    string full_log_path;
} write_t;

// GTFileSystem basic API calls

gtfs_t* gtfs_init(string directory, int verbose_flag);
int gtfs_clean(gtfs_t *gtfs);

file_t* gtfs_open_file(gtfs_t* gtfs, string filename, int file_length);
int gtfs_close_file(gtfs_t* gtfs, file_t* fl);
int gtfs_remove_file(gtfs_t* gtfs, file_t* fl);

char* gtfs_read_file(gtfs_t* gtfs, file_t* fl, int offset, int length);
write_t* gtfs_write_file(gtfs_t* gtfs, file_t* fl, int offset, int length, const char* data);
int gtfs_sync_write_file(write_t* write_id);
int gtfs_abort_write_file(write_t* write_id);



// BONUS: Implement below API calls to get bonus credits

int gtfs_clean_n_bytes(gtfs_t *gtfs, int bytes);
int gtfs_sync_write_file_n_bytes(write_t* write_id, int bytes);

// TODO: Add here any additional data structures or API calls
int sync_log_to_file(gtfs_t *gtfs, string file_path, string logfile_path);
bool lockFile(int fd);
bool unlockFile(int fd);



#endif
