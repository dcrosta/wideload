#ifndef WIDELOAD_URLFILE_H
#define WIDELOAD_URLFILE_H

#include "loader.h"


/**
 * Parse a URL file line into a request
 */
request parse_line(char* line, const char* url_filename, int lineno);

/**
 * Parse a URL file into a requests
 */
requests parse_urls(const char* url_filename);

#endif
