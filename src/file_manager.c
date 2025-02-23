// src/file_manager.c
#include "file_manager.h"
#include "common.h"
#include "db_manager.h"
#include "error_handler.h"
#include "json_object.h"
#include "json_tokener.h"
#include "json_types.h"
#include <fcntl.h>
#include <json-c/json.h>
#include <openssl/evp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char root_path[MAX_PATH_LENGTH] = "/home/o/Public";

static int calculate_checksum(const char *file_path, char *checksum) {
  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned char buffer[BUFFER_SIZE];
  FILE *file = fopen(file_path, "rb");
  if (!file)
    return FM_ERR_NOT_FOUND;

  EVP_MD_CTX *ctx = EVP_MD_CTX_new();
  const EVP_MD *md = EVP_sha256();
  if (!ctx) {
    fprintf(stderr, "Error creating context\n");
    return 1;
  }

  if (EVP_DigestInit_ex(ctx, md, NULL) != 1) {
    fprintf(stderr, "Error initializing digest\n");
    EVP_MD_CTX_free(ctx);
    return 1;
  }

  size_t bytes;
  while ((bytes = fread(buffer, 1, BUFFER_SIZE, file)) != 0) {
    if (EVP_DigestUpdate(ctx, buffer, bytes) != 1) {
      fprintf(stderr, "Error updating digest\n");
      EVP_MD_CTX_free(ctx);
      return 1;
    }
  }

  uint32_t hashlen = 0;
  if (EVP_DigestFinal_ex(ctx, hash, &hashlen) != 1) {
    fprintf(stderr, "Error updating digest\n");
    EVP_MD_CTX_free(ctx);
    return 1;
  }
  EVP_MD_CTX_free(ctx);
  fclose(file);

  for (uint32_t i = 0; i < hashlen; i++) {
    sprintf(checksum + (i * 2), "%02x", hash[i]);
  }
  checksum[64] = '\0';
  return FM_SUCCESS;
}

static int get_parent_path(const char *path, char *parent_path) {
  strncpy(parent_path, path, MAX_PATH_LENGTH);
  parent_path[MAX_PATH_LENGTH - 1] = '\0';

  char *last_slash = strrchr(parent_path, '/');
  if (!last_slash)
    return FM_ERR_INVALID_PATH;

  if (last_slash == parent_path) {
    parent_path[1] = '\0';
  } else {
    *last_slash = '\0';
  }
  return FM_SUCCESS;
}

int fm_init(const char *base_path) {
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

int fm_create_file(const char *path, size_t size) {
  char full_path[MAX_PATH_LENGTH];
  snprintf(full_path, MAX_PATH_LENGTH, "%s/%s", root_path, path);

  // Check if file already exists
  if (access(full_path, F_OK) == 0) {
    return FM_ERR_ALREADY_EXISTS;
  }

  // Create new file
  FILE *file = fopen(full_path, "wb");
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

  char *name = strrchr(path, '/');
  name = name ? name + 1 : (char *)path;
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

int fm_create_directory(const char *path) {
  char full_path[MAX_PATH_LENGTH];
  snprintf(full_path, MAX_PATH_LENGTH, "%s/%s", root_path, path);
  // printf("full_path:%s - %s", root_path, path);
  // return 0;

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

  char *name = strrchr(path, '/');
  name = name ? name + 1 : (char *)path;
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

int fm_copy(const char *src, const char *dest) {
  char full_src[MAX_PATH_LENGTH], full_dest[MAX_PATH_LENGTH];
  snprintf(full_src, MAX_PATH_LENGTH, "%s/%s", root_path, src);
  snprintf(full_dest, MAX_PATH_LENGTH, "%s/%s", root_path, dest);

  // Check if source exists and destination doesn't
  if (access(full_src, F_OK) != 0)
    return FM_ERR_NOT_FOUND;
  if (access(full_dest, F_OK) == 0)
    return FM_ERR_ALREADY_EXISTS;

  // Copy file
  FILE *source = fopen(full_src, "rb");
  FILE *destination = fopen(full_dest, "wb");
  if (!source || !destination) {
    if (source)
      fclose(source);
    if (destination)
      fclose(destination);
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
  char *name = strrchr(dest, '/');
  name = name ? name + 1 : (char *)dest;
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

int fm_rename(const char *old_path, const char *new_path) {
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

  char *name = strrchr(new_path, '/');
  name = name ? name + 1 : (char *)new_path;
  strncpy(file_info.name, name, MAX_NAME_LENGTH - 1);
  strncpy(file_info.path, new_path, MAX_PATH_LENGTH - 1);

  if (strcmp(file_info.type, FILE_TYPE_FILE) == 0) {
    calculate_checksum(full_new, file_info.checksum);
  }

  return db_update_file(&file_info);
}

int fm_delete(const char *path) {
  char full_path[MAX_PATH_LENGTH];
  snprintf(full_path, MAX_PATH_LENGTH, "%s/%s", root_path, path);

  struct stat st;
  if (stat(full_path, &st) != 0) {
    return FM_ERR_NOT_FOUND;
  }

  if (S_ISDIR(st.st_mode)) {
    DIR *dir = opendir(full_path);
    if (!dir)
      return FM_ERR_SYSTEM;

    struct dirent *entry;
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

int fm_get_file_info(const char *path, file_info_t *info) {
  return db_get_file_info(path, info);
}

int fm_list_directory(const char *path, file_list_t *list) {
  return db_list_directory(path, list);
}

char *file_string(const char *json_file) {
  FILE *f = fopen(json_file, "r");
  fseek(f, 0, SEEK_END);
  long file_size = ftell(f);
  fseek(f, 0, SEEK_SET);

  char *json_str = malloc(file_size + 1);
  fread(json_str, file_size, 1, f);
  json_str[file_size] = '\0';

  fclose(f);

  return json_str;
}

const char *fm_get_base_file_name(const char *path) {
  const char *lastslash = strrchr(path, '/');
  if (!lastslash)
    return path;
  return lastslash + 1;
}

int fm_do_copy_inner(const char *from, char *to) {
  struct stat from_stat, to_stat;
  if (stat(from, &from_stat) != 0) {
    printf("file not exists\n");
    return -1;
  }
  if (stat(to, &to_stat) == 0) {
    if (S_ISDIR(to_stat.st_mode)) {
      strcat(to, "/");
      strcat(to, fm_get_base_file_name(from));
    } else {
      printf("can't copy to existed file\n");
      return -1;
    }
  }

  char cmd[2048];
  sprintf(cmd, "cp -r %s %s", from, to);
  printf("cmd: %s, <%s><%s>\n", cmd, from, to);
  system(cmd);

  return 0;
}

int fm_do_copy(const char *from, const char *to) {
  char from_full[1024];
  char to_full[1024];
  sprintf(from_full, "%s/%s", root_path, from);
  sprintf(to_full, "%s/%s", root_path, to);

  int res = fm_do_copy_inner(from_full, to_full);
  printf("copy from %s to %s, result:%d\n", from_full, to_full, res);
  return res;
}

int fm_batch_do_json_objet(struct json_object *json_obj) {
  struct json_object *from_arr, *from_item_obj, *to_obj;
  if (json_object_object_get_ex(json_obj, "from", &from_arr) &&
      json_object_object_get_ex(json_obj, "to", &to_obj) &&
      json_object_get_type(from_arr) == json_type_array &&
      json_object_get_type(to_obj) == json_type_string) {
    int from_item_num = json_object_array_length(from_arr);
    for (int i = 0; i < from_item_num; i++) {
      // parse string[]
      from_item_obj = json_object_array_get_idx(from_arr, i);
      if (json_object_get_type(from_item_obj) != json_type_string) {
        continue;
      }
      fm_do_copy(json_object_get_string(from_item_obj),
                 json_object_get_string(to_obj));
    }
  }
  return 0;
}

int fm_batch_do_json(const char *json_file) {
  char *json_str = file_string(json_file);
  if (!json_str)
    return FM_ERR_SYSTEM;
  printf("json_str:%s\n", json_str);
  struct json_object *parsed_json;
  parsed_json = json_tokener_parse(json_str);
  free(json_str);
  if (!parsed_json)
    return FM_ERR_SYSTEM;
  printf("parsed json:%p\n", parsed_json);

  return fm_batch_do_json_objet(parsed_json);
}

void fm_cleanup(void) { db_close(); }
