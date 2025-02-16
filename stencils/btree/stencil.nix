{
  pkgs,
  serene,
  include-path,
  basetype,
  branching,
  cmp,
  print,
  treename,
  filename,
}: let
  include = builtins.baseNameOf include-path;
in pkgs.stdenv.mkDerivation {
  inherit basetype branching cmp print treename filename include;
  name = filename;
  src = pkgs.lib.fileset.toSource {
    root = ./..;
    fileset = pkgs.lib.fileset.unions [
      ./.
      include-path
    ];
  };
  dontStrip = true;
  buildInputs = [ serene ];
  buildPhase = ''
    substituteAll $src/btree/stencil.h ./${filename}.h
    substituteAll $src/btree/stencil.c ./stencil.c
    cp $src/${include} ./
    gcc -c -O0 -g -o ./stencil.o ./stencil.c ${serene}/lib/*
    ar rcs ./stencil.a ./stencil.o
  '';
  installPhase = ''
    mkdir -p $out/include
    mkdir -p $out/lib
    cp ./${include} $out/include/${include}
    cp ./${filename}.h $out/include/${filename}.h
    cp ./stencil.a $out/lib/${filename}.a
  '';
}
