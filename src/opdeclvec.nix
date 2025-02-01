{ pkgs }: import ./vec/stencil.nix {
  inherit pkgs;
  include-path = ./opdecl.h;
  basetype = "struct Opdecl";
  vecname = "Opdecls";
  filename = "opdeclvec";
}
