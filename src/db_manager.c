// src/db_manager.c
#include "db_manager.h"
#include "error_handler.h"
#include <stdarg.h>

static sqlite3* db = NULL;

static const char* CREATE_TABLE_SQL =
    "CREATE TABLE IF NOT EXISTS fileMana ("
    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "name TEXT NOT NULL,"
    "path TEXT NOT NULL UNIQUE,"
    "type TEXT CHECK(type IN ('file', 'directory')) NOT NULL,"
    "size INTEGER,"
    "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
    "modified_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
    "parent_id INTEGER,"
    "checksum TEXT,"
    "status TEXT DEFAULT 'active',"
    "FOREIGN KEY (parent_id) REFERENCES fileMana(id));";

static int execute_sql(const char* sql, ...) {
    char* error_msg = NULL;
    char formatted_sql[4096];
    va_list args;
    
    va_start(args, sql);
    vsnprintf(formatted_sql, sizeof(formatted_sql), sql, args);
    va_end(args);
    
    if (sqlite3_exec(db, formatted_sql, NULL, NULL, &error_msg) != SQLITE_OK) {
        error_log(FM_ERR_DB_ERROR, error_msg);
        sqlite3_free(error_msg);
        return FM_ERR_DB_ERROR;
    }
    return FM_SUCCESS;
}

static int callback_get_file(void* data, int argc, char** argv, char** col_names) {
    file_info_t* file_info = (file_info_t*)data;
    
    for (int i = 0; i < argc; i++) {
        if (strcmp(col_names[i], "id") == 0) file_info->id = atoi(argv[i]);
        else if (strcmp(col_names[i], "name") == 0) strncpy(file_info->name, argv[i], MAX_NAME_LENGTH-1);
        else if (strcmp(col_names[i], "path") == 0) strncpy(file_info->path, argv[i], MAX_PATH_LENGTH-1);
        else if (strcmp(col_names[i], "type") == 0) strncpy(file_info->type, argv[i], 19);
        else if (strcmp(col_names[i], "size") == 0) file_info->size = argv[i] ? atoll(argv[i]) : 0;
        else if (strcmp(col_names[i], "parent_id") == 0) file_info->parent_id = argv[i] ? atoi(argv[i]) : 0;
        else if (strcmp(col_names[i], "checksum") == 0 && argv[i]) strncpy(file_info->checksum, argv[i], 64);
    }
    return 0;
}

int db_init(const char* db_path) {
    if (sqlite3_open(db_path, &db) != SQLITE_OK) {
        error_log(FM_ERR_DB_ERROR, sqlite3_errmsg(db));
        return FM_ERR_DB_ERROR;
    }

    return execute_sql(CREATE_TABLE_SQL);
}

void db_close(void) {
    if (db) {
        sqlite3_close(db);
        db = NULL;
    }
}

int db_insert_file(const file_info_t* file_info) {
    return execute_sql(
        "INSERT INTO fileMana (name, path, type, size, parent_id, checksum) "
        "VALUES ('%s', '%s', '%s', %zu, %d, '%s');",
        file_info->name, file_info->path, file_info->type,
        file_info->size, file_info->parent_id, file_info->checksum
    );
}

int db_update_file(const file_info_t* file_info) {
    return execute_sql(
        "UPDATE fileMana SET name = '%s', size = %zu, modified_at = CURRENT_TIMESTAMP, "
        "checksum = '%s' WHERE path = '%s';",
        file_info->name, file_info->size, file_info->checksum, file_info->path
    );
}

int db_delete_file(const char* path) {
    return execute_sql(
        "UPDATE fileMana SET status = 'deleted' WHERE path = '%s';",
        path
    );
}

int db_get_file_info(const char* path, file_info_t* file_info) {
    char* error_msg = NULL;
    const char* sql = "SELECT * FROM fileMana WHERE path = '%s' AND status = 'active';";
    char formatted_sql[4096];
    snprintf(formatted_sql, sizeof(formatted_sql), sql, path);

    if (sqlite3_exec(db, formatted_sql, callback_get_file, file_info, &error_msg) != SQLITE_OK) {
        error_log(FM_ERR_DB_ERROR, error_msg);
        sqlite3_free(error_msg);
        return FM_ERR_DB_ERROR;
    }
    return FM_SUCCESS;
}

static int callback_list_directory(void* data, int argc, char** argv, char** col_names) {
    file_list_t* list = (file_list_t*)data;
    list->items = realloc(list->items, (list->count + 1) * sizeof(file_info_t));
    
    if (!list->items) {
        error_log(FM_ERR_SYSTEM, "Memory allocation failed");
        return 1;
    }

    file_info_t* current = &list->items[list->count];
    callback_get_file(current, argc, argv, col_names);
    list->count++;
    return 0;
}

int db_list_directory(const char* path, file_list_t* list) {
    char* error_msg = NULL;
    const char* sql = 
        "SELECT * FROM fileMana WHERE parent_id = ("
        "SELECT id FROM fileMana WHERE path = '%s') "
        "AND status = 'active';";
    char formatted_sql[4096];
    
    list->count = 0;
    list->items = NULL;
    
    snprintf(formatted_sql, sizeof(formatted_sql), sql, path);
    
    if (sqlite3_exec(db, formatted_sql, callback_list_directory, list, &error_msg) != SQLITE_OK) {
        error_log(FM_ERR_DB_ERROR, error_msg);
        sqlite3_free(error_msg);
        return FM_ERR_DB_ERROR;
    }
    return FM_SUCCESS;
}