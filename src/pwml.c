#include "PWML/pwml.h"
#include "PWML/file_utils.h"
#include "PWML/mod.h"
#include "PWML/weapon.h"
#include "PWML/xml_utils.h"
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
const char* const PWML_BIN_FOLDER = "bin";
const char* const PWML_WEAPONS_DAT = "Weapons.dat";

const char* const PWML_METADATA_JSON = "metadata.json";
const char* const PWML_MOD_DESCRIPTION_FILE = "description.pango";
const char* const PWML_ACTIVE_MODS_JSON = "active_mods.json";
const char* const PWML_WEAPON_JSON = "weapon.json";
const char* const PWML_BUILTIN_WEAPONS_JSON = "builtin_weapons.json";

const char* const PWML_MENU_MUSIC_TXT = "menu_music.txt";
const char* const PWML_GRAPHICS_XML = "Graphics.xml";
const char* const PWML_SOUNDS_XML = "Sounds.xml";

const char* const PWML_WINGS_EXECUTABLE = "Wings.exe";

const char* const PWML_MOD_DATA_FOLDER = "data";

static bool _pwml_ensure_folder(PWML* pwml, const char* path) {
	if (g_mkdir_with_parents(g_build_filename(pwml->working_directory, path, NULL), 0755) == -1) {
		g_printerr("Failed to create folder %s\n", path);
		return false;
	}
	return true;
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



static GHashTable* _pwml_parse_weapons_dat(PWML* pwml, const char* weapons_path) {
	char* contents;
	GError* error = NULL;
	
	const char* weapons_dat_path = g_build_filename(pwml->weapons_path, PWML_WEAPONS_DAT, NULL);
	if (!g_file_get_contents(weapons_dat_path, &contents, NULL, &error)) {
		g_printerr("Failed to read %s: %s\n", weapons_path, error->message);
		g_error_free(error);
		return NULL;
	}
	free((char*)weapons_dat_path);

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
					weapon->has_built_in_files = true;
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
						weapon->has_built_in_files = true;
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
						weapon->has_built_in_files = true;
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
	const char* vanilla_mod_path = g_build_filename(pwml->mods_path, "vanilla", NULL);
	const char* vanilla_mod_data = g_build_filename(vanilla_mod_path, PWML_MOD_DATA_FOLDER, NULL);
	free((char*)vanilla_mod_path);
	return vanilla_mod_data;
}

static bool __pwml_clone_vanilla_weapons(PWML* pwml) {
	GHashTable* weapons = _pwml_parse_weapons_dat(pwml, PWML_WEAPONS_FOLDER);
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

	GPtrArray* files = _file_utils_list_files_in_directory(pwml->weapons_path);
	for (uint i = 0; i < files->len; i++) {
		const char* path = g_ptr_array_index(files, i);
		if (_file_utils_is_dir(path)) {
			_PWML_Weapon* weapon;
			const char* name = g_path_get_basename(path);
			if (g_hash_table_contains(weapons, name)) {
				weapon = g_hash_table_lookup(weapons, name);
				weapon->has_built_in_files = false;
			} else {
				weapon = malloc(sizeof(_PWML_Weapon));
				weapon->name = g_strdup(name);
				weapon->pilot = false;
				weapon->ship = false;
				weapon->has_built_in_files = false;
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
	}

	json_object* root = json_object_new_object();
	json_object* j_weapons = json_object_new_array();

	GHashTableIter iter;
	g_hash_table_iter_init(&iter, weapons);

	_PWML_Weapon* weapon;
	while (g_hash_table_iter_next(&iter, NULL, (void**)&weapon)) {
		if (weapon->has_built_in_files) {
			json_object* j_weapon = json_object_new_object();

			json_object* name = json_object_new_string(weapon->name);
			json_object_object_add(j_weapon, "name", name);
			json_object* ship = json_object_new_boolean(weapon->ship);
			json_object_object_add(j_weapon, "ship", ship);
			json_object* pilot = json_object_new_boolean(weapon->pilot);
			json_object_object_add(j_weapon, "pilot", pilot);

			json_object_array_add(j_weapons, j_weapon);
		}
	}

	json_object_object_add(root, "weapons", j_weapons);

	const char* built_in_weapons_json_path = g_build_filename(pwml->mods_path, "vanilla", PWML_MOD_DATA_FOLDER, PWML_WEAPONS_FOLDER, PWML_BUILTIN_WEAPONS_JSON, NULL);
	const char* json_str = json_object_to_json_string(root);

	GError* error = NULL;
	g_file_set_contents(built_in_weapons_json_path, json_str, -1, &error);
	if (error) {
		g_printerr("Failed to write json file %s\n", built_in_weapons_json_path);
		g_error_free(error);
	}

	json_object_put(root);
	free((char*)built_in_weapons_json_path);
	free((char*)vanilla_mod_data);
	free((char*)vanilla_mod_weapons);
	g_hash_table_destroy(weapons);
	g_ptr_array_free(files, true);

	return true;
}

static bool __pwml_clone_vanilla_levels(PWML* pwml) {
	const char* vanilla_mod_data = __pwml_get_vanilla_mod_data_folder(pwml);
	const char* vanilla_mod_levels = g_build_filename(vanilla_mod_data, PWML_LEVELS_FOLDER, NULL);
	
	if (g_mkdir_with_parents(vanilla_mod_levels, 0755) == -1) {
		g_print("Failed to make vanilla levels directory\n");
		return false;
	};

	GPtrArray* files = _file_utils_list_files_in_directory(pwml->levels_path);
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
	const char* folder_path = g_build_filename(pwml->working_directory, folder, NULL);
	_file_utils_copy_all(folder_path, vanilla_mod_folder_path);

	free((char*)folder_path);
	free((char*)vanilla_mod_data);
	free((char*)vanilla_mod_folder_path);

	return true;
}

static void _pwml_clone_vanilla(PWML* pwml) {
	const char* vanilla_mod_path = g_build_filename(pwml->mods_path, "vanilla", NULL);

	if (g_mkdir_with_parents(vanilla_mod_path, 0755) == -1) {
		g_printerr("Failed to make vanilla mod folder at %s\n", vanilla_mod_path);
		return;
	}

	json_object* root = json_object_new_object();

	json_object* name = json_object_new_string("Vanilla");
	json_object_object_add(root, "name", name);

	json_object* description = json_object_new_string("Base Wings 2 by Miika Virpioja et al.");
	json_object_object_add(root, "short_description", description);

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
	g_hash_table_destroy(pwml->mods);
	g_ptr_array_free(pwml->weapons, true);

	g_ptr_array_free(pwml->menu_music_paths, true);
	g_ptr_array_free(pwml->graphics_xml_paths, true);
	g_ptr_array_free(pwml->sounds_xml_paths, true);

	free((char*)pwml->graphics_path);
	free((char*)pwml->levels_path);
	free((char*)pwml->mods_path);
	free((char*)pwml->music_path);
	free((char*)pwml->objects_path);
	free((char*)pwml->sound_path);
	free((char*)pwml->weapons_path);
	free((char*)pwml->bin_path);
	free((char*)pwml->exectuable_path);
	free(pwml);
}

PWML* pwml_new(const char* working_directory) {
	if (!g_file_test(working_directory, G_FILE_TEST_IS_DIR))
		return NULL;

	PWML* pwml = malloc(sizeof(PWML));

	pwml->working_directory = g_strdup(working_directory);
	pwml->mods = g_hash_table_new(g_str_hash, g_str_equal);
	pwml->weapons = g_ptr_array_new_with_free_func(_pwml_weapon_free);

	pwml->menu_music_paths = g_ptr_array_new();
	pwml->graphics_xml_paths = g_ptr_array_new();
	pwml->sounds_xml_paths = g_ptr_array_new();

	pwml->bin_path = g_build_filename(pwml->working_directory, PWML_BIN_FOLDER, NULL);
	pwml->graphics_path = g_build_filename(pwml->working_directory, PWML_GRAPHICS_FOLDER, NULL);
	pwml->levels_path = g_build_filename(pwml->working_directory, PWML_LEVELS_FOLDER, NULL);
	pwml->mods_path = g_build_filename(pwml->working_directory, PWML_MODS_FOLDER, NULL);
	pwml->music_path = g_build_filename(pwml->working_directory, PWML_MUSIC_FOLDER, NULL);
	pwml->objects_path = g_build_filename(pwml->working_directory, PWML_OBJECTS_FOLDER, NULL);
	pwml->sound_path = g_build_filename(pwml->working_directory, PWML_SOUND_FOLDER, NULL);
	pwml->weapons_path = g_build_filename(pwml->working_directory, PWML_WEAPONS_FOLDER, NULL);
	pwml->exectuable_path = g_build_filename(pwml->bin_path, PWML_WINGS_EXECUTABLE, NULL);
	
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



static PWML_Mod* _pwml_load_mod(const char* path) {
	PWML_Mod* mod = malloc(sizeof(PWML_Mod));
	mod->path = g_strdup(path);
	mod->id = g_path_get_basename(path);
	mod->name = NULL;
	mod->short_description = NULL;
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
	if (!json_object_object_get_ex(parsed_json, "short_description", &description)) {
		g_printerr("Missing short_description field in %s\n", metadata_path);
		pwml_mod_free(mod);
		json_object_put(parsed_json);
		free((char*)metadata_path);
		return NULL;
	}
	
	free((char*)metadata_path);

	mod->name = g_strdup(json_object_get_string(name));
	mod->short_description = g_strdup(json_object_get_string(description));

	json_object_put(parsed_json);

	return mod;
}

static GHashTable* _pwml_get_active_mods(PWML* pwml) {
	GHashTable* active_mods = g_hash_table_new(g_str_hash, g_str_equal);

	const char* active_mods_json_path = g_build_filename(pwml->working_directory, PWML_ACTIVE_MODS_JSON, NULL);
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

		uint len = json_object_array_length(j_active_mods);
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
	GPtrArray* files = _file_utils_list_files_in_directory(pwml->mods_path);
	
	for (uint i = 0; i < files->len; i++) {
		PWML_Mod* mod = _pwml_load_mod(g_ptr_array_index(files, i));
		if (mod) {
			if (g_hash_table_contains(active_mods, mod->id))
				mod->active = true;

			g_hash_table_insert(pwml->mods, strdup(mod->id), mod);
		}
	}

	g_ptr_array_free(files, true);
	if (active_mods)
		g_hash_table_destroy(active_mods);
}

GPtrArray* pwml_list_mods(PWML* pwml) {
	GPtrArray* mods = g_ptr_array_new();
	GHashTableIter iter;
	g_hash_table_iter_init(&iter, pwml->mods);

	PWML_Mod* mod;
	while (g_hash_table_iter_next(&iter, NULL, (void**)&mod)) {
		g_ptr_array_add(mods, strdup(mod->id));
	}

	return mods;
}

void pwml_set_mod_active(PWML* pwml, const char* id, bool active) {
	if (g_hash_table_contains(pwml->mods, id)) {
		PWML_Mod* mod = g_hash_table_lookup(pwml->mods, id);
		mod->active = active;
	} else {
		char activity[11];
		if (active)
			strcpy(activity, "activate");
		else
			strcpy(activity, "deactivate");

		g_printerr("Couldn't %s mod %s; No such mod exists.\n", activity, id);
	}
}

bool pwml_is_mod_active(PWML* pwml, const char* id) {
	return g_hash_table_contains(pwml->mods, id) ? ((PWML_Mod*)g_hash_table_lookup(pwml->mods, id))->active : false;
}

const char* pwml_get_mod_name(PWML* pwml, const char* id) {
	if (!g_hash_table_contains(pwml->mods, id)) {
		g_printerr("Couldn't get name of mod with id %s; No such mod exists.\n", id);
	}
	return ((PWML_Mod*)g_hash_table_lookup(pwml->mods, id))->name;
}

const char* pwml_get_mod_description(PWML* pwml, const char* id) {
	if (!g_hash_table_contains(pwml->mods, id)) {
		g_printerr("Couldn't get name of mod with id %s; No such mod exists.\n", id);
	}
	PWML_Mod* mod = g_hash_table_lookup(pwml->mods, id);
	if (!mod->description) {
		const char* path = g_build_filename(mod->path, PWML_MOD_DESCRIPTION_FILE, NULL);
		if (!g_file_test(path, G_FILE_TEST_EXISTS))
			return NULL;

		GError* error = NULL;
		char* buffer;

		g_file_get_contents(path, &buffer, NULL, &error);
		if (error) {
			g_print("<span foreground=\"red\"><b>Error:</b></span> failed to read description file at %s\n\nGError->message = %s", path, error->message);
			return NULL;
		}

		mod->description = buffer;
	}
	return mod->description;
}

// pretty sure you can replace this with strcmp and a cast
static int __compare_alphabetical(const void* _a, const void* _b) {
	const char* a = _a;
	const char* b = _b;
	uint i = 0;
	while (a[i] != '\0' && b[i] != '\0') {
		int diff = a[i] - b[i];
		if (diff != 0) {
			return diff;
		}
		i++;
	}

	return strlen(a) - strlen(b);
}

static void __pwml_write_weapons_dat(PWML* pwml) {
	//g_print("--------------\nApplying mods:\npwml->weapons->len: %u\n", pwml->weapons->len);

	GPtrArray* weapon_names = g_ptr_array_new();
	GPtrArray* ship_weapon_names = g_ptr_array_new();
	GPtrArray* pilot_weapon_names = g_ptr_array_new();

	uint weapons_dat_size = 0;
	// Weapons:\n
	weapons_dat_size += 9;
	// Ship weapons:\n
	weapons_dat_size += 14;
	// Pilot weapons:\n
	weapons_dat_size += 15;

	for (uint i = 0; i < pwml->weapons->len; i++) {
		_PWML_Weapon* weapon = g_ptr_array_index(pwml->weapons, i);

		// +2 for the two spaces, +1 for the newline
		uint name_size = strlen(weapon->name) + 3;
		g_ptr_array_add(weapon_names, strdup(weapon->name));
		weapons_dat_size += name_size;

		if (weapon->ship) {
			g_ptr_array_add(ship_weapon_names, strdup(weapon->name));
			weapons_dat_size += name_size;
		}
		if (weapon->pilot) {
			g_ptr_array_add(pilot_weapon_names, strdup(weapon->name));
			weapons_dat_size += name_size;
		}
	}

	g_ptr_array_sort_values(weapon_names, __compare_alphabetical);
	g_ptr_array_sort_values(ship_weapon_names, __compare_alphabetical);
	g_ptr_array_sort_values(pilot_weapon_names, __compare_alphabetical);

	// NULL terminator
	weapons_dat_size += 1;
	char* weapons_dat_data = calloc(weapons_dat_size, sizeof(char));

	strcat(weapons_dat_data, "Weapons:\n");
	for (uint i = 0; i < weapon_names->len; i++) {
		strcat(weapons_dat_data, "  ");
		strcat(weapons_dat_data, g_ptr_array_index(weapon_names, i));
		strcat(weapons_dat_data, "\n");
	}

	strcat(weapons_dat_data, "Ship weapons:\n");
	for (uint i = 0; i < ship_weapon_names->len; i++) {
		strcat(weapons_dat_data, "  ");
		strcat(weapons_dat_data, g_ptr_array_index(ship_weapon_names, i));
		strcat(weapons_dat_data, "\n");
	}

	strcat(weapons_dat_data, "Pilot weapons:\n");
	for (uint i = 0; i < pilot_weapon_names->len; i++) {
		strcat(weapons_dat_data, "  ");
		strcat(weapons_dat_data, g_ptr_array_index(pilot_weapon_names, i));
		strcat(weapons_dat_data, "\n");
	}

	const char* weapons_dat_path = g_build_filename(pwml->weapons_path, PWML_WEAPONS_DAT, NULL);

	GError* error = NULL;
	g_file_set_contents(weapons_dat_path, weapons_dat_data, -1, &error);
	if (error) {
		g_printerr("Failed to write weapons.dat at %s\nGError: %s\n", weapons_dat_path, error->message);
		g_error_free(error);
	}

	free((char*)weapons_dat_path);
	free((char*)weapons_dat_data);

	g_ptr_array_free(weapon_names, true);
	g_ptr_array_free(ship_weapon_names, true);
	g_ptr_array_free(pilot_weapon_names, true);
}

static void __pwml_write_menu_music_txt(PWML* pwml) {
	uint size = 0;
	char* buffer = calloc(1, sizeof(char));
	for (uint i = 0; i < pwml->menu_music_paths->len; i++) {
		const char* path = g_ptr_array_index(pwml->menu_music_paths, i);

		char* contents;
		GError* error = NULL;

		g_file_get_contents(path, &contents, NULL, &error);
		if (error) {
			g_printerr("Failed to read file %s\nGError: %s\n", path, error->message);
			g_error_free(error);
			continue;
		}

		uint old = size;
		size += strlen(contents) + 1;
		buffer = realloc(buffer, size * sizeof(char));
		strcpy(buffer + old, contents);
		buffer[size - 1] = '\n';
	}
	if (size > 0)
		buffer[size - 1] = '\0';
	
	const char* menu_music_txt_path = g_build_filename(pwml->music_path, PWML_MENU_MUSIC_TXT, NULL);

	GError* error = NULL;
	g_file_set_contents(menu_music_txt_path, buffer, -1, &error);
	if (error) {
		g_printerr("Failed to write %s\nGError: %s\n", menu_music_txt_path, error->message);
		g_error_free(error);
	}

	free((char*)menu_music_txt_path);
	free(buffer);
}

static void _g_ptr_array_clear(GPtrArray* array) {
	g_ptr_array_remove_range(array, 0, array->len);
}

void pwml_apply_mods(PWML* pwml) {
	_file_utils_delete_all(pwml->graphics_path);
	_file_utils_delete_all(pwml->levels_path);
	_file_utils_delete_all(pwml->music_path);
	_file_utils_delete_all(pwml->objects_path);
	_file_utils_delete_all(pwml->sound_path);
	_file_utils_delete_all(pwml->weapons_path);

	GHashTableIter iter;
	g_hash_table_iter_init(&iter, pwml->mods);

	PWML_Mod* mod;
	while (g_hash_table_iter_next(&iter, NULL, (void**)&mod)) {
		if (mod->active) {
			_pwml_mod_apply(pwml, mod);
		}
	}
	
	__pwml_write_weapons_dat(pwml);
	_g_ptr_array_clear(pwml->weapons);

	__pwml_write_menu_music_txt(pwml);
	_g_ptr_array_clear(pwml->menu_music_paths);

	const char* graphics_xml_path = g_build_filename(pwml->graphics_path, PWML_GRAPHICS_XML, NULL);
	_xml_utils_combine_all_files(pwml->graphics_xml_paths, graphics_xml_path);
	_g_ptr_array_clear(pwml->graphics_xml_paths);
	const char* sounds_xml_path = g_build_filename(pwml->sound_path, PWML_SOUNDS_XML, NULL);
	_xml_utils_combine_all_files(pwml->sounds_xml_paths, sounds_xml_path);
	_g_ptr_array_clear(pwml->sounds_xml_paths);
	free((char*)graphics_xml_path);
	free((char*)sounds_xml_path);
}
