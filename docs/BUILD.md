# Tuxels — How to Build Right Now

(M0, Ubuntu 24.04 — 2026-04-20)

## Dependencies

```bash
sudo apt install -y \
  build-essential cmake ninja-build pkg-config git \
  qt6-base-dev qt6-base-dev-tools libqt6opengl6-dev libqt6openglwidgets6 qt6-tools-dev qt6-tools-dev-tools \
  liblcms2-dev \
  libpng-dev libjpeg-turbo8-dev libtiff-dev libwebp-dev \
  zlib1g-dev
```

Verified versions on Ubuntu 24.04 (2026-04-20):
- Qt 6.4.2, CMake 3.28.3, g++ 13.3.0, Ninja, lcms2 2.14, libpng 1.6.43, libtiff 4.5, libwebp 1.3, zlib 1.3.

## Configure + Build

```bash
cd /home/james/Tuxels
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Optional: enable AddressSanitizer + UBSan:
```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DTUXELS_SANITIZE=ON
```

## Run

```bash
./build/tuxels
```

## Test

```bash
ctest --test-dir build --output-on-failure
```

## Manual Verify (M0 Definition of Done)

1. `./build/tuxels`
2. `File → Open…` → pick any PNG. Image appears as a single layer.
3. `Layer → New` → add a second layer.
4. Press `B` for brush; press `[`/`]` to adjust size; paint on the new layer.
5. In Layers panel, change the new layer's blend mode — observe composite updates.
6. `Layer → Add Layer Mask`; click the mask thumbnail; paint with black to hide portions.
7. `Ctrl+Z` several times to undo; `Ctrl+Shift+Z` to redo.
8. `File → Export As… PNG` → save. Reopen in any viewer to confirm.

## Common Failures

*(populated as they arise)*
