{
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.11";
  inputs.serene.url = "github:artemisYo/serene";
  inputs.serene.inputs.nixpkgs.follows = "nixpkgs";
  outputs = { self, nixpkgs, serene, ... }: let
    system = "x86_64-linux";
    name = "cara";
    src = ./src;
    pkgs = (import nixpkgs) { inherit system; };
    serene-drv = serene.packages."${system}".default;
    commonBuildInputs = [ serene-drv pkgs.gcc pkgs.libllvm ];
    instances = [
      (import ./src/tokenvec.nix { inherit pkgs; })
      (import ./src/opdeclvec.nix { inherit pkgs; })
      (import ./src/ordstrings.nix { inherit pkgs; serene = serene-drv; })
      (import ./src/types.nix { inherit pkgs; serene = serene-drv; })
    ];
  in {
    packages."${system}" = let
      # expects a .c file of same name in $src/
      modules = [
        "main"
        "tokens"
        "lexer"
        "ast"
        "types"
        "typer"
        "converter"
        "strings"
        "parser"
        "symbols"
        "ordstrings"
        "codegen"
      ];
      debugOpts = "-Wall -Wextra -g -O0";
      releaseOpts = "-O2";
      
      installPhase = ''
        mkdir -p "$out/bin"
        cp ./main "$out/bin/${name}"
      '';
    in {
      debug = pkgs.stdenv.mkDerivation {
        inherit name src installPhase;
        dontStrip = true;
        buildInputs = commonBuildInputs ++ instances;
        buildPhase =
          "cc `llvm-config --cflags` -c " + debugOpts
          # "gcc -o ./main " + debugOpts
          + pkgs.lib.concatStrings (map (m: " $src/${m}.c") modules)
          # + pkgs.lib.concatStrings (map (d: " ${d}/lib/*") instances)
          # + " ${serene-drv}/lib/*; "
          + "; "
          + "c++ `llvm-config --cxxflags --ldflags --libs core analysis target --system-libs` -o ./main ./*.o "
          + pkgs.lib.concatStrings (map (d: " ${d}/lib/*") instances)
          + " ${serene-drv}/lib/*"
        ;
      };
      default = pkgs.stdenv.mkDerivation {
        inherit name src installPhase;
        nativeBuildInputs = commonBuildInputs ++ instances;
        buildPhase = 
          "cc `llvm-config --cflags` -c " + releaseOpts
          # "gcc -o ./main " + releaseOpts
          + pkgs.lib.concatStrings (map (m: " $src/${m}.c") modules)
          # + pkgs.lib.concatStrings (map (d: " ${d}/lib/*") instances)
          # + " ${serene-drv}/lib/*; "
          + "; "
          + "c++ `llvm-config --cxxflags --ldflags --libs core analysis target --system-libs` -o ./main ./*.o "
          + pkgs.lib.concatStrings (map (d: " ${d}/lib/*") instances)
          + " ${serene-drv}/lib/*"
        ;
      };
    };
    apps."${system}" = {
      debug = {
        type = "app";
        program = "${self.packages."${system}".debug}/bin/${name}";
      };
      default = {
        type = "app";
        program = "${self.packages."${system}".default}/bin/${name}";
      };
    };
    devShells."${system}" = {
      default = pkgs.stdenv.mkDerivation {
        inherit name;
        src = ./.;
        buildInputs = [ pkgs.clang-tools pkgs.gdb ] ++ commonBuildInputs ++ instances;
      };
    };
  };
}
