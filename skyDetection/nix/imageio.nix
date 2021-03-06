# https://github.com/NixOS/nixpkgs/blob/nixos-21.11/pkgs/development/python-modules/imageio/default.nix#L40

{ lib
, buildPythonPackage
, isPy27
, fetchPypi
, imageio-ffmpeg
, numpy
, pillow
, psutil
, pytestCheckHook
, callPackage
}:

buildPythonPackage rec {
  pname = "imageio";
  version = "2.9.0";
  disabled = isPy27;

  src = fetchPypi {
    sha256 = "52ddbaeca2dccf53ba2d6dec5676ca7bc3b2403ef8b37f7da78b7654bb3e10f0";
    inherit pname version;
  };

  propagatedBuildInputs = [
    imageio-ffmpeg
    numpy
    (callPackage ./pillow.nix {}) #pillow
  ];

  # checkInputs = [
  #   psutil
  #   pytestCheckHook
  # ];

  doCheck = false;

  preCheck = ''
    export IMAGEIO_USERDIR="$TMP"
    export IMAGEIO_NO_INTERNET="true"
    export HOME="$(mktemp -d)"
  '';

  meta = with lib; {
    description = "Library for reading and writing a wide range of image, video, scientific, and volumetric data formats";
    homepage = "http://imageio.github.io/";
    license = licenses.bsd2;
  };
}
