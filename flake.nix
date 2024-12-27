{
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.11";
  outputs = { self, nixpkgs, ... }: let
    system = "x86_64-linux";
    name = "cara";
    src = ./src;
    pkgs = (import nixpkgs) { inherit system; };
  in {
    packages."${system}" = let
      commonBuildInputs = [ pkgs.gcc ];
      installPhase = ''
        mkdir -p "$out/bin"
        cp ./main "$out/bin/${name}"
      '';
    in {
      debug = pkgs.stdenv.mkDerivation {
        inherit name src installPhase;
        dontStrip = true;
        nativeBuildInputs = commonBuildInputs;
        buildInputs = [ pkgs.gdb ];
        buildPhase = ''
          gcc "$src/main.c" -Wall -Wextra -g -o ./main
        '';
      };
      default = pkgs.stdenv.mkDerivation {
        inherit name src installPhase;
        nativeBuildInputs = commonBuildInputs;
        buildPhase = ''
          gcc "$src/main.c" -O2 -o ./main
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
  };
}
