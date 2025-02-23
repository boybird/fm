// include/common.h
#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sqlite3.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <openssl/sha.h>
#include <openssl/evp.h>

// Error codes
#define FM_SUCCESS           0
#define FM_ERR_NOT_FOUND    -1
#define FM_ERR_ALREADY_EXISTS -2
#define FM_ERR_PERMISSION    -3
#define FM_ERR_DB_ERROR      -4
#define FM_ERR_SYSTEM        -5
#define FM_ERR_INVALID_PATH  -6

// File types
#define FILE_TYPE_FILE      "file"
#define FILE_TYPE_DIRECTORY "directory"

// Maximum path length
#define MAX_PATH_LENGTH 1024
#define MAX_NAME_LENGTH 256
#define BUFFER_SIZE 4096

typedef struct {
    int id;
    char name[MAX_NAME_LENGTH];
    char path[MAX_PATH_LENGTH];
    char type[20];
    size_t size;
    time_t created_at;
    time_t modified_at;
    int parent_id;
    char checksum[65];  // SHA-256 hash (64 chars + null terminator)
    char status[10];
} file_info_t;

typedef struct {
    file_info_t* items;
    size_t count;
} file_list_t;

#endif // COMMON_H
