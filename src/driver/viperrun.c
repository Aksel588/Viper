#include "bytecode.h"
#include "diagnostics.h"
#include "vm.h"

#include <stdio.h>
#include <stdlib.h>

static void print_usage(const char *prog) {
    fprintf(stderr, "Usage: %s FILE.vbc\n", prog);
}

int main(int argc, char **argv) {
    if (argc != 2) {
        print_usage(argv[0]);
        return 1;
    }

    DiagContext diag;
    if (!diag_init(&diag, 16)) {
        return 1;
    }

    BcProgram prog;
    if (!bc_read_file(&prog, argv[1])) {
        fprintf(stderr, "failed to read bytecode file '%s'\n", argv[1]);
        diag_free(&diag);
        return 1;
    }

    bool ok = vm_run(&prog, &diag);
    if (diag.error_count > 0) {
        diag_print_all(&diag, stderr);
    }

    bc_program_free(&prog);
    diag_free(&diag);
    return ok ? 0 : 1;
}
