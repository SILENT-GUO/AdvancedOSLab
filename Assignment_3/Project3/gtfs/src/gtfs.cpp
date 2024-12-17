#include "gtfs.hpp"

#define VERBOSE_PRINT(verbose, str...) do { \
    if (verbose) cout << "VERBOSE: "<< __FILE__ << ":" << __LINE__ << " " << __func__ << "(): " << str; \
} while(0)

int do_verbose;
unordered_map<string, gtfs_t *> directory_gtfs_mapping;
unordered_map<string, vector<write_t *>> file_log_updates_mapping;
unordered_map<string, file_t *> filename_file_t_mapping;
unordered_map<string, vector<string>> directory_files_mapping;

mutex directory_gtfs_mapping_mutex;
mutex file_log_updates_mapping_mutex;
mutex filename_file_t_mapping_mutex;
mutex directory_files_mapping_mutex;




bool lockFile(int fd) {
    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;

    if (fcntl(fd, F_SETLKW, &lock) == -1) {
        perror("Error locking file");
        return false;
    }
    return true;
}

bool unlockFile(int fd) {
    struct flock lock;
    lock.l_type = F_UNLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;

    if (fcntl(fd, F_SETLK, &lock) == -1) {
        perror("Error unlocking file");
        return false;
    }
    return true;
}

gtfs_t* gtfs_init(string directory, int verbose_flag) {
    do_verbose = verbose_flag;
    gtfs_t *gtfs = NULL;
    VERBOSE_PRINT(do_verbose, "Initializing GTFileSystem inside directory " << directory << "\n");
    //TODO: Add any additional initializations and checks, and complete the functionality
    gtfs = new gtfs_t();
    gtfs->dirname = directory;
    //check if directory exists
    struct stat sb;
    if (stat(directory.c_str(), &sb) == 0 && S_ISDIR(sb.st_mode)) {
        VERBOSE_PRINT(do_verbose, "Directory exists\n");
    } else {
        VERBOSE_PRINT(do_verbose, "Directory does not exist\n");
        return NULL;
    }

    //check if the gtfs for this directory already exists
    if (directory_gtfs_mapping.find(directory) != directory_gtfs_mapping.end()) {
        VERBOSE_PRINT(do_verbose, "GTFileSystem already exists\n");
        VERBOSE_PRINT(do_verbose, "Success\n"); //On success returns non NULL.
        return directory_gtfs_mapping[directory];
    } else{
        VERBOSE_PRINT(do_verbose, "GTFileSystem does not exist: directory " << directory << "\n");
        directory_files_mapping_mutex.lock();
        directory_gtfs_mapping[directory] = gtfs;
        directory_files_mapping_mutex.unlock();
    }
    VERBOSE_PRINT(do_verbose, "Success\n"); //On success returns non NULL.
    return gtfs;
}

int sync_log_to_file(gtfs_t *gtfs, string file_path, string logfile_path) {
    int ret = -1;
    VERBOSE_PRINT(do_verbose, "Syncing log to file\n");
    // check if both file and log file exists, if not return -1
    struct stat sb;
    if (stat(file_path.c_str(), &sb) == 0 && S_ISREG(sb.st_mode)) {
        VERBOSE_PRINT(do_verbose, "File exists\n");
    } else {
        VERBOSE_PRINT(do_verbose, "File does not exist\n");
        return ret;
    }
    struct stat sb_log;
    if (stat(logfile_path.c_str(), &sb_log) == 0 && S_ISREG(sb_log.st_mode)) {
        VERBOSE_PRINT(do_verbose, "Log file exists\n");
    } else {
        VERBOSE_PRINT(do_verbose, "Log file does not exist\n");
        return ret;
    }
    string line;
    ifstream log_file(logfile_path.c_str());
    if (log_file.is_open()) {
        VERBOSE_PRINT(do_verbose, "Log file opened\n");
        while (getline(log_file, line)) {
            //skip empty lines
            if (line.empty()) {
                VERBOSE_PRINT(do_verbose, "Empty line detected\n");
                continue;
            }
            VERBOSE_PRINT(do_verbose, "Reading log line: " << line << "\n");
            vector<string> log_data;
            stringstream ss(line);
            string token;
            while(getline(ss, token, '|')) {
                log_data.push_back(token);
            }
            string filename = log_data[0];
            int offset = stoi(log_data[1]);
            int length = stoi(log_data[2]);
            string data = log_data[3];
            data += "\n";
            string full_file_path = gtfs->dirname + "/" + filename;
            fstream fs(full_file_path.c_str());
            fs >> noskipws;
            fs.seekp(offset);
            fs.write(data.c_str(), length);
            fs.close();
        }
        log_file.close();
        VERBOSE_PRINT(do_verbose, "Log synced to file\n");
        return 0;
    } else {
        VERBOSE_PRINT(do_verbose, "Error opening log file\n");
        return ret;
    }
    
    return ret;
}

int gtfs_clean(gtfs_t *gtfs) {
    int ret = -1;
    if (gtfs) {
        VERBOSE_PRINT(do_verbose, "Cleaning up GTFileSystem inside directory " << gtfs->dirname << "\n");
    } else {
        VERBOSE_PRINT(do_verbose, "GTFileSystem does not exist\n");
        return ret;
    }
    //TODO: Add any additional initializations and checks, and complete the functionality

    struct dirent *entry;
    DIR *dp = opendir(gtfs->dirname.c_str());
    if(dp == NULL) {
        VERBOSE_PRINT(do_verbose, "Error opening directory\n");
        return ret;
    }

    while((entry = readdir(dp)) != NULL) {
        string filename = entry->d_name;
        // check if the file is a log, we don't create log for logs
        if (filename.find(".txt") == string::npos) {
            continue;
        }
        if(filename.find(".log") == string::npos) {
            VERBOSE_PRINT(do_verbose, "apply the log content to the file: " << filename << "\n");
        } else {
            continue;
        }
        string file_path = gtfs->dirname + "/" + filename;
        string logfile_path = gtfs->dirname + "/" + filename + ".log";

        // TODO: sync the log file with the file
        int res = sync_log_to_file(gtfs, file_path, logfile_path);
        if (res == -1) {
            VERBOSE_PRINT(do_verbose, "Error syncing log to file\n");
            return ret;
        }
        // clear the contents of the log file
        VERBOSE_PRINT(do_verbose, "Clearing log file\n");
        ofstream ofs;
        ofs.open(logfile_path, ofstream::trunc);
        ofs.close();
        
    }
    // clean contents in mapping of file and log updates
    file_log_updates_mapping_mutex.lock();
    file_log_updates_mapping.clear();
    file_log_updates_mapping_mutex.unlock();
    int is_close = closedir(dp);
    if(is_close == -1) {
        VERBOSE_PRINT(do_verbose, "Error closing directory\n");
    }else {
        VERBOSE_PRINT(do_verbose, "Directory closed\n");
        VERBOSE_PRINT(do_verbose, "Success\n"); //On success returns 0.
        return 0;
    }

    return ret;
}

file_t* gtfs_open_file(gtfs_t* gtfs, string filename, int file_length) {
    file_t *fl = new file_t();
    if (gtfs) {
        VERBOSE_PRINT(do_verbose, "Opening file " << filename << " inside directory " << gtfs->dirname << "\n");
    } else {
        VERBOSE_PRINT(do_verbose, "GTFileSystem does not exist\n");
        return NULL;
    }
    //TODO: Add any additional initializations and checks, and complete the functionality
    if(filename.length() > MAX_FILENAME_LEN) {
        VERBOSE_PRINT(do_verbose, "Filename too long\n");
        return NULL;
    }
    if(directory_files_mapping[gtfs->dirname].size() > MAX_NUM_FILES_PER_DIR) {
        VERBOSE_PRINT(do_verbose, "Too many files in directory\n");
        return NULL;
    }

    string full_file_path = gtfs->dirname + "/" + filename;
    int fd = open(full_file_path.c_str(), O_RDWR | O_CREAT, 0666);
    if (fd == -1) {
        perror("Error opening file");
        return NULL;
    }

    // Lock the file to prevent concurrent access
    if (!lockFile(fd)) {
        close(fd);
        return NULL;
    }



    if(filename_file_t_mapping.find(filename) != filename_file_t_mapping.end()) {
        VERBOSE_PRINT(do_verbose, "File already exists\n");
        if(filename_file_t_mapping[filename]->file_length > file_length) {
            VERBOSE_PRINT(do_verbose, "File length is smaller than existing file, operation not allowed\n");
            unlockFile(fd);
            close(fd);
            return nullptr;
        } else {
            VERBOSE_PRINT(do_verbose, "File length is greater/equal than existing file, operation allowed\n");
            fl = filename_file_t_mapping[filename];
            fl->file_length = file_length;
            fl->is_open = 1;
        }
    }else{
        // The rest condition solves the file is not existing
        fl->filename = filename;
        fl->file_length = file_length;
        fl->is_open = 1;
        string full_file_path = gtfs->dirname + "/" + filename;
        struct stat sb;
        if (stat(full_file_path.c_str(), &sb) == 0 && S_ISREG(sb.st_mode)) {
            VERBOSE_PRINT(do_verbose, "File exists but not recorded in mapping\n");
        } else {
            VERBOSE_PRINT(do_verbose, "File does not exist\n");
            // create the file
            ofstream ofs;
            ofs.open(full_file_path);
            ofs.close();
        }
        // insert the file into the mappings
        filename_file_t_mapping_mutex.lock();
        filename_file_t_mapping[filename] = fl;
        filename_file_t_mapping_mutex.unlock();
        directory_files_mapping_mutex.lock();
        directory_files_mapping[gtfs->dirname].push_back(filename);
        directory_files_mapping_mutex.unlock();
        
        string full_log_path = gtfs->dirname + "/" + filename + ".log";
        struct stat sb_log;
        if (stat(full_log_path.c_str(), &sb_log) == 0 && S_ISREG(sb_log.st_mode)) {
            VERBOSE_PRINT(do_verbose, "Log file exists\n");
            int res = sync_log_to_file(gtfs, filename, full_log_path);
            if (res == -1) {
                VERBOSE_PRINT(do_verbose, "Error syncing log to file\n");
                unlockFile(fd);
                close(fd);
                return NULL;
            }
        }else{
            VERBOSE_PRINT(do_verbose, "Log file does not exist\n");
        }
        // clear the log file
        ofstream ofs;
        ofs.open(full_log_path, ofstream::trunc);
        ofs.close();
        if(file_log_updates_mapping.find(filename) != file_log_updates_mapping.end()) {
            //erase the log updates
            file_log_updates_mapping_mutex.lock();
            file_log_updates_mapping.erase(filename);
            file_log_updates_mapping_mutex.unlock();
        }

    } 
    unlockFile(fd);
    close(fd);
    VERBOSE_PRINT(do_verbose, "Success\n"); //On success returns non NULL.
    return fl;
}

int gtfs_close_file(gtfs_t* gtfs, file_t* fl) {
    int ret = -1;
    if (gtfs and fl) {
        VERBOSE_PRINT(do_verbose, "Closing file " << fl->filename << " inside directory " << gtfs->dirname << "\n");
    } else {
        VERBOSE_PRINT(do_verbose, "GTFileSystem or file does not exist\n");
        return ret;
    }
    //TODO: Add any additional initializations and checks, and complete the functionality
    if(fl->is_open == 0) {
        VERBOSE_PRINT(do_verbose, "File is not open\n");
        return ret;
    }else{
        fl->is_open = 0;
        VERBOSE_PRINT(do_verbose, "Success\n"); //On success returns 0.
        return 0;
    }
    return ret;
}

int gtfs_remove_file(gtfs_t* gtfs, file_t* fl) {
    int ret = -1;
    if (gtfs and fl) {
        VERBOSE_PRINT(do_verbose, "Removing file " << fl->filename << " inside directory " << gtfs->dirname << "\n");
    } else {
        VERBOSE_PRINT(do_verbose, "GTFileSystem or file does not exist\n");
        return ret;
    }
    //TODO: Add any additional initializations and checks, and complete the functionality
    if(fl->is_open == 1) {
        VERBOSE_PRINT(do_verbose, "File is open, close it first before removing\n");
        return ret;
    }else{
        string full_file_path = gtfs->dirname + "/" + fl->filename;
        string full_log_path = gtfs->dirname + "/" + fl->filename + ".log";
        // Lock the file before removing
        int fd = open(full_file_path.c_str(), O_RDWR);
        if (fd == -1) {
            perror("Error opening file");
            return ret;
        }
        if (!lockFile(fd)) {
            close(fd);
            return ret;
        }
        // remove the file
        int res = remove(full_file_path.c_str());
        if (res != 0) {
            VERBOSE_PRINT(do_verbose, "Error removing file\n");
            unlockFile(fd);
            close(fd);
            return ret;
        }
        unlockFile(fd);
        close(fd);
        // remove the log file
        res = remove(full_log_path.c_str());
        if (res != 0) {
            VERBOSE_PRINT(do_verbose, "Error removing log file\n");
            return ret;
        }
        // remove the file from the mappings
        filename_file_t_mapping_mutex.lock();
        filename_file_t_mapping.erase(fl->filename);
        filename_file_t_mapping_mutex.unlock();
        file_log_updates_mapping_mutex.lock();
        file_log_updates_mapping.erase(fl->filename);
        file_log_updates_mapping_mutex.unlock();
        // remove the file from the directory
        directory_files_mapping_mutex.lock();
        vector<string>& files = directory_files_mapping[gtfs->dirname];  
        for(auto it = files.begin(); it != files.end(); ++it) {
            if(*it == fl->filename) {
                files.erase(it);
                break;
            }
        }
        directory_files_mapping_mutex.unlock();
        VERBOSE_PRINT(do_verbose, "Success\n"); //On success returns 0.
        return 0;
    }
    return ret;
}

char* gtfs_read_file(gtfs_t* gtfs, file_t* fl, int offset, int length) {
    char* ret_data = new char[length];
    char* emptyStr = new char[1];
    emptyStr[0] = '\0';
    int data_in_log = 0;
    if (gtfs and fl) {
        VERBOSE_PRINT(do_verbose, "Reading " << length << " bytes starting from offset " << offset << " inside file " << fl->filename << "\n");
    } else {
        VERBOSE_PRINT(do_verbose, "GTFileSystem or file does not exist\n");
        return NULL;
    }
    //TODO: Add any additional initializations and checks, and complete the functionality
    // first find the most updated data in log
    if(file_log_updates_mapping.find(fl->filename) != file_log_updates_mapping.end()) {
        vector<write_t *> logs = file_log_updates_mapping[fl->filename];
        for(auto it = logs.begin(); it != logs.end(); ++it) {
            write_t *log = *it;
            if(log->offset == offset and log->length == length) {
                ret_data = log->data;
                data_in_log = 1;
                VERBOSE_PRINT(do_verbose, "Data found in log\n");
                return ret_data;
            }
        }
    }

    if(data_in_log == 0){
        // now find it in the file
        string full_file_path = gtfs->dirname + "/" + fl->filename;
        // Citation: from GenAI tools advice, don't skip white spaces
        fstream fs(full_file_path.c_str());
        fs >> noskipws;
        struct stat sb;
        if (stat(full_file_path.c_str(), &sb) == 0 && S_ISREG(sb.st_mode)) {
            VERBOSE_PRINT(do_verbose, "File exists\n");
            if(offset > sb.st_size) {
                VERBOSE_PRINT(do_verbose, "Offset is greater than file size\n");
                return emptyStr;
            }
            if(offset + length > sb.st_size) {
                VERBOSE_PRINT(do_verbose, "Offset + Length is greater than file size\n");
                return emptyStr;
            }
            fs.seekg(offset);
            char *data = new char[length];
            fs.read(data, length);
            data[length] = '\0';
            VERBOSE_PRINT(do_verbose, "Data read from file: " << data);
            ret_data = data;
        } else {
            VERBOSE_PRINT(do_verbose, "File does not exist\n");
            return NULL;
        }

    }

    VERBOSE_PRINT(do_verbose, "Success\n"); //On success returns pointer to data read.
    return ret_data;
}

write_t* gtfs_write_file(gtfs_t* gtfs, file_t* fl, int offset, int length, const char* data) {
    write_t *write_id = new write_t();
    if (gtfs and fl) {
        VERBOSE_PRINT(do_verbose, "Writting " << length << " bytes starting from offset " << offset << " inside file " << fl->filename << "\n");
    } else {
        VERBOSE_PRINT(do_verbose, "GTFileSystem or file does not exist\n");
        return NULL;
    }
    //TODO: Add any additional initializations and checks, and complete the functionality
    string full_file_path = gtfs->dirname + "/" + fl->filename;
    string full_log_path = gtfs->dirname + "/" + fl->filename + ".log";
    write_id->filename = fl->filename;
    write_id->full_file_path = full_file_path;
    write_id->full_log_path = full_log_path;
    write_id->offset = offset;
    write_id->length = length;
    write_id->data = (char *)data;

    vector<write_t *> logs;
    if(file_log_updates_mapping.find(fl->filename) != file_log_updates_mapping.end()) {
        logs = file_log_updates_mapping[fl->filename];
        logs.push_back(write_id);
        file_log_updates_mapping_mutex.lock();
        file_log_updates_mapping[fl->filename] = logs;
        file_log_updates_mapping_mutex.unlock();
    }else{
        logs.push_back(write_id);
        file_log_updates_mapping_mutex.lock();
        file_log_updates_mapping[fl->filename] = logs;
        file_log_updates_mapping_mutex.unlock();
    }

    VERBOSE_PRINT(do_verbose, "Success\n"); //On success returns non NULL.
    return write_id;
}

int gtfs_sync_write_file(write_t* write_id) {
    int ret = -1;
    if (write_id) {
        VERBOSE_PRINT(do_verbose, "Persisting write of " << write_id->length << " bytes starting from offset " << write_id->offset << " inside file " << write_id->filename << "\n");
    } else {
        VERBOSE_PRINT(do_verbose, "Write operation does not exist\n");
        return ret;
    }
    //TODO: Add any additional initializations and checks, and complete the functionality
    int log_fd = open(write_id->full_log_path.c_str(), O_RDWR | O_CREAT, 0666);
    if (log_fd == -1) {
        perror("Error opening log file");
        return ret;
    }
    if (!lockFile(log_fd)) {
        close(log_fd);
        return ret;
    }

    // check if log file is created, if not create it
    struct stat sb_log;
    if(stat(write_id->full_log_path.c_str(), &sb_log) != 0) {
        ofstream ofs;
        ofs.open(write_id->full_log_path);
        ofs.close();
    } 
    ofstream of;
    of.open(write_id->full_log_path, ofstream::app);
    // Citation: from GenAI tools advice, use pipe ("|") to separate the data
    string log_data = write_id->filename + "|" + to_string(write_id->offset) + 
    "|" + to_string(write_id->length) + "|" + write_id->data;
    VERBOSE_PRINT(do_verbose, "Writing log data: " << log_data << "\n");
    of << log_data << endl;
    of.close();

    unlockFile(log_fd);
    close(log_fd);
    int data_fd = open(write_id->full_file_path.c_str(), O_RDWR);
    if (data_fd == -1) {
        perror("Error opening data file");
        return ret;
    }
    if (!lockFile(data_fd)) {
        close(data_fd);
        return ret;
    }

    // Move the file offset to the desired location
    lseek(data_fd, write_id->offset, SEEK_SET);

    // Write the data using write() and capture the number of bytes written
    ssize_t bytesWritten = write(data_fd, write_id->data, write_id->length);
    if (bytesWritten == -1) {
        perror("Error writing to file");
        unlockFile(data_fd);
        close(data_fd);
        return ret;
    }

    VERBOSE_PRINT(do_verbose, "Bytes written to file: " << bytesWritten << "\n");

    unlockFile(data_fd);
    close(data_fd);

    VERBOSE_PRINT(do_verbose, "Success\n"); //On success returns number of bytes written.
    ret = bytesWritten;
    VERBOSE_PRINT(do_verbose, "bytes written: " << ret << "\n");
    return ret;
}

int gtfs_abort_write_file(write_t* write_id) {
    int ret = -1;
    if (write_id) {
        VERBOSE_PRINT(do_verbose, "Aborting write of " << write_id->length << " bytes starting from offset " << write_id->offset << " inside file " << write_id->filename << "\n");
    } else {
        VERBOSE_PRINT(do_verbose, "Write operation does not exist\n");
        return ret;
    }
    //TODO: Add any additional initializations and checks, and complete the functionality

    //discard the log in this function
    if(file_log_updates_mapping.find(write_id->filename) != file_log_updates_mapping.end()) {
        vector<write_t *> logs = file_log_updates_mapping[write_id->filename];
        for(auto it = logs.begin(); it != logs.end(); ++it) {
            write_t *log = *it;
            if(log->offset == write_id->offset && log->length == write_id->length && log->data == write_id->data) {
                logs.erase(it);
                break;
            }
        }
        file_log_updates_mapping_mutex.lock();
        file_log_updates_mapping[write_id->filename] = logs;
        file_log_updates_mapping_mutex.unlock();
        VERBOSE_PRINT(do_verbose, "Success.\n"); //On success returns 0.
        return 0;
    } 
    return ret;
}

// BONUS: Implement below API calls to get bonus credits

int gtfs_clean_n_bytes(gtfs_t *gtfs, int bytes){
    int ret = -1;
    if (gtfs) {
        VERBOSE_PRINT(do_verbose, "Cleaning up [ " << bytes << " bytes ] GTFileSystem inside directory " << gtfs->dirname << "\n");
    } else {
        VERBOSE_PRINT(do_verbose, "GTFileSystem does not exist\n");
        return ret;
    }

    VERBOSE_PRINT(do_verbose, "Success\n"); //On success returns 0.
    return ret;
}

int gtfs_sync_write_file_n_bytes(write_t* write_id, int bytes){
    int ret = -1;
    if (write_id) {
        VERBOSE_PRINT(do_verbose, "Persisting [ " << bytes << " bytes ] write of " << write_id->length << " bytes starting from offset " << write_id->offset << " inside file " << write_id->filename << "\n");
    } else {
        VERBOSE_PRINT(do_verbose, "Write operation does not exist\n");
        return ret;
    }

    VERBOSE_PRINT(do_verbose, "Success\n"); //On success returns 0.
    return ret;
}

