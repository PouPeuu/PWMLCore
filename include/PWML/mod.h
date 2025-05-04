#ifndef PWML_MOD_H
#define PWML_MOD_H

typedef struct {
	const char* path;
	const char* id;
	const char* name;
	const char* description;
} PWML_Mod;

void pwml_mod_free(PWML_Mod* mod);

#endif
