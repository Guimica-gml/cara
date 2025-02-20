#! /bin/sh

BUILD=./build
SRC=./src
OPTS="$SANITIZER -Werror=return-type -Wall -Wextra -g -O0"
LLVM_LIBS="core analysis target"
LLVM_CFLAGS=$(llvm-config --cflags)
LLVM_CXXFLAGS=$(llvm-config --cxxflags --ldflags --libs $LLVM_LIBS)
# SANITIZER="-fsanitize=address -fno-sanitize-recover"
SANITIZER=""

if [ "$1" = "release" ]; then
    OPTS="-O2"
fi

SERENE=0
serene() {
    if [ "$SERENE" -eq "0" ]; then
        echo compiling serene
        $CC $OPTS -o $BUILD/serene.o -c $SRC/serene.c
        SERENE=1
    fi
}

# refs: NONE
ORDSTRINGS=0
ordstrings() {
    if [ "$ORDSTRINGS" -eq "0" ]; then
        echo compiling ordstrings
        $CC $OPTS -o $BUILD/ordstrings.o -c $SRC/ordstrings.c
        ORDSTRINGS=1
    fi
}

# refs: ordstrings
BTRINGS=0
btrings() {
    if [ "$BTRINGS" -eq "0" ]; then
        ordstrings
        echo compiling btrings
        $CC $OPTS -o $BUILD/btrings.o -c $SRC/btrings.c
        BTRINGS=1
    fi
}

# refs: btrings ordstrings
STRINGS=0
strings() {
    if [ "$STRINGS" -eq "0" ]; then
        btrings
        echo compiling strings
        $CC $OPTS -o $BUILD/strings.o -c $SRC/strings.c
        STRINGS=1
    fi
}

# refs: strings btrings ordstrings
TOKENS=0
tokens() {
    if [ "$TOKENS" -eq "0" ]; then
        strings
        echo compiling tokens
        $CC $OPTS -o $BUILD/tokens.o -c $SRC/tokens.c
        TOKENS=1
    fi
}

# refs: tokens strings btrings ordstrings
TOKENVEC=0
tokenvec() {
    if [ "$TOKENVEC" -eq "0" ]; then
        tokens
        # echo generating tokenvec
        # $SRC/vec/stencil.sh \
        #     $SRC            \
        #     $BUILD          \
        #     tokens.h        \
        #     "struct Token"  \
        #     Tokenvec        \
        #     tokenvec
        echo compiling tokenvec
        $CC $OPTS -o $BUILD/tokenvec.o -c $SRC/tokenvec.c
        TOKENVEC=1
    fi
}

# refs: NONE
OPDECLVEC=0
opdeclvec() {
    if [ "$OPDECLVEC" -eq "0" ]; then
        # echo generating opdeclvec
        # $SRC/vec/stencil.sh \
        #     $SRC            \
        #     $BUILD          \
        #     opdecl.h        \
        #     "struct Opdecl" \
        #     Opdecls         \
        #     opdeclvec
        echo compiling opdeclvec
        $CC $OPTS -o $BUILD/opdeclvec.o -c $SRC/opdeclvec.c
        OPDECLVEC=1
    fi
}

# refs: NONE
TYPES=0
types() {
    if [ "$TYPES" -eq "0" ]; then
        echo compiling types
        $CC $OPTS -o $BUILD/types.o -c $SRC/types.c
        TYPES=1
    fi
}

# refs: types
TYPEREG=0
typereg() {
    if [ "$TYPEREG" -eq "0" ]; then
        types
        # echo generating typereg
        # $SRC/btree/stencil.sh \
        #     $SRC              \
        #     $BUILD            \
        #     types.h           \
        #     "struct Type *"   \
        #     32                \
        #     Type_cmp          \
        #     Type_print        \
        #     Typereg           \
        #     typereg
        echo compiling typereg
        $CC $OPTS -o $BUILD/typereg.o -c $SRC/typereg.c
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
        $CC $OPTS -o $BUILD/lexer.o -c $SRC/lexer.c
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
        $CC $OPTS -o $BUILD/opscan.o -c $SRC/opscan.c
        OPSCAN=1
    fi
}

SYMBOLS=0
symbols() {
    if [ "$SYMBOLS" -eq "0" ]; then
        strings
        echo compiling symbols
        $CC $OPTS -o $BUILD/symbols.o -c $SRC/symbols.c
        SYMBOLS=1
    fi
}

AST=0
ast() {
    if [ "$AST" -eq "0" ]; then
        symbols
        types
        typereg
        echo compiling ast
        $CC $OPTS -o $BUILD/ast.o -c $SRC/ast.c
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
        $CC $OPTS -o $BUILD/parser.o -c $SRC/parser.c
        PARSER=1
    fi
}

TYPER=0
typer() {
    if [ "$TYPER" -eq "0" ]; then
        ast
        symbols
        echo compiling typer
        $CC $OPTS -o $BUILD/typer.o -c $SRC/typer.c
        TYPER=1
    fi
}

CONVERTER=0
converter() {
    if [ "$CONVERTER" -eq "0" ]; then
        ast
        symbols
        echo compiling converter
        $CC $OPTS -o $BUILD/converter.o -c $SRC/converter.c
        CONVERTER=1
    fi
}

CODEGEN=0
codegen() {
    if [ "$CODEGEN" -eq "0" ]; then
        strings
        echo compiling codegen
        $CC $LLVM_CFLAGS $OPTS -o $BUILD/codegen.o -c $SRC/codegen.c
        CODEGEN=1
    fi
}

MTREE=0
mtree() {
    if [ "$MTREE" -eq "0" ]; then
        serene
        echo compiling mtree
        $CC $OPTS -o $BUILD/mtree.o -c $SRC/mtree.c
        MTREE=1
    fi
}

PREIMPORT=0
preimport() {
    if [ "$PREIMPORT" -eq "0" ]; then
        opscan
        mtree
        strings
        echo compiling preimport
        $CC $OPTS -o $BUILD/preimport.o -c $SRC/preimport.c
        PREIMPORT=1
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
    mtree
    tokens
    lexer
    opscan
    preimport
    ast
    parser
    typer
    # converter
    codegen
    echo compiling main
    $CC $LLVM_CFLAGS $OPTS -o $BUILD/main.o -c $SRC/main.c
    echo linking it all
    $CXX $SANITIZER -o $BUILD/main $BUILD/*.o $LLVM_CXXFLAGS
}

echo compiling with $CC
echo linking with $CXX
echo llvm cflags are $LLVM_CFLAGS
echo llvm lflags are $LLVM_CXXFLAGS

main
