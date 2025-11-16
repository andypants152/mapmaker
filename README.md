# Mapmaker (Switch Doom Editor)

Controller-driven mapmaker/playtest tool for a simple Doom-like prototype. Runs on desktop via SDL2/OpenGL and on Nintendo Switch via libnx, sharing the same assets under `romfs/data`.

## Features
- Top-down grid editing with snapping, vertex/line creation, and automatic sector detection.
- Rebuilds a textured 3D mesh from 2D sectors and lets you drop into a first-person playtest instantly.
- Entity placement for player starts, enemy wizards/spawners, and item pickups.
- Shared data layout: desktop builds copy `romfs/data` to `data/` for asset loading; Switch builds mount `romfs:/data/`.

## Controls (gamepad)
- Left stick: move cursor (edit) / move player (play).
- Right stick: pan camera (edit) / look around (play).
- R / L shoulder: zoom in/out in edit mode; L shoulder also blocks projectiles in play mode.
- A: place vertex/entity at the cursor.
- B: start/extend a wall chain or select the hovered entity.
- X: delete hovered vertex/line/entity.
- D-Pad Up: enter entity mode or cycle the active entity brush.
- D-Pad Down: leave entity mode.
- Minus/Back: toggle playtest mode; Plus/Start: quit.

## Building
Desktop (SDL2 + OpenGL + GLAD + libpng + zlib):
```sh
make desktop
./mapmaker_desktop
```

Nintendo Switch (devkitA64 + libnx, assets packed into the NRO):
```sh
export DEVKITPRO=<path to devkitpro>
make switch
```

Works best with a gamepad; there is no keyboard/mouse path wired up at the moment.
