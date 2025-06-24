# RahiTuber

For info, please see the itch.io page: https://rahisaurus.itch.io/rahituber

This project resolves many dependencies via submodule. When cloning you should use `git clone --recurse-submodules`.
Otherwise use `git submodule update --remote`, as without this the project will not build correctly.


To build create a directory in the project root to build from (`build` is the convention)
and from that directory use CMake to generate the project files.


The following dependencies are included as submodules:
  - SFML 2.6.1
  - imgui
  - imgui-sfml (Included in-tree with my modifications)
  - portaudio
  - mongoose
  - tinyxml2
  - Spout2 (Only used on Win32)


Mandatory Linux dependencies(apt packages included in parentheses):
  - LibX11 (libx11-dev)
  - libXrandr (libxrandr-dev)
  - libXcursor (libxcursor-dev)
  - libuuid (uuid-dev)
  - OpenGL (freeglut3-dev)
  - libudev (libudev-dev)
  - OpenAL (libopenal-dev)
  - libogg (libogg-dev)
  - libvorbis (libvorbis-dev)
  - libflac (libflac-dev)


Optional Linux dependencies (functionality may not work without these):
  - libpng (libpng-dev)
  - zlib (zlib1g-dev)
  - libbz2 (libbz2-dev)
  - alsa-lib (libasound2-dev)
  - libjack (libjack-dev)
  - libbrotli (libbrotli-dev)
 