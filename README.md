Silly little partially vibecoded Greeting Card Generator using "LSDML" (SDL3/ImGui/C++/ffmpeg)
Turned in to mostly vibecoded Accidental Game Engine, with Godot integration

## Building Godot (Local Library)

To build the local Godot library used by this project, run the following command from the `godot` directory:

```bash
scons p=linuxbsd target=template_release library_type=shared_library builtin_sdl=no builtin_libpng=no vulkan=yes disable_path_overrides=no -j8
```
