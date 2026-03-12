# Doom for CubeOS

This directory now contains the CubeOS-focused Doom port only.

The shared game code lives in `doomgeneric/`. The CubeOS-specific glue lives in `src/`. Alternate platform backends, legacy project files, and old screenshots were removed so this tree only carries code and documentation needed for the CubeOS build.

## Build

```sh
make -C apps/doom build
```

The resulting binary is written to `build/apps/doom/DOOM.ELF`.

## Runtime data

You still need a Doom IWAD. The current port looks for WAD files in the working directory and under `assets/`.

## Port structure

- `apps/doom/doomgeneric`: shared Doom engine code kept for the CubeOS port.
- `apps/doom/src/doom_cubeos.c`: CubeOS window/input integration.
- `apps/doom/src/w_file_cubeos.c`: CubeOS WAD file backend.
- `apps/doom/src/newlib_stubs.c`: runtime and filesystem stubs used by the port.
