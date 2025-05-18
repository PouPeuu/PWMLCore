#include "PWML/file_utils.h"
#include "glib-object.h"
#include <glib.h>
#include <gio/gio.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

const GFileCopyFlags FLAGS = G_FILE_COPY_OVERWRITE | G_FILE_COPY_ALL_METADATA | G_FILE_COPY_NOFOLLOW_SYMLINKS;

bool _file_utils_is_dir(const char* path) {
	return g_file_test(path, G_FILE_TEST_IS_DIR);
}

void _file_utils_copy_file(GFile* source, GFile* destination) {
	GError* error = NULL;
	const char* destination_path = g_file_get_path(destination);
	g_file_copy(source, destination, FLAGS, NULL, NULL, NULL, &error);
	if (error) {
		g_printerr("Failed to copy file %s to %s: %s\n", g_file_get_path(source), g_file_get_path(destination), error->message);
		g_error_free(error);
	}
	free((char*)destination_path);
}

void _file_utils_copy_file_with_path(const char* source, const char* destination) {
	GFile* source_gfile = g_file_new_for_path(source);
	GFile* destination_gfile = g_file_new_for_path(destination);
	_file_utils_copy_file(source_gfile, destination_gfile);
	g_object_unref(source_gfile);
	g_object_unref(destination_gfile);
}

GPtrArray* _file_utils_list_files_in_directory(const char* path) {
	GDir* dir = g_dir_open(path, 0, NULL);
	if (!dir) {
		g_printerr("Failed to open directory %s\n", path);
		return NULL;
	}

	GPtrArray* files = g_ptr_array_new_with_free_func(g_free);
	const char* entry;
	while ((entry = g_dir_read_name(dir))) {
		char* full_path = g_build_filename(path, entry, NULL);
		g_ptr_array_add(files, full_path);
	}

	g_dir_close(dir);
	return files;
}

void _file_utils_copy_recursive(const char* source_path, const char* destination_path) {
	const char* base = g_path_get_basename(source_path);

	if (_file_utils_is_dir(source_path)) {
		const char* temp = g_build_filename(destination_path, base, NULL);
		g_mkdir_with_parents(temp, 0755);
		free((char*)temp);
		
		GQueue* queued_files = g_queue_new();
		g_queue_push_head(queued_files, strdup(source_path));

		const char* current_path;
		while ((current_path = g_queue_pop_head(queued_files))) {
			uint source_path_len = strlen(source_path) - strlen(base);
			uint diff = strlen(current_path) - source_path_len;

			char relative_path[diff+1];
			strncpy(relative_path, current_path + source_path_len, diff);
			relative_path[diff] = '\0';
			
			const char* file_destination = g_build_filename(destination_path, relative_path, NULL);

			if (_file_utils_is_dir(current_path)) {
				g_mkdir_with_parents(file_destination, 0755);
				GPtrArray* files = _file_utils_list_files_in_directory(current_path);
				for (uint i = 0; i < files->len; i++) {
					char* file = g_ptr_array_index(files, i);
					g_queue_push_head(queued_files, file);
				}
				// FIXME: No free? If you add free, then test because this place had problems before
				// Nevermind it pushes it to queued_files which does free it
				g_ptr_array_free(files, false);
			} else {
				_file_utils_copy_file_with_path(current_path, file_destination);
			}

			free((char*)file_destination);
			free((char*)current_path);
		}
		g_queue_free(queued_files);
	} else {
		const char* destination_file_path = g_build_filename(destination_path, base, NULL);
		_file_utils_copy_file_with_path(source_path, destination_file_path);
		free((char*)destination_file_path);
	}

	free((char*)base);
}

void _file_utils_copy_all(const char *from, const char *to) {
	GPtrArray* files = _file_utils_list_files_in_directory(from);
	for (uint i = 0; i < files->len; i++) {
		const char* path = g_ptr_array_index(files, i);
		_file_utils_copy_recursive(path, to);
	}
}

void _file_utils_copy_all_except(const char* from, const char* to, const char* ignore) {
	GPtrArray* files = _file_utils_list_files_in_directory(from);
	for (uint i = 0; i < files->len; i++) {
		const char* path = g_ptr_array_index(files, i);
		const char* basename = g_path_get_basename(path);
		if (strcmp(path, ignore) == 0 || strcmp(basename, ignore) == 0)
			continue;
		_file_utils_copy_recursive(path, to);
		free((char*)path);
		free((char*)basename);
	}
}

/*static void __debug_print_file_flags(const char* path) {
	g_print("-- BEGIN FLAGS FOR FILE \"%s\" --\n", path);
	if (g_file_test(path, G_FILE_TEST_EXISTS)) {
		g_print("exists\n");
	} else {
		g_print("doesn't exist\n");
	}
	if (g_file_test(path, G_FILE_TEST_IS_REGULAR)) {
		g_print("regular\n");
	} else {
		g_print("not regular\n");
	}
	if (g_file_test(path, G_FILE_TEST_IS_DIR)) {
		g_print("directory\n");
	} else {
		g_print("not a directory\n");
	}
	if (g_file_test(path, G_FILE_TEST_IS_EXECUTABLE)) {
		g_print("executable\n");
	} else {
		g_print("not an executable\n");
	}
	if (g_file_test(path, G_FILE_TEST_IS_SYMLINK)) {
		g_print("symlink\n");
	} else {
		g_print("not a symlink\n");
	}
}*/

void _file_utils_delete_recursive(const char* path) {
	if (g_file_test(path, G_FILE_TEST_IS_REGULAR)) {
		remove(path);
		return;
	}
	if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
		g_printerr("Cannot recursively delete %s; No such file or directory.\n", path);
		return;
	}

	GPtrArray* directory_hitlist = g_ptr_array_new();
	g_ptr_array_add(directory_hitlist, strdup(path));

	GQueue* queue = g_queue_new();
	g_queue_push_head(queue, strdup(path));

	const char* current_path;
	while ((current_path = g_queue_pop_head(queue))) {
		if (_file_utils_is_dir(current_path)) {
			GPtrArray* files = _file_utils_list_files_in_directory(current_path);
			for (uint i = 0; i < files->len; i++) {
				const char* file = g_ptr_array_index(files, i);
				if (_file_utils_is_dir(file)) {
					g_queue_push_head(queue, strdup(file));
					g_ptr_array_add(directory_hitlist, strdup(file));
				} else {
					remove(file);
				}
				free((char*)file);
			}
		}
	}

	for (int i = directory_hitlist->len - 1; i >= 0; i--) {
		rmdir(g_ptr_array_index(directory_hitlist, i));
	}

	g_ptr_array_free(directory_hitlist, true);
	g_queue_free(queue);
}

void _file_utils_delete_all(const char* path) {
	if (!g_file_test(path, G_FILE_TEST_IS_DIR)) {
		g_printerr("Cannot delete all from %s; No such directory exists.\n", path);
		return;
	}

	GPtrArray* files = _file_utils_list_files_in_directory(path);
	for (uint i = 0; i < files->len; i++) {
		const char* file = g_ptr_array_index(files, i);
		_file_utils_delete_recursive(file);
	}
	g_ptr_array_free(files, true);
}
