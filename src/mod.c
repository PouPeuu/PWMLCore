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
	const char* weapons_path = g_build_filename(mod->path, PWML_WEAPONS_FOLDER, NULL);
	GPtrArray* files = _list_files_in_directory(weapons_path);

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
	const char* mod_weapons_path = g_build_filename(mod->path, PWML_WEAPONS_FOLDER, NULL);
	const char* game_weapons_path = g_build_filename(pwml->working_directory, PWML_WEAPONS_FOLDER, NULL);
	GPtrArray* weapons = __pwml_mod_get_weapons(pwml, mod);

	GPtrArray* weapon_names = g_ptr_array_new();
	GPtrArray* ship_weapon_names = g_ptr_array_new();
	GPtrArray* pilot_weapon_names = g_ptr_array_new();

	for (uint i = 0; i < weapons->len; i++) {
		_PWML_Weapon* weapon = g_ptr_array_index(weapons, i);
		const char* weapon_path = g_build_filename(mod_weapons_path, weapon->name, NULL);

		_file_utils_copy_recursive(weapon_path, game_weapons_path);
		
		const char* installed_weapons_json_path = g_build_filename(game_weapons_path, weapon->name, PWML_WEAPON_JSON, NULL);
		remove(installed_weapons_json_path);
		free((char*)installed_weapons_json_path);

		free((char*)weapon_path);

		g_ptr_array_add(weapon_names, strdup(weapon->name));

		if (weapon->ship) {
			g_ptr_array_add(ship_weapon_names, strdup(weapon->name));
		}
		if (weapon->pilot) {
			g_ptr_array_add(pilot_weapon_names, strdup(weapon->name));
		}
	}

	g_ptr_array_free(weapons, true);
	free((char*)mod_weapons_path);

	free((char*)game_weapons_path);
}

void _pwml_mod_apply(PWML* pwml, PWML_Mod* mod) {
	__pwml_mod_apply_weapons(pwml, mod);
}
