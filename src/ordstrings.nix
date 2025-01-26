{ pkgs, serene }: import ./btree/stencil.nix {
  inherit pkgs serene;
  include-path = ./ordstrings.h;
  basetype = "struct Ordstring";
  branching = 32;
  cmp = "Ordstring_cmp";
  treename = "Btrings";
  filename = "btrings";
}
