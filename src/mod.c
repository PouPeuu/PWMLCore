#include "PWML/mod.h"
#include "PWML/file_utils.h"
#include "PWML/pwml.h"
#include "PWML/weapon.h"
#include "json_object.h"
#include "json_tokener.h"
#include "json_types.h"
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

void pwml_mod_free(PWML_Mod *mod) {
	free((char*)mod->path);
	free((char*)mod->id);
	free((char*)mod->name);
	free((char*)mod->description);
	free(mod);
}

static GPtrArray* __pwml_mod_get_weapons(PWML* pwml, PWML_Mod* mod) {
	const char* weapons_path = g_build_filename(mod->path, PWML_MOD_DATA_FOLDER, PWML_WEAPONS_FOLDER, NULL);
	GPtrArray* files = _file_utils_list_files_in_directory(weapons_path);

	GPtrArray* weapons = g_ptr_array_new_with_free_func(_pwml_weapon_free);

	for (uint i = 0; i < files->len; i++) {
		const char* weapon_path = g_ptr_array_index(files, i);
		if (!_file_utils_is_dir(weapon_path)) {
			free((char*)weapon_path);
			continue;
		}

		const char* weapon_json_path = g_build_filename(weapon_path, PWML_WEAPON_JSON, NULL);
		if (!g_file_test(weapon_json_path, G_FILE_TEST_EXISTS)) {
			free((char*)weapon_path);
			free((char*)weapon_json_path);
			continue;
		}

		char* buffer;
		GError* error = NULL;

		g_file_get_contents(weapon_json_path, &buffer, NULL, &error);
		if (error) {
			g_printerr("Failed to load contents of %s\nGError: %s\n", weapon_json_path, error->message);
			free((char*)weapon_path);
			free((char*)weapon_json_path);
			g_error_free(error);
			continue;
		}
		
		json_object* root = json_tokener_parse(buffer);
		free(buffer);

		if (!root) {
			g_printerr("Failed to parse json file %s\n", weapon_json_path);
			free((char*)weapon_path);
			free((char*)weapon_json_path);
			continue;
		}

		json_object *ship, *pilot;

		if (!json_object_object_get_ex(root, "ship", &ship) || json_object_get_type(ship) != json_type_boolean) {
			g_printerr("Failed to read ship from weapon.json at %s\n", weapon_json_path);
			free((char*)weapon_path);
			free((char*)weapon_json_path);
			continue;
		}

		if (!json_object_object_get_ex(root, "pilot", &pilot) || json_object_get_type(pilot) != json_type_boolean) {
			g_printerr("Failed to read pilot from weapon.json at %s\n", weapon_json_path);
			free((char*)weapon_path);
			free((char*)weapon_json_path);
			continue;
		}

		_PWML_Weapon* weapon = malloc(sizeof(_PWML_Weapon));
		weapon->name = g_path_get_basename(weapon_path);
		weapon->ship = json_object_get_boolean(ship);
		weapon->pilot = json_object_get_boolean(pilot);
		g_ptr_array_add(weapons, weapon);

		free((char*)weapon_json_path);
		free((char*)weapon_path);
		json_object_put(root);
	}

	return weapons;
}

static void __pwml_mod_apply_weapons(PWML* pwml, PWML_Mod* mod) {
	const char* mod_weapons_path = g_build_filename(mod->path, PWML_MOD_DATA_FOLDER, PWML_WEAPONS_FOLDER, NULL);
	const char* game_weapons_path = g_build_filename(pwml->working_directory, PWML_WEAPONS_FOLDER, NULL);
	GPtrArray* weapons = __pwml_mod_get_weapons(pwml, mod);

	for (uint i = 0; i < weapons->len; i++) {
		_PWML_Weapon* weapon = g_ptr_array_index(weapons, i);
		const char* weapon_path = g_build_filename(mod_weapons_path, weapon->name, NULL);

		_file_utils_copy_recursive(weapon_path, game_weapons_path);
		
		const char* installed_weapons_json_path = g_build_filename(game_weapons_path, weapon->name, PWML_WEAPON_JSON, NULL);
		remove(installed_weapons_json_path);
		free((char*)installed_weapons_json_path);

		free((char*)weapon_path);

		g_ptr_array_add(pwml->weapons, weapon);
	}

	g_ptr_array_free(weapons, false);
	free((char*)mod_weapons_path);
	free((char*)game_weapons_path);
}

static void __copy_all_if_dir(const char* from, const char* to) {
	if (g_file_test(from, G_FILE_TEST_IS_DIR)) {
		_file_utils_copy_all(from, to);
	}
}

static void __copy_all_if_dir_except(const char* from, const char* to, const char* ignore) {
	if (g_file_test(from, G_FILE_TEST_IS_DIR)) {
		_file_utils_copy_all_except(from, to, ignore);
	}
}

void _pwml_mod_apply(PWML* pwml, PWML_Mod* mod) {
	// I feel like there should be a better way
	const char* mod_weapons_path = g_build_filename(mod->path, PWML_MOD_DATA_FOLDER, PWML_WEAPONS_FOLDER, NULL);
	const char* mod_objects_path = g_build_filename(mod->path, PWML_MOD_DATA_FOLDER, PWML_OBJECTS_FOLDER, NULL);
	const char* mod_levels_path = g_build_filename(mod->path, PWML_MOD_DATA_FOLDER, PWML_LEVELS_FOLDER, NULL);
	const char* mod_music_path = g_build_filename(mod->path, PWML_MOD_DATA_FOLDER, PWML_MUSIC_FOLDER, NULL);
	const char* mod_graphics_path = g_build_filename(mod->path, PWML_MOD_DATA_FOLDER, PWML_GRAPHICS_FOLDER, NULL);
	const char* mod_sounds_path = g_build_filename(mod->path, PWML_MOD_DATA_FOLDER, PWML_SOUND_FOLDER, NULL);
	const char* mod_menu_music_file_path = g_build_filename(mod_music_path, PWML_MENU_MUSIC_TXT, NULL);
	const char* mod_graphics_xml_file_path = g_build_filename(mod_graphics_path, PWML_GRAPHICS_XML, NULL);
	const char* mod_sounds_xml_file_path = g_build_filename(mod_sounds_path, PWML_SOUNDS_XML, NULL);

	if (g_file_test(mod_weapons_path, G_FILE_TEST_IS_DIR)) {
		__pwml_mod_apply_weapons(pwml, mod);
	}

	__copy_all_if_dir(mod_objects_path, pwml->objects_path);
	__copy_all_if_dir(mod_levels_path, pwml->levels_path);
	__copy_all_if_dir_except(mod_music_path, pwml->music_path, PWML_MENU_MUSIC_TXT);
	__copy_all_if_dir_except(mod_graphics_path, pwml->graphics_path, PWML_GRAPHICS_XML);
	__copy_all_if_dir_except(mod_sounds_path, pwml->sound_path, PWML_SOUNDS_XML);

	if (g_file_test(mod_menu_music_file_path, G_FILE_TEST_EXISTS)) {
		g_ptr_array_add(pwml->menu_music_paths, strdup(mod_menu_music_file_path));
	}
	if (g_file_test(mod_graphics_xml_file_path, G_FILE_TEST_EXISTS)) {
		g_ptr_array_add(pwml->graphics_xml_paths, strdup(mod_graphics_xml_file_path));
	}
	if (g_file_test(mod_sounds_xml_file_path, G_FILE_TEST_EXISTS)) {
		g_ptr_array_add(pwml->sounds_xml_paths, strdup(mod_sounds_xml_file_path));
	}

	free((char*)mod_weapons_path);
	free((char*)mod_objects_path);
	free((char*)mod_levels_path);
	free((char*)mod_music_path);
	free((char*)mod_graphics_path);
	free((char*)mod_sounds_path);
	free((char*)mod_menu_music_file_path);
	free((char*)mod_graphics_xml_file_path);
	free((char*)mod_sounds_xml_file_path);
}
