#include "PWML/pwml.h"
#include "PWML/mod.h"
#include "PWML/weapon.h"
#include <glib.h>
#include <json-c/json_object.h>
#include <json-c/json_tokener.h>
#include <json-c/json_types.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <json-c/json.h>

const char* const PWML_MODS_FOLDER = "mods";

const char* const PWML_WEAPONS_FOLDER = "weapons";
const char* const PWML_SOUND_FOLDER = "sound";
const char* const PWML_OBJECTS_FOLDER = "objects";
const char* const PWML_MUSIC_FOLDER = "music";
const char* const PWML_LEVELS_FOLDER = "levels";
const char* const PWML_GRAPHICS_FOLDER = "graphics";

const char* const PWML_METADATA_JSON_NAME = "metadata.json";
const char* const PWML_ACTIVE_MODS_JSON = "active_mods.json";

typedef struct PWML {
	const char* working_directory;
	uint n_mods;
	PWML_Mod* mods;
} PWML;

static bool _pwml_ensure_folder(PWML* pwml, const char* path) {
	if (g_mkdir_with_parents(g_build_filename(pwml->working_directory, path, NULL), 0755) == -1) {
		g_printerr("Failed to create folder %s\n", path);
		return false;
	}
	return true;
}

static GDir* _pwml_open_folder(PWML* pwml, const char* path) {
	return g_dir_open(pwml_get_full_path(pwml, path), 0, NULL);
}

static GPtrArray* _pwml_list_files_in_directory(PWML* pwml, const char* relative_path) {
	const char* path = pwml_get_full_path(pwml, relative_path);
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

static GString* _strip_string(GString* string) {
	char* stripped_str = malloc((string->len + 1) * sizeof(char));
	strcpy(stripped_str, string->str);
	g_strstrip(stripped_str);
	return g_string_new(stripped_str);
}

static bool _g_string_equals_c_string(GString* v1, const char* v2) {
	return g_string_equal(v1, g_string_new(v2));
}

typedef enum {
	WEAPONS_STAGE,
	SHIP_STAGE,
	PILOT_STAGE
} _PWML_WeaponsDatStage;

static void _pwml_get_weapons(PWML* pwml, const char* weapons_path) {
	const char* full_path = pwml_get_full_path(pwml, weapons_path);
	char* contents;
	GError* error = NULL;
	
	if (!g_file_get_contents(g_build_filename(full_path, "Weapons.dat", NULL), &contents, NULL, &error)) {
		g_printerr("Failed to read %s: %s\n", weapons_path, error->message);
		g_error_free(error);
		return;
	}

	GHashTable* table = g_hash_table_new_full(g_str_hash, g_str_equal, free, _pwml_weapon_free);

	_PWML_WeaponsDatStage stage = WEAPONS_STAGE;

	char c;
	uint i = 0;
	GString* line = g_string_new(NULL);
	while ((c = contents[i++])) {
		if (c == '\n') {
			GString* stripped = _strip_string(line);
			if (stripped->len == 0) {
				g_string_free(stripped, true);
				continue;
			}

			if (_g_string_equals_c_string(stripped, "Weapons:")) {
				stage = WEAPONS_STAGE;
				g_string_free(stripped, true);
				continue;
			} else if (_g_string_equals_c_string(stripped, "Ship weapons:")) {
				stage = SHIP_STAGE;
				g_string_free(stripped, true);
				continue;
			} else if (_g_string_equals_c_string(stripped, "Pilot weapons:")) {
				stage = PILOT_STAGE;
				g_string_free(stripped, true);
				continue;
			}

			switch (stage) {
				case WEAPONS_STAGE:
				{
					if (!g_file_test(g_build_filename(full_path, stripped->str, NULL), G_FILE_TEST_IS_DIR)) {
						g_printerr("Weapons.dat entry %s has no actual files\n", stripped->str);
						break;
					}

					_PWML_Weapon* weapon = malloc(sizeof(_PWML_Weapon));
					char* name = malloc((stripped->len + 1) * sizeof(char));
					strcpy(name, stripped->str);
					weapon->name = name;
					weapon->is_pilot = false;
					weapon->is_ship = false;

					g_hash_table_insert(table, name, weapon);
					break;
				}
				case SHIP_STAGE:
				{
					if (!g_hash_table_contains(table, stripped->str)) {
						g_printerr("Failed to make ship weapon %s; No such weapon exists (Did you forget to add it to the Weapons section first?)\n", stripped->str);
						break;
					}

					_PWML_Weapon* weapon = g_hash_table_lookup(table, stripped->str);
					weapon->is_ship = true;
					g_print("Made weapon %s into a ship weapon\n", weapon->name);
					break;
				}
				case PILOT_STAGE:
				{
					if (!g_hash_table_contains(table, stripped->str)) {
						g_printerr("Failed to make pilot weapon %s; No such weapon exists (Did you forget to add it to the Weapons section first?)\n", stripped->str);
						break;
					}

					_PWML_Weapon* weapon = g_hash_table_lookup(table, stripped->str);
					weapon->is_pilot = true;
					g_print("Made weapon %s into a pilot weapon\n", weapon->name);
					break;
				}
				default:
					break;
			}

			g_string_free(stripped, true);
			g_string_set_size(line, 0);
		} else {
			g_string_append_c(line, c);
		}
	}

	g_string_free(line, true);
	g_free(contents);
}

static void _pwml_clone_vanilla(PWML* pwml) {
	_pwml_get_weapons(pwml, PWML_WEAPONS_FOLDER);
}

PWML* pwml_new(const char *working_directory) {
	if (!g_file_test(working_directory, G_FILE_TEST_IS_DIR))
		return NULL;

	PWML* pwml = malloc(sizeof(PWML));
	char* temp = malloc(strlen(working_directory) * sizeof(char));
	strcpy(temp, working_directory);

	pwml->working_directory = temp;
	pwml->n_mods = 0;
	pwml->mods = NULL;
	
	if (!_pwml_ensure_folder(pwml, PWML_MODS_FOLDER))
		return NULL;

	if (!g_file_test(g_build_filename(pwml->working_directory, PWML_ACTIVE_MODS_JSON, NULL), G_FILE_TEST_EXISTS)) {
		_pwml_clone_vanilla(pwml);
	}

	pwml_load_mods(pwml);

	return pwml;
}

const char* pwml_get_full_path(PWML* pwml, const char* path) {
	return g_build_filename(pwml->working_directory, path, NULL);
}

static PWML_Mod* _pwml_load_mod(PWML* pwml, const char* path) {
	PWML_Mod* mod = malloc(sizeof(PWML_Mod));
	mod->path = path;
	mod->id = g_path_get_basename(path);
	
	char* buffer;
	GError *error = NULL;

	const char* metadata_path = g_build_filename(mod->path, PWML_METADATA_JSON_NAME, NULL);
	if (!g_file_test(metadata_path, G_FILE_TEST_EXISTS)) {
		g_printerr("Missing metadata for mod %s\n", mod->id);
		return NULL;
	}

	if (!g_file_get_contents(metadata_path, &buffer, NULL, &error)) {
		g_printerr("Error reading file %s: %s\n", metadata_path, error->message);
		g_error_free(error);
		return NULL;
	}

	struct json_object *parsed_json = json_tokener_parse(buffer);
	free(buffer);

	if (!parsed_json) {
		g_printerr("Failed to parse json for %s\n", metadata_path);
		return NULL;
	}

	struct json_object *name, *description;
	if (!json_object_object_get_ex(parsed_json, "name", &name)) {
		g_printerr("Missing name field in %s\n", metadata_path);
		return NULL;
	}
	if (!json_object_object_get_ex(parsed_json, "description", &description)) {
		g_printerr("Missing description field in %s\n", metadata_path);
		return NULL;
	}

	mod->name = json_object_get_string(name);
	mod->description = json_object_get_string(description);

	return mod;
}

void pwml_load_mods(PWML* pwml) {
	GPtrArray* files = _pwml_list_files_in_directory(pwml, PWML_MODS_FOLDER);
	
	uint real_n = files->len;
	uint mod_error_offset = 0;
	pwml->mods = malloc(files->len * sizeof(PWML_Mod));
	for (uint i = 0; i < files->len; i++) {
		PWML_Mod* mod = _pwml_load_mod(pwml, g_ptr_array_index(files, i));
		if (mod) {
			pwml->mods[i - mod_error_offset] = *mod;
			free(mod);
		} else {
			real_n--;
			mod_error_offset++;
		}
	}

	pwml->n_mods = real_n;

	if (real_n != files->len) {
		pwml->mods = realloc(pwml->mods, real_n * sizeof(PWML_Mod));
	}
}

PWML_Mod* pwml_list_mods(PWML* pwml, uint* n_mods) {
	*n_mods = pwml->n_mods;
	return pwml->mods;
}
