// src/file_manager.c
#include "file_manager.h"
#include "db_manager.h"
#include "error_handler.h"

static char root_path[MAX_PATH_LENGTH];

static int calculate_checksum(const char* file_path, char* checksum) {
    SHA256_CTX sha256;
    unsigned char hash[SHA256_DIGEST_LENGTH];
    unsigned char buffer[BUFFER_SIZE];
    FILE* file = fopen(file_path, "rb");
    if (!file) return FM_ERR_NOT_FOUND;

    SHA256_Init(&sha256);
    size_t bytes;
    while ((bytes = fread(buffer, 1, BUFFER_SIZE, file)) != 0) {
        SHA256_Update(&sha256, buffer, bytes);
    }
    SHA256_Final(hash, &sha256);
    fclose(file);

    for(int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(checksum + (i * 2), "%02x", hash[i]);
    }
    checksum[64] = '\0';
    return FM_SUCCESS;
}

static int get_parent_path(const char* path, char* parent_path) {
    strncpy(parent_path, path, MAX_PATH_LENGTH);
    parent_path[MAX_PATH_LENGTH - 1] = '\0';
    
    char* last_slash = strrchr(parent_path, '/');
    if (!last_slash) return FM_ERR_INVALID_PATH;
    
    if (last_slash == parent_path) {
        parent_path[1] = '\0';
    } else {
        *last_slash = '\0';
    }
    return FM_SUCCESS;
}

int fm_init(const char* base_path) {
    strncpy(root_path, base_path, MAX_PATH_LENGTH);
    root_path[MAX_PATH_LENGTH - 1] = '\0';

    // Create root directory if it doesn't exist
    struct stat st;
    if (stat(root_path, &st) == -1) {
        if (mkdir(root_path, 0755) != 0) {
            error_log(FM_ERR_SYSTEM, "Failed to create root directory");
            return FM_ERR_SYSTEM;
        }
    }

    // Initialize database
    char db_path[MAX_PATH_LENGTH];
    snprintf(db_path, MAX_PATH_LENGTH, "%s/filedb.sqlite", root_path);
    return db_init(db_path);
}

int fm_create_file(const char* path, size_t size) {
    char full_path[MAX_PATH_LENGTH];
    snprintf(full_path, MAX_PATH_LENGTH, "%s/%s", root_path, path);

    // Check if file already exists
    if (access(full_path, F_OK) == 0) {
        return FM_ERR_ALREADY_EXISTS;
    }

    // Create new file
    FILE* file = fopen(full_path, "wb");
    if (!file) {
        error_log(FM_ERR_SYSTEM, "Failed to create file");
        return FM_ERR_SYSTEM;
    }

    // Allocate file size if specified
    if (size > 0) {
        fseek(file, size - 1, SEEK_SET);
        fputc('\0', file);
    }
    fclose(file);

    // Prepare file info for database
    file_info_t file_info;
    memset(&file_info, 0, sizeof(file_info));
    
    char* name = strrchr(path, '/');
    name = name ? name + 1 : (char*)path;
    strncpy(file_info.name, name, MAX_NAME_LENGTH - 1);
    strncpy(file_info.path, path, MAX_PATH_LENGTH - 1);
    strcpy(file_info.type, FILE_TYPE_FILE);
    file_info.size = size;

    // Calculate checksum
    calculate_checksum(full_path, file_info.checksum);

    // Get parent directory info
    char parent_path[MAX_PATH_LENGTH];
    get_parent_path(path, parent_path);
    file_info_t parent_info;
    if (db_get_file_info(parent_path, &parent_info) == FM_SUCCESS) {
        file_info.parent_id = parent_info.id;
    }

    return db_insert_file(&file_info);
}

int fm_create_directory(const char* path) {
    char full_path[MAX_PATH_LENGTH];
    snprintf(full_path, MAX_PATH_LENGTH, "%s/%s", root_path, path);

    // Check if directory already exists
    struct stat st;
    if (stat(full_path, &st) == 0) {
        return FM_ERR_ALREADY_EXISTS;
    }

    // Create directory
    if (mkdir(full_path, 0755) != 0) {
        error_log(FM_ERR_SYSTEM, "Failed to create directory");
        return FM_ERR_SYSTEM;
    }

    // Prepare directory info for database
    file_info_t dir_info;
    memset(&dir_info, 0, sizeof(dir_info));
    
    char* name = strrchr(path, '/');
    name = name ? name + 1 : (char*)path;
    strncpy(dir_info.name, name, MAX_NAME_LENGTH - 1);
    strncpy(dir_info.path, path, MAX_PATH_LENGTH - 1);
    strcpy(dir_info.type, FILE_TYPE_DIRECTORY);

    // Get parent directory info
    char parent_path[MAX_PATH_LENGTH];
    get_parent_path(path, parent_path);
    file_info_t parent_info;
    if (db_get_file_info(parent_path, &parent_info) == FM_SUCCESS) {
        dir_info.parent_id = parent_info.id;
    }

    return db_insert_file(&dir_info);
}

int fm_copy(const char* src, const char* dest) {
    char full_src[MAX_PATH_LENGTH], full_dest[MAX_PATH_LENGTH];
    snprintf(full_src, MAX_PATH_LENGTH, "%s/%s", root_path, src);
    snprintf(full_dest, MAX_PATH_LENGTH, "%s/%s", root_path, dest);

    // Check if source exists and destination doesn't
    if (access(full_src, F_OK) != 0) return FM_ERR_NOT_FOUND;
    if (access(full_dest, F_OK) == 0) return FM_ERR_ALREADY_EXISTS;

    // Copy file
    FILE *source = fopen(full_src, "rb");
    FILE *destination = fopen(full_dest, "wb");
    if (!source || !destination) {
        if (source) fclose(source);
        if (destination) fclose(destination);
        return FM_ERR_SYSTEM;
    }

    char buffer[BUFFER_SIZE];
    size_t bytes;
    while ((bytes = fread(buffer, 1, BUFFER_SIZE, source)) != 0) {
        fwrite(buffer, 1, bytes, destination);
    }

    fclose(source);
    fclose(destination);

    // Update database
    file_info_t src_info;
    if (db_get_file_info(src, &src_info) != FM_SUCCESS) {
        return FM_ERR_DB_ERROR;
    }

    file_info_t dest_info = src_info;
    char* name = strrchr(dest, '/');
    name = name ? name + 1 : (char*)dest;
    strncpy(dest_info.name, name, MAX_NAME_LENGTH - 1);
    strncpy(dest_info.path, dest, MAX_PATH_LENGTH - 1);

    calculate_checksum(full_dest, dest_info.checksum);

    // Get parent directory info for destination
    char parent_path[MAX_PATH_LENGTH];
    get_parent_path(dest, parent_path);
    file_info_t parent_info;
    if (db_get_file_info(parent_path, &parent_info) == FM_SUCCESS) {
        dest_info.parent_id = parent_info.id;
    }

    return db_insert_file(&dest_info);
}

int fm_rename(const char* old_path, const char* new_path) {
    char full_old[MAX_PATH_LENGTH], full_new[MAX_PATH_LENGTH];
    snprintf(full_old, MAX_PATH_LENGTH, "%s/%s", root_path, old_path);
    snprintf(full_new, MAX_PATH_LENGTH, "%s/%s", root_path, new_path);

    if (rename(full_old, full_new) != 0) {
        error_log(FM_ERR_SYSTEM, "Failed to rename file");
        return FM_ERR_SYSTEM;
    }

    // Update database entry
    file_info_t file_info;
    if (db_get_file_info(old_path, &file_info) != FM_SUCCESS) {
        return FM_ERR_DB_ERROR;
    }

    char* name = strrchr(new_path, '/');
    name = name ? name + 1 : (char*)new_path;
    strncpy(file_info.name, name, MAX_NAME_LENGTH - 1);
    strncpy(file_info.path, new_path, MAX_PATH_LENGTH - 1);

    if (strcmp(file_info.type, FILE_TYPE_FILE) == 0) {
        calculate_checksum(full_new, file_info.checksum);
    }

    return db_update_file(&file_info);
}

int fm_delete(const char* path) {
    char full_path[MAX_PATH_LENGTH];
    snprintf(full_path, MAX_PATH_LENGTH, "%s/%s", root_path, path);

    struct stat st;
    if (stat(full_path, &st) != 0) {
        return FM_ERR_NOT_FOUND;
    }

    if (S_ISDIR(st.st_mode)) {
        DIR* dir = opendir(full_path);
        if (!dir) return FM_ERR_SYSTEM;

        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }

            char sub_path[MAX_PATH_LENGTH];
            snprintf(sub_path, MAX_PATH_LENGTH, "%s/%s", path, entry->d_name);
            int result = fm_delete(sub_path);
            if (result != FM_SUCCESS) {
                closedir(dir);
                return result;
            }
        }
        closedir(dir);

        if (rmdir(full_path) != 0) {
            return FM_ERR_SYSTEM;
        }
    } else {
        if (unlink(full_path) != 0) {
            return FM_ERR_SYSTEM;
        }
    }

    return db_delete_file(path);
}

int fm_get_file_info(const char* path, file_info_t* info) {
    return db_get_file_info(path, info);
}

int fm_list_directory(const char* path, file_list_t* list) {
    return db_list_directory(path, list);
}

void fm_cleanup(void) {
    db_close();
}