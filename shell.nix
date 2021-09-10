# Tip: direnv to keep dependencies for a specific project in Nix
# Run: nix-shell

{ pkgs ? import (builtins.fetchTarball { # https://nixos.wiki/wiki/FAQ/Pinning_Nixpkgs :
  # Descriptive name to make the store path easier to identify
  name = "nixos-unstable-2020-09-03";
  # Commit hash for nixos-unstable as of the date above
  url = "https://github.com/NixOS/nixpkgs/archive/702d1834218e483ab630a3414a47f3a537f94182.tar.gz";
  # Hash obtained using `nix-prefetch-url --unpack <url>`
  sha256 = "1vs08avqidij5mznb475k5qb74dkjvnsd745aix27qcw55rm0pwb";
}) { }}:
with pkgs;

let
  # my-python-packages = python39.withPackages(ps: with ps; [
    
  # ]);
in
mkShell {
  buildInputs = [ #my-python-packages
    opencv clang pkgconfig libpng

    #bear # Optional, for generating emacs compile_commands.json

    llvmPackages.libstdcxxClang
    #ginac # https://www.ginac.de/tutorial/#A-comparison-with-other-CAS
    
    #(callPackage ./webscraping.nix {})
  ]; # Note: for macos need this: write this into the path indicated:
  # b) For `nix-env`, `nix-build`, `nix-shell` or any other Nix command you can add
  #   { allowUnsupportedSystem = true; }
  # to ~/.config/nixpkgs/config.nix.
  # ^^^^^^^^^^^^^^^^^^ This doesn't work, use `brew install cartr/qt4/pyqt` instead.
}
