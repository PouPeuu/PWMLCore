#ifndef PWML_H
#define PWML_H

#include "PWML/mod.h"
#include <sys/types.h>
#include <stdbool.h>

extern const char* const PWML_MODS_FOLDER;
extern const char* const PWML_METADATA_JSON_NAME;

typedef struct PWML PWML;

PWML* pwml_new(const char *working_directory);

const char* pwml_get_full_path(PWML* pwml, const char* path);
PWML_Mod* pwml_list_mods(PWML* pwml, uint* n_mods);
void pwml_load_mods(PWML* pwml);

#endif
