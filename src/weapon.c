#include "PWML/weapon.h"
#include "glib.h"
#include <stdlib.h>

void _pwml_weapon_free(void* weapon_ptr) {
	_PWML_Weapon* weapon = (_PWML_Weapon*)weapon_ptr;
	free((char*)weapon->name);
	free(weapon);
}

bool _pwml_weapon_equals(_PWML_Weapon* w1, _PWML_Weapon* w2) {
	return g_str_equal(w1->name, w2->name);
}
