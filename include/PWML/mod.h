#ifndef PWML_MOD_H
#define PWML_MOD_H

#include <stdbool.h>

typedef struct PWML PWML;

typedef struct {
	const char* path;
	const char* id;
	const char* name;
	const char* description;
	bool active;
} PWML_Mod;

void pwml_mod_free(PWML_Mod* mod);

void _pwml_mod_apply(PWML* pwml, PWML_Mod* mod);

#endif
