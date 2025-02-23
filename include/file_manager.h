// include/file_manager.h
#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include "common.h"

// Initialize file management system
int fm_init(const char* root_path);

// File operations
int fm_create_file(const char* path, size_t size);
int fm_create_directory(const char* path);
int fm_copy(const char* src, const char* dest);
int fm_rename(const char* old_path, const char* new_path);
int fm_delete(const char* path);

// File information
int fm_get_file_info(const char* path, file_info_t* info);
int fm_list_directory(const char* path, file_list_t* list);

// json
int fm_batch_do_json(const char *json_file);

// Cleanup
void fm_cleanup(void);

#endif // FILE_MANAGER_H
