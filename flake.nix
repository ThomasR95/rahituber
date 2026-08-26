{
  description = "flake devShell template";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = import nixpkgs {
          inherit system;
        };
      in
      with pkgs;
      {
        #formatter.${system} = nixpkgs.legacyPackages.${system}.nixfmt-tree;

        devShells.default = mkShell {
          packages = [
            gcc
            cmake
            pkgconf
          ];
          inputsFrom = [ ]; # build dependencies of the listed derivations
          buildInputs = [
            portaudio
            freetype
            sfml
            tinyxml
            libGL
            libGLX
            libGLU
            imgui
            libX11
            alsa-lib
            mongoose
          ]; # libraries
        };
      }
    );
}
