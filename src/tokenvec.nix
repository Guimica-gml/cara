{pkgs} : import./ vec / stencil.nix {
    inherit pkgs;
    include - path =./ tokens.h;
    basetype = "struct Token";
    vecname = "Tokenvec";
    filename = "tokenvec";
}
