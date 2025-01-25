{
  pkgs,
  include-path,
  basetype,
  vecname,
  filename,
}: let
  include = builtins.baseNameOf include-path;
in pkgs.stdenv.mkDerivation {
  inherit basetype vecname filename include;
  name = filename;
  src = pkgs.lib.fileset.toSource {
    root = ./..;
    fileset = pkgs.lib.fileset.unions [
      ./.
      include-path
    ];
  };
  buildPhase = ''
    substituteAll $src/vec/stencil.h ./stencil.h
    substituteAll $src/vec/stencil.c ./stencil.c
    cp $src/${include} ./
    gcc -c -O2 -o ./stencil.o ./stencil.c
    ar rcs ./stencil.a ./stencil.o
  '';
  installPhase = ''
    mkdir -p $out/include
    mkdir -p $out/lib
    cp ./${include} $out/include/${include}
    cp ./stencil.h $out/include/${filename}.h
    cp ./stencil.a $out/lib/${filename}.a
  '';
}
