#ifndef XML_UTILS_H
#define XML_UTILS_H

#include <glib.h>
#include <stdbool.h>

bool _xml_utils_combine_files(const char* path_a, const char* path_b, const char* destination_path);
bool _xml_utils_combine_all_files(GPtrArray* files, const char* destination_path);

#endif
