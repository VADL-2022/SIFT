# Tip: direnv to keep dependencies for a specific project in Nix
# Run: nix-shell

{ useGtk ? true,
  pkgs ? import (builtins.fetchTarball { # https://nixos.wiki/wiki/FAQ/Pinning_Nixpkgs :
  # Descriptive name to make the store path easier to identify
  name = "nixos-unstable-2020-09-03";
  # Commit hash for nixos-unstable as of the date above
  url = "https://github.com/NixOS/nixpkgs/archive/702d1834218e483ab630a3414a47f3a537f94182.tar.gz";
  # Hash obtained using `nix-prefetch-url --unpack <url>`
  sha256 = "1vs08avqidij5mznb475k5qb74dkjvnsd745aix27qcw55rm0pwb";
}) { }}:
with pkgs;

let
  opencvGtk = opencv4.override (old : { enableGtk2 = true; }); # https://stackoverflow.com/questions/40667313/how-to-get-opencv-to-work-in-nix , https://github.com/NixOS/nixpkgs/blob/master/pkgs/development/libraries/opencv/default.nix
  my-python-packages = python39.withPackages(ps: with ps; [
    (lib.optional stdenv.hostPlatform.isMacOS opencv4)
    numpy
    matplotlib
  ]);

  # Actual package is https://github.com/NixOS/nixpkgs/blob/nixos-21.05/pkgs/development/compilers/llvm/7/libunwind/default.nix#L45
  libunwind_modded = llvmPackages.libunwind.overrideAttrs (oldAttrs: rec {
      nativeBuildInputs = oldAttrs.nativeBuildInputs ++ [ fixDarwinDylibNames ];
  });
in
mkShell {
  buildInputs = [ my-python-packages
                ] ++ (lib.optional (stdenv.hostPlatform.isMacOS || !useGtk) [ opencv4 ])
  ++ (lib.optional (stdenv.hostPlatform.isLinux && useGtk) [ (python39Packages.opencv4.override { enableGtk2 = true; })
                                                                 opencvGtk
                                                               ]) ++ [
    clang_12 # Need >= clang 10 to fix fast-math bug (when using -Ofast) ( https://bugzilla.redhat.com/show_bug.cgi?id=1803203 )
    pkgconfig libpng
    #] ++ (lib.optional (stdenv.hostPlatform.isLinux) lldb) ++ [

    #bear # Optional, for generating emacs compile_commands.json

    # For stack traces #
    (callPackage ./backward-cpp.nix {}) # https://github.com/bombela/backward-cpp
    ] ++ (lib.optional (stdenv.hostPlatform.isMacOS) libunwind_modded) ++ (lib.optional (stdenv.hostPlatform.isLinux) libunwind) ++ [
    # #

    llvmPackages.libstdcxxClang
    #ginac # https://www.ginac.de/tutorial/#A-comparison-with-other-CAS
    
    #(callPackage ./webscraping.nix {})
  ]; # Note: for macos need this: write this into the path indicated:
  # b) For `nix-env`, `nix-build`, `nix-shell` or any other Nix command you can add
  #   { allowUnsupportedSystem = true; }
  # to ~/.config/nixpkgs/config.nix.
  # ^^^^^^^^^^^^^^^^^^ This doesn't work, use `brew install cartr/qt4/pyqt` instead.
  
  shellHook = ''
    export INCLUDE_PATHS_FROM_CFLAGS=$(./makeIncludePathsFromCFlags.sh)
  '';
}
