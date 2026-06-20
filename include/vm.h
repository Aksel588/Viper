#ifndef VIPER_VM_H
#define VIPER_VM_H

#include <stdbool.h>

#include "bytecode.h"
#include "diagnostics.h"

bool vm_run(BcProgram *prog, DiagContext *diag);

#endif
