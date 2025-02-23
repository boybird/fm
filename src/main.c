#include "common.h"
#include "file_manager.h"
#include "error_handler.h"

static void print_usage() {
    printf("Usage: fm <command> [options]\n\n");
    printf("Commands:\n");
    printf("  init <root_path>         Initialize file management system\n");
    printf("  create <path> [size]     Create a new file with optional size\n");
    printf("  mkdir <path>             Create a new directory\n");
    printf("  copy <src> <dest>        Copy a file or directory\n");
    printf("  rename <old> <new>       Rename/move a file or directory\n");
    printf("  delete <path>            Delete a file or directory\n");
    printf("  list <path>              List contents of a directory\n");
    printf("  info <path>              Show file/directory information\n");
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    error_init();

    const char* command = argv[1];
    int result = FM_SUCCESS;

    if (strcmp(command, "init") == 0) {
        if (argc != 3) {
            printf("Error: init requires root path\n");
            return 1;
        }
        result = fm_init(argv[2]);
    }
    else if (strcmp(command, "create") == 0) {
        if (argc < 3) {
            printf("Error: create requires file path\n");
            return 1;
        }
        size_t size = (argc > 3) ? atoll(argv[3]) : 0;
        result = fm_create_file(argv[2], size);
    }
    else if (strcmp(command, "mkdir") == 0) {
        if (argc != 3) {
            printf("Error: mkdir requires directory path\n");
            return 1;
        }
        result = fm_create_directory(argv[2]);
    }
    else if (strcmp(command, "copy") == 0) {
        if (argc != 4) {
            printf("Error: copy requires source and destination paths\n");
            return 1;
        }
        result = fm_copy(argv[2], argv[3]);
    }
    else if (strcmp(command, "rename") == 0) {
        if (argc != 4) {
            printf("Error: rename requires old and new paths\n");
            return 1;
        }
        result = fm_rename(argv[2], argv[3]);
    }
    else if (strcmp(command, "delete") == 0) {
        if (argc != 3) {
            printf("Error: delete requires path\n");
            return 1;
        }
        result = fm_delete(argv[2]);
    }
    else if (strcmp(command, "list") == 0) {
        if (argc != 3) {
            printf("Error: list requires directory path\n");
            return 1;
        }
        file_list_t list = {0};
        result = fm_list_directory(argv[2], &list);
        if (result == FM_SUCCESS) {
            printf("Contents of %s:\n", argv[2]);
            for (size_t i = 0; i < list.count; i++) {
                printf("%s [%s] %zu bytes\n", 
                       list.items[i].name,
                       list.items[i].type,
                       list.items[i].size);
            }
            free(list.items);
        }
    }
    else if (strcmp(command, "info") == 0) {
        if (argc != 3) {
            printf("Error: info requires path\n");
            return 1;
        }
        file_info_t info;
        result = fm_get_file_info(argv[2], &info);
        if (result == FM_SUCCESS) {
            printf("Name: %s\n", info.name);
            printf("Path: %s\n", info.path);
            printf("Type: %s\n", info.type);
            printf("Size: %zu bytes\n", info.size);
            if (strcmp(info.type, FILE_TYPE_FILE) == 0) {
                printf("Checksum: %s\n", info.checksum);
            }
        }
    }
    else {
        printf("Unknown command: %s\n", command);
        print_usage();
        return 1;
    }

    if (result != FM_SUCCESS) {
        printf("Error: %s\n", error_get_last());
        return 1;
    }

    return 0;
}
