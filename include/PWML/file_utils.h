#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include "PWML/pwml.h"
#include <glib.h>

void _file_utils_copy_file_with_path(const char* source, const char* destination);
void _file_utils_copy_recursive(const char* source_path, const char* destination_path);
void _file_utils_copy_all(const char* from, const char* to);
const char* pwml_get_full_path(PWML* pwml, const char* path);
GPtrArray* _list_files_in_directory(const char* path);
bool _is_dir(const char* path);

#endif
