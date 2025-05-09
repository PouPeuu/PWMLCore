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

const char* const PWML_METADATA_JSON = "metadata.json";
const char* const PWML_ACTIVE_MODS_JSON = "active_mods.json";
const char* const PWML_WEAPON_JSON = "weapon.json";

const char* const PWML_MOD_DATA_FOLDER = "data";

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



static const char* __pwml_get_vanilla_mod_data_folder(PWML* pwml) {
	const char* vanilla_mod_path = g_build_filename(pwml->working_directory, PWML_MODS_FOLDER, "vanilla", NULL);
	const char* vanilla_mod_data = g_build_filename(vanilla_mod_path, PWML_MOD_DATA_FOLDER, NULL);
	free((char*)vanilla_mod_path);
	return vanilla_mod_data;
}

static bool __pwml_clone_vanilla_weapons(PWML* pwml) {
	GHashTable* weapons = _pwml_get_weapons(pwml, PWML_WEAPONS_FOLDER);
	if (!weapons) {
		g_printerr("Failed to retrieve vanilla weapons\n");
		return false;
	}

	const char* vanilla_mod_data = __pwml_get_vanilla_mod_data_folder(pwml);
	const char* vanilla_mod_weapons = g_build_filename(vanilla_mod_data, PWML_WEAPONS_FOLDER, NULL);

	if (g_mkdir_with_parents(vanilla_mod_weapons, 0755) == -1) {
		g_print("Failed to make vanilla weapons directory\n");
		return false;
	};

	GPtrArray* files = _list_files_in_directory(pwml_get_full_path(pwml, PWML_WEAPONS_FOLDER));
	for (uint i = 0; i < files->len; i++) {
		const char* path = g_ptr_array_index(files, i);
		if (_is_dir(path)) {
			_PWML_Weapon* weapon;
			const char* name = g_path_get_basename(path);
			if (g_hash_table_contains(weapons, name)) {
				weapon = g_hash_table_lookup(weapons, name);
			} else {
				weapon = malloc(sizeof(_PWML_Weapon));
				weapon->name = g_strdup(name);
				weapon->pilot = false;
				weapon->ship = false;
			}

			_file_utils_copy_recursive(path, vanilla_mod_weapons);
			const char* weapon_json_path = g_build_filename(vanilla_mod_weapons, name, PWML_WEAPON_JSON, NULL);

			json_object* root = json_object_new_object();

			json_object *ship = json_object_new_boolean(weapon->ship);
			json_object_object_add(root, "ship", ship);
			
			json_object *pilot = json_object_new_boolean(weapon->pilot);
			json_object_object_add(root, "pilot", pilot);

			const char* json_str = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);

			GError* error = NULL;
			g_file_set_contents(weapon_json_path, json_str, -1, &error);
			if (error) {
				g_printerr("Failed to write weapon json at %s\nGError: %s\n", weapon_json_path, error->message);
				g_free(error);
			}

			json_object_put(root);

			free((char*)name);
			free((char*)weapon_json_path);
		}

		free((char*)path);
	}

	free((char*)vanilla_mod_data);
	free((char*)vanilla_mod_weapons);

	return true;
}

static bool __pwml_clone_vanilla_levels(PWML* pwml) {
	const char* vanilla_mod_data = __pwml_get_vanilla_mod_data_folder(pwml);
	const char* vanilla_mod_levels = g_build_filename(vanilla_mod_data, PWML_LEVELS_FOLDER, NULL);
	
	if (g_mkdir_with_parents(vanilla_mod_levels, 0755) == -1) {
		g_print("Failed to make vanilla levels directory\n");
		return false;
	};

	GPtrArray* files = _list_files_in_directory(pwml_get_full_path(pwml, PWML_LEVELS_FOLDER));
	for (uint i = 0; i < files->len; i++) {
		const char* path = g_ptr_array_index(files, i);

		const char* basename = g_path_get_basename(path);
		if (g_str_equal(basename, "received")) {
			free((char*)basename);
			continue;
		}
		free((char*)basename);

		_file_utils_copy_recursive(path, vanilla_mod_levels);
	}

	g_ptr_array_free(files, true);

	free((char*)vanilla_mod_data);
	free((char*)vanilla_mod_levels);

	return true;
}

static bool __pwml_clone_vanilla_folder_simple(PWML* pwml, const char* folder) {
	const char* vanilla_mod_data = __pwml_get_vanilla_mod_data_folder(pwml);
	const char* vanilla_mod_folder_path = g_build_filename(vanilla_mod_data, folder, NULL);

	if (g_mkdir_with_parents(vanilla_mod_folder_path, 0755) == -1) {
		g_printerr("Failed to clone %s\n", folder);
		free((char*)vanilla_mod_data);
		free((char*)vanilla_mod_folder_path);
		return false;
	}
	const char* folder_path = pwml_get_full_path(pwml, folder);
	_file_utils_copy_all(folder_path, vanilla_mod_folder_path);

	free((char*)folder_path);
	free((char*)vanilla_mod_data);
	free((char*)vanilla_mod_folder_path);

	return true;
}

static void _pwml_clone_vanilla(PWML* pwml) {
	const char* vanilla_mod_path = g_build_filename(pwml->working_directory, PWML_MODS_FOLDER, "vanilla", NULL);

	if (g_mkdir_with_parents(vanilla_mod_path, 0755) == -1) {
		g_printerr("Failed to make vanilla mod folder at %s\n", vanilla_mod_path);
		return;
	}

	json_object* root = json_object_new_object();

	json_object* name = json_object_new_string("Vanilla");
	json_object_object_add(root, "name", name);

	json_object* description = json_object_new_string("Base Wings 2 by Miika Virpioja et al.");
	json_object_object_add(root, "description", description);

	const char* metadata_path = g_build_filename(vanilla_mod_path, PWML_METADATA_JSON, NULL);

	const char* json_str = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);

	GError* error = NULL;
	g_file_set_contents(metadata_path, json_str, -1, &error);
	if (error) {
		g_printerr("Failed to write vanilla mod metadata json at %s\nGError: %s\n", metadata_path, error->message);
		g_free(error);
	}

	json_object_put(root);
	free((char*)metadata_path);

	if (!__pwml_clone_vanilla_weapons(pwml)) {
		g_printerr("Vanilla weapon cloning failed\n");
		return;
	}

	if (!__pwml_clone_vanilla_levels(pwml)) {
		g_printerr("Vanilla level cloning failed\n");
		return;
	}

	if (!__pwml_clone_vanilla_folder_simple(pwml, PWML_OBJECTS_FOLDER)) {
		g_printerr("Vanilla object cloning failed\n");
		return;
	}

	if (!__pwml_clone_vanilla_folder_simple(pwml, PWML_SOUND_FOLDER)) {
		g_printerr("Vanilla sound cloning failed\n");
		return;
	}

	if (!__pwml_clone_vanilla_folder_simple(pwml, PWML_MUSIC_FOLDER)) {
		g_printerr("Vanilla music cloning failed\n");
		return;
	}
	
	if (!__pwml_clone_vanilla_folder_simple(pwml, PWML_GRAPHICS_FOLDER)) {
		g_printerr("Vanilla graphics cloning failed\n");
		return;
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

	const char* active_mods_json_path = g_build_filename(pwml->working_directory, PWML_ACTIVE_MODS_JSON, NULL);
	if (!g_file_test(active_mods_json_path, G_FILE_TEST_EXISTS)) {
		_pwml_clone_vanilla(pwml);

		json_object* root = json_object_new_object();

		json_object* active = json_object_new_array();
		json_object_array_add(active, json_object_new_string("vanilla"));
		json_object_object_add(root, "active", active);

		const char* json_str = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);

		GError* error = NULL;
		g_file_set_contents(active_mods_json_path, json_str, -1, &error);
		if (error) {
			g_printerr("Failed to write active_mods.json: %s\n", error->message);
			g_free(error);
		}

		json_object_put(root);
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
	mod->active = false;
	
	char* buffer;
	GError* error = NULL;

	const char* metadata_path = g_build_filename(mod->path, PWML_METADATA_JSON, NULL);
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

	json_object* parsed_json = json_tokener_parse(buffer);
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

static GHashTable* _pwml_get_active_mods(PWML* pwml) {
	GHashTable* active_mods = g_hash_table_new(g_str_hash, g_str_equal);

	const char* active_mods_json_path = pwml_get_full_path(pwml, PWML_ACTIVE_MODS_JSON);
	if (g_file_test(active_mods_json_path, G_FILE_TEST_EXISTS)) {
		char* buffer;
		GError* error = NULL;

		if (!g_file_get_contents(active_mods_json_path, &buffer, NULL, &error)){
			g_printerr("Failed to read active_mods.json at %s\n", active_mods_json_path);
			g_error_free(error);
			g_hash_table_destroy(active_mods);
			return NULL;
		}
 
		json_object* root = json_tokener_parse(buffer);
		free(buffer);

		if (!root) {
			g_printerr("Failed to parse active_mods.json at %s\n", active_mods_json_path);
			return NULL;
		}

		json_object* j_active_mods;
		if (!json_object_object_get_ex(root, "active", &j_active_mods) || json_object_get_type(j_active_mods) != json_type_array) {
			json_object_put(root);
			g_printerr("Couldn't get array of active mods from %s\n", active_mods_json_path);
			g_hash_table_destroy(active_mods);
			return NULL;
		}

		int len = json_object_array_length(j_active_mods);
		for (uint i = 0; i < len; i++) {
			json_object* name = json_object_array_get_idx(j_active_mods, i);
			if (json_object_get_type(name) != json_type_string)
				continue;
			
			g_hash_table_add(active_mods, strdup(json_object_get_string(name)));
		}

		json_object_put(root);
	}
	free((char*)active_mods_json_path);

	return active_mods;
}

void pwml_load_mods(PWML* pwml) {
	GHashTable* active_mods = _pwml_get_active_mods(pwml);
	GPtrArray* files = _list_files_in_directory(pwml_get_full_path(pwml, PWML_MODS_FOLDER));
	
	uint real_n = files->len;
	uint mod_error_offset = 0;
	pwml->mods = malloc(files->len * sizeof(PWML_Mod));
	for (uint i = 0; i < files->len; i++) {
		PWML_Mod* mod = _pwml_load_mod(pwml, g_ptr_array_index(files, i));
		if (mod) {
			if (g_hash_table_contains(active_mods, mod->id))
				mod->active = true;
			pwml->mods[i - mod_error_offset] = *mod;

			// Notes for self:
			// The mod is dereferenced (*mod) when being added to the array, so the mods array contains the actual data.
			// Doing free(mod) instead of pwml_mod_free(mod) to avoid destroying the strings
			// Thus, this is fine
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
	if (active_mods)
		g_hash_table_destroy(active_mods);
}

PWML_Mod* pwml_list_mods(PWML* pwml, uint* n_mods) {
	*n_mods = pwml->n_mods;
	return pwml->mods;
}
