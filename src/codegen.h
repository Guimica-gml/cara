#ifndef CODEGEN_H
#define CODEGEN_H

#include "./tst.h"
#include "serene.h"
#include <llvm-c/Core.h>

LLVMModuleRef lower(struct Tst *, struct serene_Trea alloc);

#endif
