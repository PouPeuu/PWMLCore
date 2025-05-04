#include "PWML/weapon.h"
#include <stdlib.h>

void _pwml_weapon_free(void* voidptr_weapon) {
	_PWML_Weapon* weapon = (_PWML_Weapon*)voidptr_weapon;
	free((char*)weapon->name);
	free(weapon);
}
