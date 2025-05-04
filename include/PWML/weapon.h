#ifndef WEAPON_H
#define WEAPON_H

#include <stdbool.h>

typedef struct {
	const char* name;
	bool is_ship;
	bool is_pilot;
} _PWML_Weapon;

void _pwml_weapon_free(void* weapon);
bool _pwml_weapon_equals(_PWML_Weapon* w1, _PWML_Weapon* w2);

#endif
