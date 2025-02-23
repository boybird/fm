// include/db_manager.h
#ifndef DB_MANAGER_H
#define DB_MANAGER_H

#include "common.h"

// Initialize database
int db_init(const char* db_path);

// Close database connection
void db_close(void);

// File management operations
int db_insert_file(const file_info_t* file_info);
int db_update_file(const file_info_t* file_info);
int db_delete_file(const char* path);
int db_get_file_info(const char* path, file_info_t* file_info);
int db_list_directory(const char* path, file_list_t* list);

#endif // DB_MANAGER_H