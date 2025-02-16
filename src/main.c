#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>

#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>

#include "modules.h"
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

    struct String main_file;
    {
        int len = strlen(argv[1]);
        if (len < 5 || strcmp(&argv[1][len - 5], ".tara") != 0) {
            printf("Please provide a file with a '.tara' extension!\n");
            return 1;
        }
        char* str = serene_trenalloc(&alloc, len - 4, char);
        assert(str && "OOM");
        snprintf(str, len - 4, "%s", argv[1]);
        main_file.str = basename(str);
        main_file.len = strlen(main_file.str);
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
        (struct String){dir_path, dir_len}
    );

    ModuleNode_print(modules, 0);

    struct ModuleNode* entry = ModuleNode_get(&modules, main_file);
    assert(entry != NULL);

    struct Intern intern = Intern_init(strings_alloc);
    struct Symbols symbols = populate_interner(&intern);

    printf("[\n");
    {
        struct Lexer lexer = {
            .rest = entry->self.contents,
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

    scan(&intern, entry, &modules);

    struct Tokenstream stream = {
        .buf = entry->self.tokens.buf,
        .len = entry->self.tokens.len,
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
        entry->self.ops,
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

    system("ld.lld -o out out.o");

    ModuleNode_unmap(modules);
    serene_Trea_deinit(alloc);
    printf("\n");
    return 0;
}

