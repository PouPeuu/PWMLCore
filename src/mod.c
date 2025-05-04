#include "PWML/mod.h"
#include <stdlib.h>

void pwml_mod_free(PWML_Mod *mod) {
	free((char*)mod->path);
	free((char*)mod->id);
	free((char*)mod->name);
	free((char*)mod->description);
	free(mod);
}
