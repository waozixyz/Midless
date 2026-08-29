![Image](https://i.imgur.com/4Ku3xak.png)
[![Chat](https://img.shields.io/discord/908871478576033832?label=%20chat%20on%20discord)](https://discord.gg/tZthSbpUcV)

Katalis is a voxel sandbox built in C on the
[kryon](https://github.com/kryonlabs/kryon) runtime.

Katalis began as a port of [Midless](https://github.com/Sirvoid/Midless)
by Sirvoid (MIT); the world, chunk engine and game design come from
there. The client no longer links raylib directly: the chunk renderer
runs on kryon's 3D surface tier (curated rlgl entry points plus
raymath-style math3d) and the menus use the kryon UI toolkit. The
desktop build targets OpenGL ES 2 with the GLSL 100 shader pair.

The game launches directly into a local world with a visible cursor;
the title screen is reachable from the pause menu. Networking
transports (ENet/WebSocket) and the dedicated server are not part of
this build yet.

Build: `git submodule update --init --recursive`, then `make` (binary
in `build/bin/linux/`, run it from a directory containing `textures/`,
e.g. `cd client/bin && ../../build/bin/linux/katalis-linux-x86_64`).
Debug aid: `KATALIS_DEBUG_INPUT=1` logs look-control state.

Everything below is the original Midless readme.


> **Kryon port fork** (woazixyz): this fork runs the client on
> [kryon](https://github.com/kryonlabs/kryon) instead of linking raylib
> directly. The chunk renderer uses kryon's 3D surface tier (curated rlgl
> entry points + raymath-style math3d); the menus use the kryon UI toolkit.
> The desktop build targets OpenGL ES 2 with the GLSL 100 shader pair.
> Networking transports (ENet/WebSocket) and the server are not part of
> this build yet; singleplayer local worlds work.
>
> Build: `git submodule update --init --recursive`, then `make` (binary in
> `build/bin/linux/`, run it from a directory containing `textures/`, e.g.
> `cd client/bin && ../../build/bin/linux/midless-linux-*`).
> Debug aids: `MIDLESS_AUTOPLAY=1` starts straight in a local world with a
> visible cursor, `MIDLESS_AUTOPLAY=2` keeps the gameplay cursor lock,
> `KATALIS_DEBUG_INPUT=1` logs look-control state.
>
> Everything below is the original upstream readme.

## Controls

| Input                        | Action                |
|-------------------------------|----------------------|
| W A S D             | Move                           |
| Space               | Jump                           |
| Left Click          | Break block                    |
| Right Click         | Place block                    |
| Mouse wheel         | Block Selection                |
| T                   | Open Chat                      |
| ESC                 | Open menu                      |

## Dependencies

| Dependency    | Version | Type      | Used By|
|---------------|---------|-----------|--------|
| [Raylib](https://github.com/raysan5/raylib/)        | 4.5     | Single-File | Client / Server
| [Zpl-c/ENet](https://github.com/zpl-c/enet)    | 2.3.6   | Single-File | Client / Server
| [FastNoiseLite](https://github.com/Auburn/FastNoiseLite) | -       | Single-File | Client / Server
| [stb_ds](https://github.com/nothings/stb/blob/master/stb_ds.h) | -       | Single-File | Client / Server
| [MiniLua](https://github.com/edubart/minilua) | -       | Single-File | Server
| For Optional Server's Websocket Support:
| [mongoose](https://github.com/cesanta/mongoose/) | 7.8       | Single-Files (.c, .h) | Server
| [OpenSSL](https://github.com/openssl/openssl) | -       | Linked | Server


## Compiling for Windows using MinGW

1. [Download and Build Raylib](https://github.com/raysan5/raylib/wiki/Working-on-Windows)
2. Place single-files dependencies inside /libs
4. Edit the makefile's properties if needed
3. Run mingw32-make inside the Midless folder where the MakeFile is located. 

Make arguments:
```
BUILD_SERVER=TRUE       - Build Midless Server (Doesn't build the client)
SERVER_HEADLESS=TRUE    - Compile server without graphics
SERVER_WEB_SUPPORT=TRUE - Compile server with websocket support

DEBUG=TRUE              - Debug build

PLATFORM=PLATFORM_WEB   - Build for the web (Client only)
```


## License

All code in this repository is licensed under the [MIT License](https://github.com/Sirvoid/Midless/blob/main/LICENSE).