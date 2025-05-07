#include "PWML/pwml.h"
#include "PWML/file_utils.h"
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

static GString* _strip_string(GString* string) {
	char* stripped_str = malloc((string->len + 1) * sizeof(char));
	strcpy(stripped_str, string->str);
	g_strstrip(stripped_str);
	GString* result = g_string_new(stripped_str);
	free(stripped_str);
	return result;
}

static bool _g_string_equals_c_string(GString* v1, const char* v2) {
	return g_string_equal(v1, g_string_new(v2));
}

static char* _g_string_clone_buffer(GString* string) {
	char* buffer = malloc((string->len + 1) * sizeof(char));
	strcpy(buffer, string->str);
	return buffer;
}

typedef enum {
	WEAPONS_STAGE,
	SHIP_STAGE,
	PILOT_STAGE
} _PWML_WeaponsDatStage;

static GHashTable* _pwml_get_weapons(PWML* pwml, const char* weapons_path) {
	const char* full_path = pwml_get_full_path(pwml, weapons_path);
	char* contents;
	GError* error = NULL;
	
	if (!g_file_get_contents(g_build_filename(full_path, "Weapons.dat", NULL), &contents, NULL, &error)) {
		g_printerr("Failed to read %s: %s\n", weapons_path, error->message);
		g_error_free(error);
		free((char*)full_path);
		return NULL;
	}

	GHashTable* weapons = g_hash_table_new_full(g_str_hash, g_str_equal, free, _pwml_weapon_free);

	_PWML_WeaponsDatStage stage = WEAPONS_STAGE;

	char c;
	uint i = 0;
	GString* line = g_string_new(NULL);
	while ((c = contents[i++])) {
		if (c == '\n') {
			GString* stripped = _strip_string(line);
			if (stripped->len == 0) {
				g_string_free(stripped, true);
				g_string_set_size(line, 0);
				continue;
			}

			if (_g_string_equals_c_string(stripped, "Weapons:")) {
				stage = WEAPONS_STAGE;
				g_string_free(stripped, true);
				g_string_set_size(line, 0);
				continue;
			} else if (_g_string_equals_c_string(stripped, "Ship weapons:")) {
				stage = SHIP_STAGE;
				g_string_free(stripped, true);
				g_string_set_size(line, 0);
				continue;
			} else if (_g_string_equals_c_string(stripped, "Pilot weapons:")) {
				stage = PILOT_STAGE;
				g_string_free(stripped, true);
				g_string_set_size(line, 0);
				continue;
			}

			switch (stage) {
				case WEAPONS_STAGE:
				{
					_PWML_Weapon* weapon = malloc(sizeof(_PWML_Weapon));
					weapon->name = _g_string_clone_buffer(stripped);
					weapon->pilot = false;
					weapon->ship = false;
					g_hash_table_insert(weapons, g_strdup(weapon->name), weapon);
					break;
				}
				case SHIP_STAGE:
				{
					if (g_hash_table_contains(weapons, stripped->str)) {
						_PWML_Weapon* weapon = g_hash_table_lookup(weapons, stripped->str);
						weapon->ship = true;
					} else {
						_PWML_Weapon* weapon = malloc(sizeof(_PWML_Weapon));
						weapon->name = _g_string_clone_buffer(stripped);
						weapon->pilot = false;
						weapon->ship = true;
						g_hash_table_insert(weapons, g_strdup(weapon->name), weapon);
					}
					break;
				}
				case PILOT_STAGE:
				{
					if (g_hash_table_contains(weapons, stripped->str)) {
						_PWML_Weapon* weapon = g_hash_table_lookup(weapons, stripped->str);
						weapon->pilot = true;
					} else {
						_PWML_Weapon* weapon = malloc(sizeof(_PWML_Weapon));
						weapon->name = _g_string_clone_buffer(stripped);
						weapon->pilot = true;
						weapon->ship = false;
						g_hash_table_insert(weapons, g_strdup(weapon->name), weapon);
					}
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

	return weapons;
}

static void _pwml_clone_vanilla(PWML* pwml) {
	int check_n = 0;
	GHashTable* weapons = _pwml_get_weapons(pwml, PWML_WEAPONS_FOLDER);
	if (!weapons) {
		g_printerr("Failed to retrieve vanilla weapons\n");
		return;
	}

	const char* vanilla_mod_path = g_build_filename(pwml->working_directory, PWML_MODS_FOLDER, "vanilla", NULL);

	GHashTableIter iterator;
	g_hash_table_iter_init(&iterator, weapons);
	
	gpointer key, value;
	while (g_hash_table_iter_next(&iterator, &key, &value)) {
		_PWML_Weapon* weapon = (_PWML_Weapon*)value;
		const char* weapon_path = g_build_filename(pwml->working_directory, PWML_WEAPONS_FOLDER, weapon->name, NULL);
		if (_is_dir(weapon_path))
			_file_utils_copy_recursive(weapon_path, vanilla_mod_path);
		free((char*)weapon_path);
		_pwml_weapon_free(weapon);
	}

	free((char*)vanilla_mod_path);
}

void pwml_free(PWML* pwml) {
	free((char*)pwml->working_directory);

	for (uint i = 0; i < pwml->n_mods; i++) {
		pwml_mod_free(&pwml->mods[i]);
	}
	free(pwml->mods);
	free(pwml);
}

PWML* pwml_new(const char *working_directory) {
	if (!g_file_test(working_directory, G_FILE_TEST_IS_DIR))
		return NULL;

	PWML* pwml = malloc(sizeof(PWML));

	pwml->working_directory = g_strdup(working_directory);
	pwml->n_mods = 0;
	pwml->mods = NULL;
	
	if (!_pwml_ensure_folder(pwml, PWML_MODS_FOLDER)) {
		pwml_free(pwml);
		return NULL;
	}

	if (!g_file_test(g_build_filename(pwml->working_directory, PWML_ACTIVE_MODS_JSON, NULL), G_FILE_TEST_EXISTS)) {
		_pwml_clone_vanilla(pwml);
	}

	pwml_load_mods(pwml);

	return pwml;
}

static PWML_Mod* _pwml_load_mod(PWML* pwml, const char* path) {
	PWML_Mod* mod = malloc(sizeof(PWML_Mod));
	mod->path = g_strdup(path);
	mod->id = g_path_get_basename(path);
	mod->name = NULL;
	mod->description = NULL;
	
	char* buffer;
	GError *error = NULL;

	const char* metadata_path = g_build_filename(mod->path, PWML_METADATA_JSON_NAME, NULL);
	if (!g_file_test(metadata_path, G_FILE_TEST_EXISTS)) {
		g_printerr("Missing metadata for mod %s\n", mod->id);
		pwml_mod_free(mod);
		free((char*)metadata_path);
		return NULL;
	}

	if (!g_file_get_contents(metadata_path, &buffer, NULL, &error)) {
		g_printerr("Error reading file %s: %s\n", metadata_path, error->message);
		pwml_mod_free(mod);
		g_error_free(error);
		free((char*)metadata_path);
		return NULL;
	}

	struct json_object *parsed_json = json_tokener_parse(buffer);
	free(buffer);

	if (!parsed_json) {
		g_printerr("Failed to parse json for %s\n", metadata_path);
		pwml_mod_free(mod);
		free((char*)metadata_path);
		return NULL;
	}

	struct json_object *name, *description;
	if (!json_object_object_get_ex(parsed_json, "name", &name)) {
		g_printerr("Missing name field in %s\n", metadata_path);
		pwml_mod_free(mod);
		json_object_put(parsed_json);
		free((char*)metadata_path);
		return NULL;
	}
	if (!json_object_object_get_ex(parsed_json, "description", &description)) {
		g_printerr("Missing description field in %s\n", metadata_path);
		pwml_mod_free(mod);
		json_object_put(parsed_json);
		free((char*)metadata_path);
		return NULL;
	}
	
	free((char*)metadata_path);

	mod->name = g_strdup(json_object_get_string(name));
	mod->description = g_strdup(json_object_get_string(description));

	json_object_put(parsed_json);

	return mod;
}

void pwml_load_mods(PWML* pwml) {
	GPtrArray* files = _list_files_in_directory(pwml_get_full_path(pwml, PWML_MODS_FOLDER));
	
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

	g_ptr_array_free(files, true);
}

PWML_Mod* pwml_list_mods(PWML* pwml, uint* n_mods) {
	*n_mods = pwml->n_mods;
	return pwml->mods;
}
