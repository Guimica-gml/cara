#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>

#include <llvm-c/Core.h>
#include <llvm-c/Target.h>
#include <llvm-c/TargetMachine.h>

#include "mtree.h"
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
#include "opscan.h"
#include "preimport.h"
#include "serene.h"

void source_print(void* _string) {
    struct String* string = _string;
    printf("String{ %p, len: %zu }", string->str, string->len);
}

int main(int argc, char** argv) {
    struct serene_Trea
        alloc = serene_Trea_init(serene_Libc_dyn()),
        module_alloc = serene_Trea_sub(&alloc),
        tst_alloc = serene_Trea_sub(&alloc),
        strings_alloc = serene_Trea_sub(&alloc);
        // type_alloc = serene_Trea_sub(&strings_alloc);
        // ast_alloc = serene_Trea_sub(&type_alloc);

    if (argc != 2) {
        printf("Please provide a single filename!\n");
        return 1;
    }

    struct String main_mod;
    {
        int len = strlen(argv[1]);
        if (len < 5 || strcmp(&argv[1][len - 5], ".tara") != 0) {
            printf("Please provide a file with a '.tara' extension!\n");
            return 1;
        }
        char* str = serene_trenalloc(&alloc, len - 4, char);
        assert(str && "OOM");
        snprintf(str, len - 4, "%s", argv[1]);
        main_mod.str = basename(str);
        main_mod.len = strlen(main_mod.str);
    }
    char* dir_path = dirname(argv[1]);
    int dir_len = strlen(dir_path);
    struct MTree* mtree = MTree_load(&module_alloc, (struct String){dir_path, dir_len});
    printf("\n--- load time: ---\n");
    MTree_print(mtree, source_print);

    struct Intern intern = Intern_init(strings_alloc);
    struct Symbols symbols = populate_interner(&intern);
    mtree = scan(&module_alloc, &intern, mtree);
    printf("\n--- scan time: ---\n");
    MTree_print(mtree, PIData_print);

    mtree = preimport(&module_alloc, mtree);
    printf("\n--- preimport time: ---\n");
    MTree_print(mtree, PIData_print);

    mtree = parse(&module_alloc, &symbols, mtree);
    printf("\n--- parse time: ---\n");
    MTree_print(mtree, PTData_print);

    typecheck(mtree);
    printf("\n--- typecheck time: ---\n");
    MTree_print(mtree, PTData_print);

    struct Tst tst = convert_ast(&tst_alloc, MTree_index(mtree, main_mod)->data);

    /* struct serene_Trea tst_alloc = serene_Trea_sub(&alloc); */
    /* struct Tst tst = convert_ast(&tst_alloc, &types, ast); */
    LLVMModuleRef mod = lower(&tst, serene_Trea_sub(&tst_alloc));
    /* serene_Trea_deinit(tst_alloc); */

    printf("\n--- lolvm time: ---\n");
    LLVMDumpModule(mod);

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

    serene_Trea_deinit(alloc);
    printf("\n");
    return 0;
}

