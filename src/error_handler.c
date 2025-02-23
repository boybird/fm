// src/error_handler.c
#include "error_handler.h"

static char last_error[1024] = {0};
static int last_error_code = FM_SUCCESS;

void error_init(void) {
    error_clear();
}

void error_log(int error_code, const char* message) {
    last_error_code = error_code;
    strncpy(last_error, message, sizeof(last_error) - 1);
    last_error[sizeof(last_error) - 1] = '\0';
    fprintf(stderr, "Error %d: %s\n", error_code, message);
}

const char* error_get_last(void) {
    return last_error;
}

void error_clear(void) {
    last_error[0] = '\0';
    last_error_code = FM_SUCCESS;
}