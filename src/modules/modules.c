#include <string.h>

#include "modules.h"
#include "math_module.h"

ObjModule *loadBuiltinModule(const char *name, int length)
{
    if (strcmp(name, "math") == 0)
        return initMathModule();
    return NULL;
}
