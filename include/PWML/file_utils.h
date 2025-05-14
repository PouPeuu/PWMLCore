#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <glib.h>
#include <stdbool.h>

void _file_utils_copy_file_with_path(const char* source, const char* destination);
void _file_utils_copy_recursive(const char* source_path, const char* destination_path);
void _file_utils_copy_all(const char* from, const char* to);
void _file_utils_copy_all_except(const char* from, const char* to, const char* ignore);
void _file_utils_delete_recursive(const char* path);
void _file_utils_delete_all(const char* path);
bool _file_utils_is_dir(const char* path);
GPtrArray* _file_utils_list_files_in_directory(const char* path);

#endif
