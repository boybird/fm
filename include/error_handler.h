// include/error_handler.h
#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include "common.h"

// Initialize error handling system
void error_init(void);

// Log an error with details
void error_log(int error_code, const char* message);

// Get last error message
const char* error_get_last(void);

// Clear last error
void error_clear(void);

#endif // ERROR_HANDLER_H