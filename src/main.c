#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>

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
#include "serene.h"

int main(int argc, char **argv) {
    struct serene_Arena strings_arena, ast_arena, type_arena, check_arena,
        tst_arena, codegen_arena;
    strings_arena = ast_arena = type_arena = check_arena = tst_arena =
        codegen_arena = (struct serene_Arena){
            .backing = serene_Libc_dyn(),
        };

    if (argc != 2) {
        printf("Please provide a filename!\n");
        return 1;
    }
    char *filename = argv[1];
    struct stat filestat = {0};
    if (stat(filename, &filestat) == -1) {
        printf("Could not access file '%s'!\n", filename);
        return 1;
    }
    int filedesc = open(filename, O_RDONLY);
    char *file =
        mmap(NULL, filestat.st_size, PROT_READ, MAP_PRIVATE, filedesc, 0);

    struct Intern intern = {0};
    struct Symbols symbols =
        populate_interner(serene_Arena_dyn(&strings_arena), &intern);

    struct Tokenvec tokens =
        lex(serene_Arena_dyn(&strings_arena), &intern, file);

    munmap(file, filestat.st_size);
    close(filedesc);

    printf("[\n");
    struct Tokenstream stream = Tokenvec_stream(&tokens);
    for (size_t i = 0; i < stream.len; i++) {
        printf(
            "\t(%p)\t'%s'\n", stream.buf[i].spelling, stream.buf[i].spelling
        );
    }
    printf("]\n");

    struct Opdecls ops = {0};
    struct TypeIntern types =
        TypeIntern_init(serene_Arena_dyn(&type_arena), symbols);

    struct Ast ast = parse(
        serene_Arena_dyn(&ast_arena), ops, &types, Tokenvec_stream(&tokens)
    );
    Tokenvec_deinit(&tokens);

    Ast_print(&ast);
    
    typecheck(serene_Arena_dyn(&check_arena), &types, &ast);
    serene_Arena_deinit(&check_arena);

    Ast_print(&ast);

    struct Tst tst = convert_ast(serene_Arena_dyn(&tst_arena), &types, ast);
    LLVMModuleRef mod = lower(&tst, serene_Arena_dyn(&codegen_arena));
    serene_Arena_deinit(&codegen_arena);
    serene_Arena_deinit(&tst_arena);

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

    serene_Arena_deinit(&ast_arena);
    serene_Arena_deinit(&type_arena);
    serene_Arena_deinit(&strings_arena);
    printf("\n");
    return 0;
}
