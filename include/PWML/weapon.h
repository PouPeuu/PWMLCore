#ifndef WEAPON_H
#define WEAPON_H

#include <stdbool.h>

typedef struct {
	const char* name;
	bool ship;
	bool pilot;
} _PWML_Weapon;

void _pwml_weapon_free(void* weapon);

#endif
