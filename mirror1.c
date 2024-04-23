#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <stdbool.h>
#include <limits.h>
#include <sys/wait.h>
#include <pthread.h> 
#include <stdarg.h> 

#define PORT 8889
#define BACKLOG 5
#define MAXDATASIZE 1024
#define MIRROR1_IP "127.0.0.1"
#define MIRROR1_PORT 8889
#define MIRROR2_IP "127.0.0.1"
#define MIRROR2_PORT 8890
#define PATH_MAX_LENGTH 512
#define TAR_COMMAND "tar -czf"
#define DATE_FORMAT "%Y-%m-%d"
#define MAX_PATH_LENGTH 1024
#define LOG_FILE "server.log"




// Enum for log levels
enum LogLevel { INFO, WARNING, ERROR };

bool search_file(const char *path, const char *filename, char *response) {
    DIR *dir;
    struct dirent *entry;
    struct stat file_stat;

    dir = opendir(path);
    if (dir == NULL) {
        perror("Error opening directory");
        return false;
    }

    while ((entry = readdir(dir)) != NULL) {
        char full_path[MAXDATASIZE];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        if (strcmp(entry->d_name, filename) == 0 && stat(full_path, &file_stat) == 0) {
            // Construct response string with filename, size, date created, and permissions
            snprintf(response, MAXDATASIZE, "Filename: %s\nSize: %ld bytes\nDate created: %s\nPermissions: %o", entry->d_name, file_stat.st_size, ctime(&file_stat.st_mtime), file_stat.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO));
            closedir(dir);
            return true;
        }

        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            // Recursively search subdirectories
            if (search_file(full_path, filename, response)) {
                closedir(dir);
                return true;
            }
        }
    }

    closedir(dir);
    return false;
}

// Function to handle w24fn command
void handle_w24fn(int client_socket, const char *filename) {
    char response[MAXDATASIZE];
    bool file_found = search_file(getenv("HOME"), filename, response);

    if (file_found) {
        // Send response to client if file is found
        send(client_socket, response, strlen(response), 0);
    } else {
        // Send "File not found" response if file is not found
        char error_response[MAXDATASIZE];
        snprintf(error_response, sizeof(error_response), "File '%s' not found", filename);
        send(client_socket, error_response, strlen(error_response), 0);
    }
}




int compare_strings(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

void handleDirectoryListing(int client_socket) {
    DIR *dir;
    struct dirent *entry;
    char *file_names[MAXDATASIZE]; // Array to store file names
    int num_entries = 0;

    // Open the current directory
    dir = opendir(".");
    if (dir == NULL) {
        perror("Error opening directory");
        send(client_socket, "Error opening directory", strlen("Error opening directory"), 0);
        return;
    }

    // Read directory entries and store file names in the array
    while ((entry = readdir(dir)) != NULL && num_entries < MAXDATASIZE) {
        file_names[num_entries] = strdup(entry->d_name);
        num_entries++;
    }

    // Close the directory
    closedir(dir);

    // Sort the file names
    qsort(file_names, num_entries, sizeof(char *), compare_strings);

    // Concatenate the sorted file names into the response string
    char response[MAXDATASIZE] = "";
    for (int i = 0; i < num_entries; i++) {
        strcat(response, file_names[i]);
        strcat(response, "\n");
        free(file_names[i]); // Free dynamically allocated memory
    }

    // Send the response string to the client
    send(client_socket, response, strlen(response), 0);
}

void handle_dirlist_t(int client_socket) {
    DIR *dir;
    struct dirent *entry;
    char response[MAXDATASIZE] = "";
    int num_entries = 0;

    // Open the current directory
    dir = opendir(".");
    if (dir == NULL) {
        log_message(ERROR, "Error opening directory: %s", strerror(errno));
        send(client_socket, "Error opening directory", strlen("Error opening directory"), 0);
        return;
    }

    // Read directory entries and store them in the response string
    while ((entry = readdir(dir)) != NULL && num_entries < MAXDATASIZE - 1) {
        strcat(response, entry->d_name);
        strcat(response, "\n");
        num_entries++;
    }

    // Send the response string to the client
    send(client_socket, response, strlen(response), 0);

    // Close the directory
    closedir(dir);
}

void *handle_client(void *arg) {
    int client_socket = *((int *)arg);
    char buffer[MAXDATASIZE];

    // Receive client request
    if (recv(client_socket, buffer, MAXDATASIZE, 0) == -1) {
        perror("recv");
        close(client_socket);
        pthread_exit(NULL);
    }

    // Parse client request and dispatch appropriate handler
    if (strcmp(buffer, "w24fn") == 0) {
        // Extract filename from client request
        char filename[MAXDATASIZE];
        if (recv(client_socket, filename, MAXDATASIZE, 0) == -1) {
            perror("recv");
            close(client_socket);
            pthread_exit(NULL);
        }
        handle_w24fn(client_socket, filename);
    } else if (strcmp(buffer, "dirlist_t") == 0) {
        handle_dirlist_t(client_socket);
    } else if (strcmp(buffer, "w24fz") == 0) {
        // Extract size parameters from client request
        long size1, size2;
        if (recv(client_socket, &size1, sizeof(long), 0) == -1 ||
            recv(client_socket, &size2, sizeof(long), 0) == -1) {
            perror("recv");
            close(client_socket);
            pthread_exit(NULL);
        }
        handle_w24fz(client_socket, size1, size2);
    } else if (strcmp(buffer, "w24ft") == 0) {
        // Extract extensions from client request
        char extensions[MAXDATASIZE];
        if (recv(client_socket, extensions, MAXDATASIZE, 0) == -1) {
            perror("recv");
            close(client_socket);
            pthread_exit(NULL);
        }
        handle_w24ft(client_socket, extensions);
    } else if (strcmp(buffer, "w24fdb") == 0) {
        // Extract date from client request
        char date[MAXDATASIZE];
        if (recv(client_socket, date, MAXDATASIZE, 0) == -1) {
            perror("recv");
            close(client_socket);
            pthread_exit(NULL);
        }
        handle_w24fdb(client_socket, date);
    } else if (strcmp(buffer, "w24fda") == 0) {
        // Extract date from client request
        char date[MAXDATASIZE];
        if (recv(client_socket, date, MAXDATASIZE, 0) == -1) {
            perror("recv");
            close(client_socket);
            pthread_exit(NULL);
        }
        handle_w24fda(client_socket, date);
    } else if (strcmp(buffer, "dirlist") == 0) {
        handleDirectoryListing(client_socket);
    } else {
        log_message(ERROR, "Unknown command received: %s", buffer);
    }

    close(client_socket);
    pthread_exit(NULL);
}



void handle_w24fz(int client_socket, long size1, long size2) {
    char response[MAXDATASIZE] = "";
    bool file_found = false;

    // Open the home directory
    DIR *dir = opendir(getenv("HOME"));
    if (dir == NULL) {
        perror("Error opening directory");
        send(client_socket, "Error opening directory", strlen("Error opening directory"), 0);
        return;
    }

    // Traverse directory tree and find files within the specified size range
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        struct stat st;
        char path[MAX_PATH_LENGTH];
        snprintf(path, sizeof(path), "%s/%s", getenv("HOME"), entry->d_name);

        if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) { // Check if it's a regular file
            if (st.st_size >= size1 && st.st_size <= size2) {
                // Add file path to response
                strcat(response, path);
                strcat(response, "\n");
                file_found = true;
            }
        }
    }

    closedir(dir);

    if (!file_found) {
        // Send "No file found" response if no file is found within the size range
        send(client_socket, "No file found", strlen("No file found"), 0);
        return;
    }

    // Create a temporary file to store the list of files
    char temp_file[] = "/home/username/w24project/w24fz_temp_list.txt";
    FILE *temp_file_ptr = fopen(temp_file, "w");
    if (!temp_file_ptr) {
        perror("Error creating temporary file");
        send(client_socket, "Error creating temporary file", strlen("Error creating temporary file"), 0);
        return;
    }

    // Write the list of files to the temporary file
    fputs(response, temp_file_ptr);
    fclose(temp_file_ptr);

    // Create the tar.gz file
    char tar_command[MAXDATASIZE];
    snprintf(tar_command, sizeof(tar_command), "tar -czf /home/username/w24project/temp.tar.gz -T %s", temp_file);
    int ret = system(tar_command);
    if (ret == -1) {
        perror("Error creating tar.gz file");
        send(client_socket, "Error creating tar.gz file", strlen("Error creating tar.gz file"), 0);
        return;
    }

    // Open the temporary tar.gz file for reading
    FILE *tar_file = fopen("/home/username/w24project/temp.tar.gz", "rb");
    if (!tar_file) {
        perror("Error opening temporary tar.gz file");
        send(client_socket, "Error opening temporary tar.gz file", strlen("Error opening temporary tar.gz file"), 0);
        return;
    }

    // // Send the contents of the temporary tar.gz file to the client
    char buffer[MAXDATASIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), tar_file)) > 0) {
        //send(client_socket, buffer, bytes_read, 0);
    }

    // Close the temporary tar.gz file
    fclose(tar_file);

    // Send a confirmation message to the client
    send(client_socket, "TAR file transmission complete", strlen("TAR file transmission complete"), 0);
}

void handle_w24ft(int client_socket, const char *extensions) {
    printf("Handling w24ft command...\n");

    // Create the w24project directory if it doesn't exist
    mkdir("/home/username/w24project", 0777); // 0777 sets permissions to allow read, write, and execute for all users
    printf("Created w24project directory if not exists.\n");

    // Parse the extension list
    char ext1[10], ext2[10], ext3[10];
    int num_matched = sscanf(extensions, "%s %s %s", ext1, ext2, ext3);
    printf("Number of extensions matched: %d\n", num_matched);

    // Ensure at least one extension is provided and up to 3 extensions are allowed
    if (num_matched < 1 || num_matched > 3) {
        printf("Invalid number of extensions. Provide 1 to 3 extensions.\n");
        send(client_socket, "Invalid number of extensions. Provide 1 to 3 extensions.", strlen("Invalid number of extensions. Provide 1 to 3 extensions."), 0);
        return;
    }

    // Construct the find command to search for files with specified extensions in the specified directory
    char find_command[MAXDATASIZE];
    snprintf(find_command, sizeof(find_command), "find ~ -type f \\( -name \"*.%s\"", ext1);
    if (num_matched >= 2) {
        snprintf(find_command + strlen(find_command), sizeof(find_command) - strlen(find_command), " -o -name \"*.%s\"", ext2);
    }
    if (num_matched == 3) {
        snprintf(find_command + strlen(find_command), sizeof(find_command) - strlen(find_command), " -o -name \"*.%s\"", ext3);
    }
    strcat(find_command, " \\)");
    printf("Find command: %s\n", find_command);

    // Execute the find command to get a list of files matching the extensions
    FILE *find_output = popen(find_command, "r");
    if (!find_output) {
        perror("Error executing find command");
        send(client_socket, "Error executing find command", strlen("Error executing find command"), 0);
        return;
    }

    // Check if any files are found
    char file_path[MAXDATASIZE];
    if (fgets(file_path, sizeof(file_path), find_output) == NULL) {
        printf("No files found with the specified extensions.\n");
        send(client_socket, "No file found", strlen("No file found"), 0);
        pclose(find_output);
        return;
    }

    // Create a temporary file to store the list of files
    char temp_file[] = "/home/username/w24project/w24ft_temp_list.txt";
    FILE *temp_file_ptr = fopen(temp_file, "w");
    if (!temp_file_ptr) {
        perror("Error creating temporary file");
        send(client_socket, "Error creating temporary file", strlen("Error creating temporary file"), 0);
        pclose(find_output);
        return;
    }
    printf("Temporary file created: %s\n", temp_file);

    // Read the list of files from the find command output and write to the temporary file
    do {
        // Remove newline character from file path
        file_path[strcspn(file_path, "\n")] = '\0';
        fprintf(temp_file_ptr, "%s\n", file_path);
    } while (fgets(file_path, sizeof(file_path), find_output));

    // Close the temporary file and find command output
    fclose(temp_file_ptr);
    pclose(find_output);

    // Compress the files into a temporary tar.gz archive
    char tar_command[MAXDATASIZE];
    snprintf(tar_command, sizeof(tar_command), "%s /home/username/w24project/w24ft_temp.tar.gz -T /home/username/w24project/w24ft_temp_list.txt", TAR_COMMAND);
    printf("Tar command: %s\n", tar_command);
    int ret = system(tar_command);
    if (ret == -1) {
        perror("Error compressing files into tar.gz");
        send(client_socket, "Error compressing files into tar.gz", strlen("Error compressing files into tar.gz"), 0);
        return;
    }

    // Open the temporary tar.gz file for reading
    FILE *tar_file = fopen("/home/username/w24project/w24ft_temp.tar.gz", "rb");
    if (!tar_file) {
        perror("Error opening temporary tar.gz file");
        send(client_socket, "Error opening temporary tar.gz file", strlen("Error opening temporary tar.gz file"), 0);
        return;
    }

    // Send the contents of the temporary tar.gz file to the client
    char buffer[MAXDATASIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), tar_file)) > 0) {
        //send(client_socket, buffer, bytes_read, 0);
    }

    // Close the temporary tar.gz file
    fclose(tar_file);
    send(client_socket, "Tar created successfully", strlen("Tar created successfully"), 0);

}

// Function to convert date string to time_t
time_t convert_date_string(const char *date) {
    struct tm tm = {0};
    if (strptime(date, DATE_FORMAT, &tm) == NULL) {
        perror("Date parsing failed");
        exit(EXIT_FAILURE);
    }
    return mktime(&tm);
}

// Function to recursively search for files created before or on the specified date, ignoring files and directories starting with "."
int search_files_by_date(const char *path, time_t target_date, FILE *temp_file) {
    DIR *dir;
    struct dirent *entry;
    struct stat file_stat;
    int files_found = 0;

    dir = opendir(path);
    if (dir == NULL) {
        perror("Error opening directory");
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        // Ignore files and directories starting with "."
        if (entry->d_name[0] == '.') {
            continue;
        }

        char full_path[MAX_PATH_LENGTH];
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue; // Skip "." and ".." directories
        }

        if (stat(full_path, &file_stat) == 0) {
            if (S_ISREG(file_stat.st_mode) && difftime(file_stat.st_ctime, target_date) <= 0) {
                // File creation date is before or on the target date
                fprintf(temp_file, "%s\n", full_path);
                files_found = 1;
            } else if (S_ISDIR(file_stat.st_mode)) {
                // Recursively search directories
                files_found = search_files_by_date(full_path, target_date, temp_file);
                if (files_found == -1) {
                    closedir(dir);
                    return -1;
                }
            }
        }
    }

    closedir(dir);
    return files_found;
}


// Function to handle w24fdb command
void handle_w24fdb(int client_socket, const char *date) {
    // Convert date string to time_t
    time_t target_date = convert_date_string(date);

    // Create a temporary file to store the list of files
    char temp_file_path[] = "/home/username/w24project/w24fdb_temp_list.txt";
    FILE *temp_file = fopen(temp_file_path, "w");
    if (!temp_file) {
        perror("Error creating temporary file");
        send(client_socket, "Error creating temporary file", strlen("Error creating temporary file"), 0);
        return;
    }

    // Start searching files recursively from the home directory
    int files_found = search_files_by_date(getenv("HOME"), target_date, temp_file);

    if (files_found == -1) {
        fclose(temp_file);
        return;
    }

    // Close the temporary file
    fclose(temp_file);

    if (files_found == 0) {
        // Send message to client if no files were found
        send(client_socket, "No files found with the specified creation date or earlier.", strlen("No files found with the specified creation date or earlier."), 0);
        return;
    }

    // Create the tar.gz file
    char tar_command[MAXDATASIZE];
    snprintf(tar_command, sizeof(tar_command), "tar -czf /home/username/w24project/w24fdb_temp.tar.gz -T %s", temp_file_path);
    int ret = system(tar_command);
    if (ret == -1) {
        perror("Error creating tar.gz file");
        send(client_socket, "Error creating tar.gz file", strlen("Error creating tar.gz file"), 0);
        return;
    }

    // Open the temporary tar.gz file for reading
    FILE *tar_file = fopen("/home/username/w24project/w24fdb_temp.tar.gz", "rb");
    if (!tar_file) {
        perror("Error opening temporary tar.gz file");
        send(client_socket, "Error opening temporary tar.gz file", strlen("Error opening temporary tar.gz file"), 0);
        return;
    }

    // Send the contents of the temporary tar.gz file to the client
    char buffer[MAXDATASIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), tar_file)) > 0) {
        //send(client_socket, buffer, bytes_read, 0);
    }

    // Close the temporary tar.gz file
    fclose(tar_file);

    // Send a confirmation message to the client
    send(client_socket, "TAR file transmission complete", strlen("TAR file transmission complete"), 0);
}
// Function to check if a file's creation date is greater than or equal to the target date
int is_file_newer_or_equal(const char *file_path, time_t target_date) {
    struct stat file_stat;
    if (stat(file_path, &file_stat) == -1) {
        perror("Error getting file status");
        return 0;
    }
    // Check if the file's creation date is greater than or equal to the target date
    return difftime(file_stat.st_ctime, target_date) >= 0;
}

// Recursive function to search for files by date in a directory tree
int search_files_by_date_recursive(const char *dir_path, time_t target_date, FILE *output_file) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        perror("Error opening directory");
        return -1;
    }

    struct dirent *entry;
    int files_found = 0;
    while ((entry = readdir(dir)) != NULL) {
        // Skip "." and ".." directories
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Construct the full path of the current entry
        char entry_path[MAX_PATH_LENGTH];
        snprintf(entry_path, sizeof(entry_path), "%s/%s", dir_path, entry->d_name);

        // Check if the entry is a directory
        if (entry->d_type == DT_DIR) {
            // Recursively search in the subdirectory
            int subdirectory_files_found = search_files_by_date_recursive(entry_path, target_date, output_file);
            if (subdirectory_files_found == -1) {
                closedir(dir);
                return -1;
            }
            files_found += subdirectory_files_found;
        } else {
            // Check if the file's creation date is greater than or equal to the target date
            if (is_file_newer_or_equal(entry_path, target_date)) {
                // Write the file path to the output file
                fprintf(output_file, "%s\n", entry_path);
                files_found = 1;
            }
        }
    }

    closedir(dir);
    return files_found;
}

// Function to handle w24fda command
void handle_w24fda(int client_socket, const char *date) {
    // Convert date string to time_t
    time_t target_date = convert_date_string(date);

    // Create a temporary file to store the list of files
    char temp_file_path[] = "/home/username/w24project/w24fda_temp_list.txt";
    FILE *temp_file = fopen(temp_file_path, "w");
    if (!temp_file) {
        perror("Error creating temporary file");
        send(client_socket, "Error creating temporary file", strlen("Error creating temporary file"), 0);
        return;
    }

    // Start searching files recursively from the home directory
    int files_found = search_files_by_date_recursive(getenv("HOME"), target_date, temp_file);

    if (files_found == -1) {
        fclose(temp_file);
        return;
    }

    // Close the temporary file
    fclose(temp_file);

    if (files_found == 0) {
        // Send message to client if no files were found
        send(client_socket, "No files found with the specified creation date or later.", strlen("No files found with the specified creation date or later."), 0);
        return;
    }

    // Create the tar.gz file
    char tar_command[MAXDATASIZE];
    snprintf(tar_command, sizeof(tar_command), "tar -czf /home/username/w24project/w24fda_temp.tar.gz -T %s", temp_file_path);
    int ret = system(tar_command);
    if (ret == -1) {
        perror("Error creating tar.gz file");
        send(client_socket, "Error creating tar.gz file", strlen("Error creating tar.gz file"), 0);
        return;
    }

    // Open the temporary tar.gz file for reading
    FILE *tar_file = fopen("/home/username/w24project/w24fda_temp.tar.gz", "rb");
    if (!tar_file) {
        perror("Error opening temporary tar.gz file");
        send(client_socket, "Error opening temporary tar.gz file", strlen("Error opening temporary tar.gz file"), 0);
        return;
    }

    // Send the contents of the temporary tar.gz file to the client
    char buffer[MAXDATASIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), tar_file)) > 0) {
        //send(client_socket, buffer, bytes_read, 0);
    }

    // Close the temporary tar.gz file
    fclose(tar_file);

    // Send a confirmation message to the client
    send(client_socket, "TAR file transmission complete", strlen("TAR file transmission complete"), 0);
}



//logmessage function
void log_message(enum LogLevel level, const char *format, ...) {
    va_list args;
    FILE *log_file;
    time_t current_time;
    char time_string[30];

    // Open the log file in append mode
    log_file = fopen(LOG_FILE, "a");
    if (log_file == NULL) {
        perror("Error opening log file");
        return;
    }


    // Get the current time
    current_time = time(NULL);
    strftime(time_string, sizeof(time_string), "%Y-%m-%d %H:%M:%S", localtime(&current_time));

    // Write the log message with timestamp and log level
    fprintf(log_file, "[%s] ", time_string);
    switch (level) {
        case INFO:
            fprintf(log_file, "[INFO] ");
            break;
        case WARNING:
            fprintf(log_file, "[WARNING] ");
            break;
        case ERROR:
            fprintf(log_file, "[ERROR] ");
            break;
    }

    // Write the formatted message
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);

    // Add newline character
    fprintf(log_file, "\n");

    // Close the log file
    fclose(log_file);
}


void handle_direct_command(int client_socket, const char *buffer) {
    if (strcmp(buffer, "dirlist -a") == 0) {
        handleDirectoryListing(client_socket);
    } else if (strcmp(buffer, "dirlist -t") == 0) {
        handle_dirlist_t(client_socket);
    } else if (strcmp(buffer, "quitc") == 0) {
        // Handle quit command
        char response[] = "quitc"; // Send confirmation to client
        send(client_socket, response, strlen(response), 0);
        close(client_socket);
    } else if (strncmp(buffer, "w24fn ", 6) == 0) {
        // Extract filename from client request
        char filename[MAXDATASIZE];
        sscanf(buffer + 6, "%s", filename);
        handle_w24fn(client_socket, filename);
    } else if (strncmp(buffer, "w24fz ", 6) == 0) {
        // Extract size parameters from client request
        long size1, size2;
        if (sscanf(buffer + 6, "%ld %ld", &size1, &size2) != 2) {
            perror("Invalid command syntax for w24fz");
            close(client_socket);
            return;
        }
        handle_w24fz(client_socket, size1, size2);
    } else if (strncmp(buffer, "w24ft ", 6) == 0) {
        // Extract extensions from client request
        char extensions[MAXDATASIZE];
        sscanf(buffer + 6, "%s", extensions);
        handle_w24ft(client_socket, extensions);
    } else if (strncmp(buffer, "w24fdb ", 7) == 0) {
        // Extract date from client request
        char date[MAXDATASIZE];
        sscanf(buffer + 7, "%s", date);
        handle_w24fdb(client_socket, date);
    } else if (strncmp(buffer, "w24fda ", 7) == 0) {
        // Extract date from client request
        char date[MAXDATASIZE];
        sscanf(buffer + 7, "%s", date);
        handle_w24fda(client_socket, date);
    } else {
        // Handle unknown command
        char response[] = "Unknown command";
        send(client_socket, response, strlen(response), 0);
    }
}

int main() {
    int sockfd, new_fd;
    struct sockaddr_in my_addr, their_addr;
    socklen_t sin_size;
    int pid;
    int connection_count = 0;

    char buffer[MAXDATASIZE];

    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(1);
    }

    // Server address setup
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(PORT);
    my_addr.sin_addr.s_addr = INADDR_ANY;
    memset(&(my_addr.sin_zero), '\0', 8);

    // Bind socket
    if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1) {
        perror("Bind failed");
        exit(1);
    }

    // Listen for connections
    if (listen(sockfd, BACKLOG) == -1) {
        perror("Listen failed");
        exit(1);
    }

    log_message(INFO, "Server started. Listening on port %d", PORT);

    while(1) {  
        sin_size = sizeof(struct sockaddr_in);

        if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size)) == -1) {
            perror("accept");
            continue;
        }

    

        // Log client connection
        log_message(INFO, "Connection from %s", inet_ntoa(their_addr.sin_addr));

        // Receive command from client
        ssize_t bytes_received = recv(new_fd, buffer, MAXDATASIZE, 0);
        if (bytes_received <= 0) {
            perror("Receive failed");
            close(new_fd);
            continue;
        }
        buffer[bytes_received] = '\0'; // Null-terminate the received data

        printf("Received command: %s\n", buffer);

        // Handle client commands
        handle_direct_command(new_fd, buffer);

        close(new_fd);
    }

    close(sockfd);
    return 0;
}

