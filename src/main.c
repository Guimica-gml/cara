#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

#include <libgen.h>
#include <dirent.h>
#include <string.h>

#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>

#include <errno.h>

#include "./ast.h"
#include "./converter.h"
#include "./lexer.h"
#include "./parser.h"
#include "./strings.h"
#include "./symbols.h"
#include "./tokens.h"
#include "./tst.h"
#include "./codegen.h"
#include "./typer.h"
#include "./opscan.h"
#include "serene.h"

struct Module {
    struct String contents;
    struct Tokenvec tokens;
    struct Opdecls ops;
};

struct ModuleNode {
    struct ModuleNodesLL* children;
    char* name;
    struct Module self;
};

struct ModuleNodesLL {
    struct ModuleNodesLL *next;
    struct ModuleNode current;
};

void ModuleNode_print(struct ModuleNode this, int level) {
    for (int i = 0; i < level; i++) printf("  ");
    printf("%s (%p)\n", this.name, this.self.contents.str);
    for (struct ModuleNodesLL* head = this.children; head; head = head->next) {
        ModuleNode_print(head->current, level + 1);
    }
}

void ModuleNode_unmap(struct ModuleNode tree) {
    if (tree.self.contents.str) munmap(
        (void*) tree.self.contents.str,
        tree.self.contents.len
    );
    for (struct ModuleNodesLL* head = tree.children; head; head = head->next)
        ModuleNode_unmap(head->current);
}

struct ModuleNode populate_internal(
    struct serene_Trea* alloc,
    struct serene_Trea* scratch,
    char* dir_name,
    char* dir_path,
    int dir_pathlen
) {
    struct ModuleNode out = {0};
    out.name = dir_name;
    DIR *dir = opendir(dir_path);
    for (
        struct dirent* entry = readdir(dir);
        entry;
        entry = readdir(dir)
    ) {
        if (entry->d_name[0] == '.') continue;
        int namelen = strlen(entry->d_name);
        char* full_path = serene_trenalloc(scratch, namelen + dir_pathlen + 2, char);
        assert(full_path && "OOM");
        snprintf(full_path, namelen + dir_pathlen + 2, "%s/%s", dir_path, entry->d_name);

        int fd = open(full_path, O_RDWR);
        if (fd != -1) {
            if (strncmp(entry->d_name + namelen - 5, ".tara", 5) != 0) {
                close(fd);
                continue;
            }
            struct stat stat = {0};
            assert(fstat(fd, &stat) == 0);
            struct ModuleNode child = {0};
            child.name = serene_trenalloc(alloc, namelen - 4, char);
            snprintf(child.name, namelen - 4, "%s", entry->d_name);
            child.self.contents.len = stat.st_size;
            child.self.contents.str = mmap(
                NULL,
                stat.st_size,
                PROT_READ,
                MAP_PRIVATE,
                fd,
                0
            );

            struct ModuleNodesLL* tmp = serene_trealloc(alloc, struct ModuleNodesLL);
            assert(tmp && "OOM");
            tmp->current = child;
            tmp->next = out.children;
            out.children = tmp;
        } else {
            struct ModuleNode child = populate_internal(
                alloc,
                scratch,
                entry->d_name,
                full_path,
                namelen + dir_pathlen + 1
            );
            struct ModuleNodesLL* head;
            for (head = out.children; head; head = head->next) {
                if (strcmp(head->current.name, child.name) == 0) {
                    assert(head->current.children == NULL);
                    head->current.children = child.children;
                    break;
                }
            }
            if (head == NULL) {
                struct ModuleNodesLL* tmp = serene_trealloc(alloc, struct ModuleNodesLL);
                assert(tmp && "OOM");
                tmp->current = child;
                tmp->next = out.children;
                out.children = tmp;
            }
        }
    }
    return out;
}

struct ModuleNode populate(
    struct serene_Trea* alloc,
    char* dir_name,
    char* dir_path,
    int dir_pathlen
) {
    struct serene_Trea scratch = serene_Trea_sub(alloc);
    struct ModuleNode out = populate_internal(alloc, &scratch, dir_name, dir_path, dir_pathlen);
    serene_Trea_deinit(scratch);
    return out;
}

int main(int argc, char** argv) {
    struct serene_Trea
        alloc = serene_Trea_init(serene_Libc_dyn()),
        module_alloc = serene_Trea_sub(&alloc),
        strings_alloc = serene_Trea_sub(&alloc),
        type_alloc = serene_Trea_sub(&strings_alloc),
        ast_alloc = serene_Trea_sub(&type_alloc);

    if (argc != 2) {
        printf("Please provide a single filename!\n");
        return 1;
    }

    char* main_file;
    {
        int len = strlen(argv[1]);
        if (len < 5 || strcmp(&argv[1][len - 5], ".tara") != 0) {
            printf("Please provide a file with a '.tara' extension!\n");
            return 1;
        }
        main_file = serene_trenalloc(&alloc, len - 4, char);
        assert(main_file && "OOM");
        snprintf(main_file, len - 4, "%s", argv[1]);
        main_file = basename(main_file);
    }
    char* dir_path = dirname(argv[1]);
    int dir_len = strlen(dir_path);
    char* dir_name = dir_path;
    for (int i = dir_len - 1; i >= 0; i--) {
        if (dir_path[i] != '/') continue;
        dir_name = &dir_path[i + 1];
        break;
    }
    struct ModuleNode modules = populate(
        &module_alloc,
        dir_name,
        dir_path,
        dir_len
    );

    ModuleNode_print(modules, 0);

    struct String file = {0};
    printf("searching: %s\n", main_file);
    for (struct ModuleNodesLL* head = modules.children; head; head = head->next) {
        printf("scanning: %s\n", head->current.name);
        if (strcmp(head->current.name, main_file) == 0) {
            file = head->current.self.contents;
            break;
        }
    }
    assert(file.str != NULL);

    struct Intern intern = Intern_init(strings_alloc);
    struct Symbols symbols = populate_interner(&intern);

    printf("[\n");
    {
        struct Lexer lexer = {
            .rest = file,
            .token = {0},
        };
        for (
            struct Token t = Lexer_next(&lexer);
            t.kind != TK_EOF;
            t = Lexer_next(&lexer)
        ) {
            printf(
                "\t(%p)\t'%.*s'\n",
                t.spelling.str,
                (int) t.spelling.len,
                t.spelling.str
            );
        }
    }
    printf("]\n");

    struct Opdecls ops = {0};
    struct Tokenvec tokenvec = {0};

    {
        struct Lexer lexer = {
            .rest = file,
            .token = {0},
        };
        scan(
            &intern,
            lexer,
            &tokenvec,
            &ops
        );
    }

    struct Tokenstream stream = {
        .buf = tokenvec.buf,
        .len = tokenvec.len,
    };

    printf("[\n");
    {
        struct Tokenstream printer = stream;
        for (
            struct Token t = Tokenstream_peek(&printer);
            t.kind != TK_EOF;
            Tokenstream_drop(&printer), t = Tokenstream_peek(&printer)
        ) {
            printf(
                "\t(%p)\t'%s'\n",
                t.spelling.str,
                t.spelling.str
            );
        }
    }
    printf("]\n");

    struct TypeIntern types = TypeIntern_init(type_alloc, symbols);

    struct Ast ast = parse(
        &ast_alloc,
        ops,
        &types,
        stream
    );
    Ast_print(&ast);
    
    typecheck(&types, &ast);
    Ast_print(&ast);

    struct serene_Trea tst_alloc = serene_Trea_sub(&alloc);
    struct Tst tst = convert_ast(&tst_alloc, &types, ast);
    LLVMModuleRef mod = lower(&tst, serene_Trea_sub(&tst_alloc));
    serene_Trea_deinit(tst_alloc);

    printf("----module start----\n");
    LLVMDumpModule(mod);
    printf("----module end----\n");

    LLVMInitializeNativeTarget();
    LLVMInitializeNativeAsmPrinter();
    LLVMInitializeNativeAsmParser();
    char *emitfile = "out.o";
    const char *triple = LLVMGetDefaultTargetTriple();
    const char *cpu = LLVMGetHostCPUName();
    const char *features = LLVMGetHostCPUFeatures();

    char *error = NULL;
    LLVMTargetRef target;
    if (LLVMGetTargetFromTriple(triple, &target, &error)) {
        printf("error occured!\n%s\n", error);
        assert(false);
    }
    LLVMDisposeMessage(error);
    
    LLVMTargetMachineRef machine = LLVMCreateTargetMachine(
        target, triple, cpu, features, LLVMCodeGenLevelNone,
        LLVMRelocDefault, LLVMCodeModelDefault
    );

    error = NULL;
    if (LLVMTargetMachineEmitToFile(
        machine, mod, emitfile, LLVMObjectFile, &error
    )) {
        printf("error occured!\n%s\n", error);
        assert(false);
    }
    LLVMDisposeMessage(error);
    LLVMDisposeTargetMachine(machine);
    LLVMDisposeModule(mod);

    system("ld -o out out.o");

    ModuleNode_unmap(modules);
    serene_Trea_deinit(alloc);
    printf("\n");
    return 0;
}
