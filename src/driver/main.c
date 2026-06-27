#include "arena.h"
#include "builtins.h"
#include "bytecode.h"
#include "diagnostics.h"
#include "discovery.h"
#include "lexer.h"
#include "module.h"
#include "parser.h"
#include "paths.h"
#include "semantic.h"
#include "source.h"
#include "symbol_table.h"
#include "viper.h"
#include "vm.h"

#include <libgen.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

typedef struct CompileOptions {
    bool run;
    const char *output_path;
} CompileOptions;

typedef struct ProjectFns {
    AstNode **fn_nodes;
    int fn_count;
    int fn_cap;
    Arena *arenas;
    int arena_count;
    int arena_cap;
} ProjectFns;

static void project_fns_free(ProjectFns *pf) {
    for (int i = 0; i < pf->arena_count; i++) {
        arena_free(&pf->arenas[i]);
    }
    free(pf->arenas);
    free(pf->fn_nodes);
    memset(pf, 0, sizeof(*pf));
}

static bool project_fns_add(ProjectFns *pf, AstNode *fn) {
    if (pf->fn_count >= pf->fn_cap) {
        int cap = pf->fn_cap == 0 ? 8 : pf->fn_cap * 2;
        AstNode **nodes = (AstNode **)realloc(pf->fn_nodes, (size_t)cap * sizeof(AstNode *));
        if (!nodes) {
            return false;
        }
        pf->fn_nodes = nodes;
        pf->fn_cap = cap;
    }
    pf->fn_nodes[pf->fn_count++] = fn;
    return true;
}

static bool file_list_contains(const FileList *list, const char *path) {
    for (int i = 0; i < list->count; i++) {
        if (strcmp(list->paths[i], path) == 0) {
            return true;
        }
    }
    return false;
}

static bool file_list_add_unique(FileList *list, const char *path) {
    if (file_list_contains(list, path)) {
        return true;
    }
    char **paths = (char **)realloc(list->paths, (size_t)(list->count + 1) * sizeof(char *));
    if (!paths) {
        return false;
    }
    list->paths = paths;
    list->paths[list->count] = strdup(path);
    if (!list->paths[list->count]) {
        return false;
    }
    list->count++;
    return true;
}

static bool file_list_merge_unique(FileList *dest, const FileList *src) {
    for (int i = 0; i < src->count; i++) {
        if (!file_list_add_unique(dest, src->paths[i])) {
            return false;
        }
    }
    return true;
}

static char *parent_directory(const char *path) {
    char *copy = strdup(path);
    if (!copy) {
        return NULL;
    }
    char *dir = dirname(copy);
    char *result = strdup(dir);
    free(copy);
    return result;
}

static bool load_project_functions(const FileList *files, DiagContext *diag, ProjectFns *out) {
    memset(out, 0, sizeof(*out));
    for (int i = 0; i < files->count; i++) {
        long length = 0;
        char *source = read_file_contents(files->paths[i], &length, diag);
        if (!source) {
            project_fns_free(out);
            return false;
        }
        if (out->arena_count >= out->arena_cap) {
            int cap = out->arena_cap == 0 ? 4 : out->arena_cap * 2;
            Arena *arenas = (Arena *)realloc(out->arenas, (size_t)cap * sizeof(Arena));
            if (!arenas) {
                free(source);
                project_fns_free(out);
                return false;
            }
            out->arenas = arenas;
            out->arena_cap = cap;
        }
        Arena *arena = &out->arenas[out->arena_count];
        if (!arena_init(arena, 1024 * 64)) {
            free(source);
            project_fns_free(out);
            return false;
        }
        out->arena_count++;

        Lexer lex;
        lexer_init(&lex, source, (int)length, files->paths[i], diag);
        TokenList tokens;
        if (!lexer_tokenize(&lex, &tokens)) {
            free(source);
            project_fns_free(out);
            return false;
        }
        Parser parser;
        char *default_mod = module_name_from_path(files->paths[i]);
        parser_init(&parser, &tokens, arena, diag, files->paths[i], default_mod ? default_mod : "main");
        free(default_mod);
        AstNode *program = parse_program(&parser);
        token_list_free(&tokens);
        if (!program || diag->error_count > 0) {
            free(source);
            project_fns_free(out);
            return false;
        }
        for (int s = 0; s < program->as.program.statement_count; s++) {
            AstNode *stmt = program->as.program.statements[s];
            if (stmt->kind == AST_FN_DECL && !project_fns_add(out, stmt)) {
                free(source);
                project_fns_free(out);
                return false;
            }
        }
        free(source);
    }
    return true;
}

static bool compile_file_pass1(const char *path, SymbolTable *symtab, ModuleIndex *module_index, DiagContext *diag) {
    long length = 0;
    char *source = read_file_contents(path, &length, diag);
    if (!source) {
        return false;
    }

    Arena arena;
    if (!arena_init(&arena, 1024 * 64)) {
        free(source);
        diag_emit(diag, DIAG_ERROR, path, 0, 0, "arena allocation failed");
        return false;
    }

    Lexer lex;
    lexer_init(&lex, source, (int)length, path, diag);
    TokenList tokens;
    if (!lexer_tokenize(&lex, &tokens)) {
        arena_free(&arena);
        free(source);
        return false;
    }

    Parser parser;
    char *default_mod = module_name_from_path(path);
    parser_init(&parser, &tokens, &arena, diag, path, default_mod ? default_mod : "main");
    free(default_mod);
    AstNode **fn_nodes = NULL;
    int fn_count = 0;
    if (!parse_fn_signatures_only(&parser, &fn_nodes, &fn_count)) {
        token_list_free(&tokens);
        arena_free(&arena);
        free(fn_nodes);
        free(source);
        return false;
    }

    SemanticContext sem = {symtab, diag, &arena, path, NULL, NULL, 0, module_index, viper_type_void(), false};
    bool ok = true;
    for (int i = 0; i < fn_count; i++) {
        if (!semantic_register_fn(&sem, fn_nodes[i])) {
            ok = false;
        }
    }

    free(fn_nodes);
    token_list_free(&tokens);
    arena_free(&arena);
    free(source);
    return ok && diag->error_count == 0;
}

static bool compile_file_pass2(const char *path, SymbolTable *symtab, ModuleIndex *module_index, DiagContext *diag,
                               BcProgram *out_prog, CompileOptions opts, ProjectFns *project_fns) {
    long length = 0;
    char *source = read_file_contents(path, &length, diag);
    if (!source) {
        return false;
    }

    Arena arena;
    if (!arena_init(&arena, 1024 * 256)) {
        free(source);
        diag_emit(diag, DIAG_ERROR, path, 0, 0, "arena allocation failed");
        return false;
    }

    Lexer lex;
    lexer_init(&lex, source, (int)length, path, diag);
    TokenList tokens;
    if (!lexer_tokenize(&lex, &tokens)) {
        arena_free(&arena);
        free(source);
        return false;
    }

    Parser parser;
    char *default_mod = module_name_from_path(path);
    parser_init(&parser, &tokens, &arena, diag, path, default_mod ? default_mod : "main");
    free(default_mod);
    AstNode *program = parse_program(&parser);
    if (diag->error_count > 0) {
        token_list_free(&tokens);
        arena_free(&arena);
        free(source);
        return false;
    }

    SemanticContext sem = {symtab, diag, &arena, path, NULL, NULL, 0, module_index, viper_type_void(), false};
    if (!semantic_check_program(&sem, program)) {
        token_list_free(&tokens);
        arena_free(&arena);
        free(source);
        return false;
    }

    BcProgram prog;
    if (!bc_compile_program(program, project_fns ? project_fns->fn_nodes : NULL,
                            project_fns ? project_fns->fn_count : 0, &prog, diag)) {
        token_list_free(&tokens);
        arena_free(&arena);
        free(source);
        return false;
    }

    if (opts.output_path && !bc_write_file(&prog, opts.output_path)) {
        diag_emit(diag, DIAG_ERROR, path, 0, 0, "failed to write bytecode to '%s'", opts.output_path);
        bc_program_free(&prog);
        token_list_free(&tokens);
        arena_free(&arena);
        free(source);
        return false;
    }

    if (opts.run && !vm_run(&prog, diag)) {
        bc_program_free(&prog);
        token_list_free(&tokens);
        arena_free(&arena);
        free(source);
        return false;
    }

    if (out_prog) {
        *out_prog = prog;
    } else {
        bc_program_free(&prog);
    }

    token_list_free(&tokens);
    arena_free(&arena);
    free(source);
    return diag->error_count == 0;
}

static void print_version(void) {
    printf("viper %s\n", VIPER_VERSION);
}

static void print_help(const char *prog) {
    fprintf(stderr, "Viper Compiler %s\n\n", VIPER_VERSION);
    fprintf(stderr, "Usage: %s [options] [path]\n\n", prog);
    fprintf(stderr, "  path                .vp file or directory to compile (default: \".\")\n\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -p, --project ROOT  module search root for your project\n");
    fprintf(stderr, "  -r                  recursively discover .vp files under project root\n");
    fprintf(stderr, "  --run               compile and execute\n");
    fprintf(stderr, "  -o FILE.vbc         write bytecode output\n");
    fprintf(stderr, "  --verbose           print discovery and compilation steps to stderr\n");
    fprintf(stderr, "  --version           show version and exit\n");
    fprintf(stderr, "  -h, --help          show this help and exit\n\n");
    fprintf(stderr, "Environment:\n");
    fprintf(stderr, "  VIPER_PATH          standard library directory (default: %s)\n\n", VIPER_DEFAULT_LIB);
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "  %s --run hello.vp\n", prog);
    fprintf(stderr, "  %s -p myproject -r --run main.vp\n", prog);
    fprintf(stderr, "  %s --run program.vp    # open math resolves from VIPER_PATH\n", prog);
}

static void verbose_log(bool verbose, const char *fmt, ...) {
    if (!verbose) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "viper: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

static void verbose_file_list(bool verbose, const char *label, const FileList *list) {
    if (!verbose || !list) {
        return;
    }
    fprintf(stderr, "viper: %s (%d file(s)):\n", label, list->count);
    for (int i = 0; i < list->count; i++) {
        fprintf(stderr, "  %s\n", list->paths[i]);
    }
}

static bool is_regular_file(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

int main(int argc, char **argv) {
    const char *path = ".";
    const char *project_root = NULL;
    bool recursive = false;
    bool run_flag = false;
    bool verbose = false;
    const char *output_path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--run") == 0) {
            run_flag = true;
        } else if (strcmp(argv[i], "-r") == 0) {
            recursive = true;
        } else if (strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "--version") == 0) {
            print_version();
            return 0;
        } else if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--project") == 0) {
            if (i + 1 >= argc) {
                print_help(argv[0]);
                return 1;
            }
            project_root = argv[++i];
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                print_help(argv[0]);
                return 1;
            }
            output_path = argv[++i];
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            return 0;
        } else if (argv[i][0] == '-') {
            print_help(argv[0]);
            return 1;
        } else {
            path = argv[i];
        }
    }

    source_mgr_init(&g_source_mgr);

    DiagContext diag;
    if (!diag_init(&diag, 32)) {
        fprintf(stderr, "failed to initialize diagnostics\n");
        return 1;
    }

    bool single_file = is_regular_file(path) && path_is_vp_file(path);
    if (!project_root) {
        project_root = single_file ? parent_directory(path) : path;
    }
    if (!project_root) {
        project_root = ".";
    }

    const char *stdlib_path = viper_stdlib_path();
    verbose_log(verbose, "VIPER_PATH (stdlib): %s", stdlib_path);
    verbose_log(verbose, "project root: %s", project_root);

    ModuleIndex index;
    ModuleIndex std_index;
    ModuleIndex proj_index;
    memset(&index, 0, sizeof(index));
    memset(&std_index, 0, sizeof(std_index));
    memset(&proj_index, 0, sizeof(proj_index));

    int stdlib_count = 0;
    if (viper_path_exists(stdlib_path)) {
        if (!module_index_build(stdlib_path, true, &std_index, &diag)) {
            diag_print_all(&diag, stderr);
            module_index_free(&std_index);
            source_mgr_free(&g_source_mgr);
            diag_free(&diag);
            return 1;
        }
        stdlib_count = std_index.count;
        if (!module_index_merge(&index, &std_index, &diag)) {
            diag_emit(&diag, DIAG_ERROR, NULL, 0, 0, "failed to merge stdlib module index");
            module_index_free(&index);
            module_index_free(&std_index);
            source_mgr_free(&g_source_mgr);
            diag_free(&diag);
            return 1;
        }
        module_index_free(&std_index);
    } else {
        verbose_log(verbose, "stdlib path not found, skipping");
    }

    if (!module_index_build(project_root, recursive, &proj_index, &diag)) {
        diag_print_all(&diag, stderr);
        module_index_free(&index);
        source_mgr_free(&g_source_mgr);
        diag_free(&diag);
        return 1;
    }

    if (!module_index_merge(&index, &proj_index, &diag)) {
        diag_emit(&diag, DIAG_ERROR, NULL, 0, 0, "failed to merge project module index");
        module_index_free(&index);
        module_index_free(&proj_index);
        source_mgr_free(&g_source_mgr);
        diag_free(&diag);
        return 1;
    }

    verbose_log(verbose, "module index: %d stdlib + %d project = %d total", stdlib_count, proj_index.count,
                index.count);
    module_index_free(&proj_index);

    FileList pass2_files;
    FileList pass1_files;
    memset(&pass2_files, 0, sizeof(pass2_files));
    memset(&pass1_files, 0, sizeof(pass1_files));

    if (single_file) {
        if (!file_list_add_unique(&pass2_files, path)) {
            diag_emit(&diag, DIAG_ERROR, path, 0, 0, "out of memory");
        }
    } else {
        bool ok = recursive ? discover_vp_files_recursive(path, &pass2_files, &diag)
                            : discover_vp_files(path, &pass2_files, &diag);
        if (!ok) {
            diag_print_all(&diag, stderr);
            module_index_free(&index);
            file_list_free(&pass2_files);
            source_mgr_free(&g_source_mgr);
            diag_free(&diag);
            return 1;
        }
    }

    if (pass2_files.count == 0) {
        diag_emit(&diag, DIAG_ERROR, path, 0, 0, "no .vp files found");
        diag_print_all(&diag, stderr);
        module_index_free(&index);
        file_list_free(&pass2_files);
        source_mgr_free(&g_source_mgr);
        diag_free(&diag);
        return 1;
    }

    for (int i = 0; i < pass2_files.count; i++) {
        FileList closure;
        memset(&closure, 0, sizeof(closure));
        if (!module_resolve_deps(pass2_files.paths[i], &index, &closure, &diag)) {
            file_list_free(&closure);
            continue;
        }
        if (verbose) {
            fprintf(stderr, "viper: dependency closure for %s (%d file(s)):\n", pass2_files.paths[i], closure.count);
            for (int j = 0; j < closure.count; j++) {
                fprintf(stderr, "  %s\n", closure.paths[j]);
            }
        }
        if (!file_list_merge_unique(&pass1_files, &closure)) {
            file_list_free(&closure);
            diag_emit(&diag, DIAG_ERROR, pass2_files.paths[i], 0, 0, "out of memory");
            continue;
        }
        file_list_free(&closure);
    }

    SymbolTable *symtab = symtab_create();
    if (!symtab) {
        diag_emit(&diag, DIAG_ERROR, NULL, 0, 0, "failed to create symbol table");
        diag_print_all(&diag, stderr);
        module_index_free(&index);
        file_list_free(&pass2_files);
        file_list_free(&pass1_files);
        source_mgr_free(&g_source_mgr);
        diag_free(&diag);
        return 1;
    }

    builtins_register(symtab);

    verbose_file_list(verbose, "Pass 1 files", &pass1_files);
    verbose_file_list(verbose, "Pass 2 files", &pass2_files);

    ProjectFns project_fns;
    if (!load_project_functions(&pass1_files, &diag, &project_fns)) {
        diag_print_all(&diag, stderr);
        project_fns_free(&project_fns);
        symtab_destroy(symtab);
        module_index_free(&index);
        file_list_free(&pass2_files);
        file_list_free(&pass1_files);
        source_mgr_free(&g_source_mgr);
        diag_free(&diag);
        return 1;
    }

    bool ok = true;
    for (int i = 0; i < pass1_files.count; i++) {
        if (!compile_file_pass1(pass1_files.paths[i], symtab, &index, &diag)) {
            ok = false;
        }
    }

    CompileOptions opts = {run_flag, output_path};
    if (ok) {
        for (int i = 0; i < pass2_files.count; i++) {
            if (!compile_file_pass2(pass2_files.paths[i], symtab, &index, &diag, NULL, opts, &project_fns)) {
                ok = false;
            }
        }
    }

    project_fns_free(&project_fns);

    if (diag.error_count > 0) {
        diag_print_all(&diag, stderr);
    } else if (!run_flag) {
        printf("Compiled %d file(s) successfully.\n", pass2_files.count);
        verbose_log(verbose, "compilation finished successfully");
    } else {
        verbose_log(verbose, "execution finished successfully");
    }

    symtab_destroy(symtab);
    module_index_free(&index);
    file_list_free(&pass2_files);
    file_list_free(&pass1_files);
    source_mgr_free(&g_source_mgr);
    diag_free(&diag);
    return ok && diag.error_count == 0 ? 0 : 1;
}
