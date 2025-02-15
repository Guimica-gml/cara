#! /bin/sh

BUILD=./build
SRC=./src
OPTS="-O2"
LLVM_LIBS="core analysis target"
LLVM_CFLAGS=$(llvm-config --cflags)
LLVM_CXXFLAGS=$(llvm-config --cxxflags --ldflags --libs $LLVM_LIBS)
# SANITIZER="-fsanitize=address -fno-sanitize-recover"
SANITIZER=""

if [ "$1" = "debug" ]; then
    OPTS="$SANITIZER -Wall -Wextra -g -O0"
fi

toBuild() {
    cp $SRC/$1 $BUILD/$1
}

SERENE=0
SERENE_SRC=./serene
serene() {
    if [ "$SERENE" -eq "0" ]; then
        echo compiling serene
        cp $SERENE_SRC/lib.h $BUILD/serene.h
        $CC $OPTS -o $BUILD/serene.o -c $SERENE_SRC/lib.c
        SERENE=1
    fi
}

# refs: NONE
ORDSTRINGS=0
ordstrings() {
    if [ "$ORDSTRINGS" -eq "0" ]; then
        echo compiling ordstrings
        toBuild ordstrings.h
        toBuild ordstrings.c
        $CC $OPTS -o $BUILD/ordstrings.o -c $BUILD/ordstrings.c
        ORDSTRINGS=1
    fi
}

# refs: ordstrings
BTRINGS=0
btrings() {
    if [ "$BTRINGS" -eq "0" ]; then
        ordstrings
        echo generating btrings
        $SRC/btree/stencil.sh  \
            $SRC               \
            $BUILD             \
            ordstrings.h       \
            "struct Ordstring" \
            32                 \
            Ordstring_cmp      \
            Ordstring_print    \
            Btrings            \
            btrings
        echo compiling btrings
        $CC $OPTS -o $BUILD/btrings.o -c $BUILD/btrings.c
        BTRINGS=1
    fi
}

# refs: btrings ordstrings
STRINGS=0
strings() {
    if [ "$STRINGS" -eq "0" ]; then
        btrings
        echo compiling strings
        toBuild strings.h
        toBuild strings.c
        $CC $OPTS -o $BUILD/strings.o -c $BUILD/strings.c
        STRINGS=1
    fi
}

# refs: strings btrings ordstrings
TOKENS=0
tokens() {
    if [ "$TOKENS" -eq "0" ]; then
        strings
        echo compiling tokens
        toBuild tokens.h
        toBuild tokens.c
        $CC $OPTS -o $BUILD/tokens.o -c $BUILD/tokens.c
        TOKENS=1
    fi
}

# refs: tokens strings btrings ordstrings
TOKENVEC=0
tokenvec() {
    if [ "$TOKENVEC" -eq "0" ]; then
        tokens
        echo generating tokenvec
        $SRC/vec/stencil.sh \
            $SRC            \
            $BUILD          \
            tokens.h        \
            "struct Token"  \
            Tokenvec        \
            tokenvec
        echo compiling tokenvec
        $CC $OPTS -o $BUILD/tokenvec.o -c $BUILD/tokenvec.c
        TOKENVEC=1
    fi
}

# refs: NONE
OPDECL=0
opdecl() {
    if [ "$OPDECL" -eq "0" ]; then
        echo compiling opdecl
        toBuild opdecl.h
        OPDECL=1
    fi
}

# refs: opdecl
OPDECLVEC=0
opdeclvec() {
    if [ "$OPDECLVEC" -eq "0" ]; then
        opdecl
        echo generating opdeclvec
        $SRC/vec/stencil.sh \
            $SRC            \
            $BUILD          \
            opdecl.h        \
            "struct Opdecl" \
            Opdecls         \
            opdeclvec
        echo compiling opdeclvec
        $CC $OPTS -o $BUILD/opdeclvec.o -c $BUILD/opdeclvec.c
        OPDECLVEC=1
    fi
}

# refs: NONE
TYPES=0
types() {
    if [ "$TYPES" -eq "0" ]; then
        echo compiling types
        toBuild types.h
        toBuild types.c
        $CC $OPTS -o $BUILD/types.o -c $BUILD/types.c
        TYPES=1
    fi
}

# refs: types
TYPEREG=0
typereg() {
    if [ "$TYPEREG" -eq "0" ]; then
        types
        echo generating typereg
        $SRC/btree/stencil.sh \
            $SRC              \
            $BUILD            \
            types.h           \
            "struct Type *"   \
            32                \
            Type_cmp          \
            Type_print        \
            Typereg           \
            typereg
        echo compiling typereg
        $CC $OPTS -o $BUILD/typereg.o -c $BUILD/typereg.c
        TYPEREG=1
    fi
}

INSTANCES=0
instances() {
    if [ "$INSTANCES" -eq "0" ]; then
        echo compiling instances
        btrings
        tokenvec
        opdeclvec
        typereg
        INSTANCES=1
    fi
}

LEXER=0
lexer() {
    if [ "$LEXER" -eq "0" ]; then
        strings
        tokens
        echo compiling lexer
        toBuild lexer.h
        toBuild lexer.c
        $CC $OPTS -o $BUILD/lexer.o -c $BUILD/lexer.c
        LEXER=1
    fi
}

OPSCAN=0
opscan() {
    if [ "$OPSCAN" -eq "0" ]; then
        lexer
        strings
        opdeclvec
        tokenvec
        echo compiling opscan
        toBuild opscan.h
        toBuild opscan.c
        $CC $OPTS -o $BUILD/opscan.o -c $BUILD/opscan.c
        OPSCAN=1
    fi
}

SYMBOLS=0
symbols() {
    if [ "$SYMBOLS" -eq "0" ]; then
        strings
        echo compiling symbols
        toBuild symbols.h
        toBuild symbols.c
        $CC $OPTS -o $BUILD/symbols.o -c $BUILD/symbols.c
        SYMBOLS=1
    fi
}

COMMON_LL=0
common_ll() {
    if [ "$COMMON_LL" -eq "0" ]; then
        echo compiling common_ll
        toBuild common_ll.h
        COMMON_LL=1
    fi
}

AST=0
ast() {
    if [ "$AST" -eq "0" ]; then
        symbols
        types
        typereg
        common_ll
        echo compiling ast
        toBuild ast.h
        toBuild ast.c
        $CC $OPTS -o $BUILD/ast.o -c $BUILD/ast.c
        AST=1
    fi
}

PARSER=0
parser() {
    if [ "$PARSER" -eq "0" ]; then
        strings
        tokens
        ast
        symbols
        echo compiling parser
        toBuild parser.h
        toBuild parser.c
        $CC $OPTS -o $BUILD/parser.o -c $BUILD/parser.c
        PARSER=1
    fi
}

TST=0
tst() {
    if [ "$TST" -eq "0" ]; then
        echo compiling tst
        toBuild tst.h
        TST=1
    fi
}

TYPER=0
typer() {
    if [ "$TYPER" -eq "0" ]; then
        ast
        symbols
        tst
        common_ll
        echo compiling typer
        toBuild typer.h
        toBuild typer.c
        $CC $OPTS -o $BUILD/typer.o -c $BUILD/typer.c
        TYPER=1
    fi
}

CONVERTER=0
converter() {
    if [ "$CONVERTER" -eq "0" ]; then
        ast
        symbols
        tst
        common_ll
        echo compiling converter
        toBuild converter.h
        toBuild converter.c
        $CC $OPTS -o $BUILD/converter.o -c $BUILD/converter.c
        CONVERTER=1
    fi
}

CODEGEN=0
codegen() {
    if [ "$CODEGEN" -eq "0" ]; then
        tst
        strings
        common_ll
        echo compiling codegen
        toBuild codegen.h
        toBuild codegen.c
        $CC $LLVM_CFLAGS $OPTS -o $BUILD/codegen.o -c $BUILD/codegen.c
        CODEGEN=1
    fi
}

main() {
    echo creating build directory
    mkdir $BUILD
    serene
    echo compiling the compiler
    instances
    strings
    symbols
    tokens
    lexer
    opscan
    ast
    parser
    typer
    tst
    converter
    codegen
    echo compiling main
    toBuild main.c
    $CC $LLVM_CFLAGS $OPTS -o $BUILD/main.o -c $BUILD/main.c
    echo linking it all
    $CXX $SANITIZER $LLVM_CXXFLAGS -o $BUILD/main $BUILD/*.o
}

echo compiling with $CC
echo linking with $CXX
echo llvm cflags are $LLVM_CFLAGS
echo llvm lflags are $LLVM_CXXFLAGS

main
