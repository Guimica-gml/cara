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
  in {
    packages."${system}" = let
      # expects a .c file of same name in $src/
      modules = [
        "main"
        "tokens"
        "lexer"
        "ast"
        "typer"
        "converter"
        "strings"
        "parser"
        "symbols"
        "runner"
      ];
      debugOpts = "-Wall -Wextra -g -O0";
      releaseOpts = "-O2";
      
      comp = (isDebug:
        "gcc -c"
        + " " + (if isDebug then debugOpts else releaseOpts)
        + " " + pkgs.lib.concatStrings
          (pkgs.lib.intersperse " "
            (map (m: "$src/" + m + ".c") modules))
      );
      link = (isDebug:
        "gcc -o ./main"
        + " " + (if isDebug then debugOpts else releaseOpts)
        + " " + pkgs.lib.concatStrings
          (pkgs.lib.intersperse " "
            (map (m: "./" + m + ".o") modules))
        + " ${serene-drv}/lib/*"
      );
      installPhase = ''
        mkdir -p "$out/bin"
        cp ./main "$out/bin/${name}"
      '';
    in {
      debug = pkgs.stdenv.mkDerivation {
        inherit name src installPhase;
        dontStrip = true;
        buildInputs = commonBuildInputs;
        buildPhase = ''
          ${comp true}
          ${link true}
        '';
      };
      default = pkgs.stdenv.mkDerivation {
        inherit name src installPhase;
        nativeBuildInputs = commonBuildInputs;
        buildPhase = ''
          ${comp false}
          ${link false}
        '';
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
        buildInputs = [ pkgs.clang-tools pkgs.gdb ] ++ commonBuildInputs;
      };
    };
  };
}
