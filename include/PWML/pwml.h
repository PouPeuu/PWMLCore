#ifndef PWML_H
#define PWML_H

#include "PWML/mod.h"
#include <glib.h>
#include <sys/types.h>
#include <stdbool.h>

extern const char* const PWML_MODS_FOLDER;

extern const char* const PWML_WEAPONS_FOLDER;
extern const char* const PWML_SOUND_FOLDER;
extern const char* const PWML_OBJECTS_FOLDER;
extern const char* const PWML_MUSIC_FOLDER;
extern const char* const PWML_LEVELS_FOLDER;
extern const char* const PWML_GRAPHICS_FOLDER;

extern const char* const PWML_METADATA_JSON;
extern const char* const PWML_ACTIVE_MODS_JSON;
extern const char* const PWML_WEAPON_JSON;

extern const char* const PWML_MOD_DATA_FOLDER;

typedef struct PWML {
	const char* working_directory;
	uint n_mods;
	GHashTable* mods;
} PWML;

PWML* pwml_new(const char *working_directory);
void pwml_free(PWML* pwml);

const char* pwml_get_full_path(PWML* pwml, const char* path);
GPtrArray* pwml_list_mods(PWML* pwml);
void pwml_load_mods(PWML* pwml);

void pwml_set_mod_active(PWML* pwml, const char* id, bool active);
bool pwml_is_mod_active(PWML* pwml, const char* id);

#endif
