# Asset Editor

A separate program (`AssetEditor.exe`, built alongside the game) for bringing your own models and textures into the project and choosing which enemies and weapons use them.

```powershell
./run.ps1 -Editor
```

## Window

| Area | What it does |
| --- | --- |
| **Toolbar** | **Import...** opens a file browser. **New Folder**, **Delete** and **Refresh** work on the library. **Save** writes the bindings (`Ctrl+S`). |
| **Library** (left) | The folder tree of `Assets/`. `M` = model, `T` = texture, grey = companion files (`.mtl`, `.bin`). A dot means an enemy or weapon uses the file. The selected folder is where imports go. Details of the selected file are shown underneath. |
| **Viewport** (center) | Model visualizer. A library file is shown as imported. A selected enemy/weapon shows its model the way the game will place it, with guides for the facing direction, the hit cylinder and the headshot line. Weapons have a **First person** view (and **Aim**) that uses the game's viewmodel camera. Animated models get a clip picker along the bottom. Textures are shown flat. |
| **Bindings** (right) | Every enemy and weapon that can use a custom model. Select one, then set its model and texture, size, offset, rotation and animation speed. |

Viewport controls: left/right drag to orbit, middle drag (or Shift + drag) to pan, mouse wheel to zoom, `F` to frame.
Number fields: drag left/right (hold Shift for fine steps), or double click to type a value.

## Workflow

1. **Import**: press **Import...** (`Ctrl+I`), or drag files or folders from Explorer onto the window. Files go into the folder selected in the library. With the `Assets` root selected they are sorted into `Models/` and `Textures/`.
   - A `.gltf` brings its `.bin` and texture files along, and an `.obj` brings its `.mtl` and the textures that file names. Relative paths are kept so the model still loads.
   - **Import folder** in the browser (or dropping a folder) copies the folder as it is, which is the easiest way to bring in a model pack.
   - Importing a file with the same name again replaces it.
2. **Bind**: select an enemy or weapon on the right, select a model in the library, and press **Use selected** next to *Model*. You can also pick a texture to use in place of the model's own.
3. **Line it up**: by default the model is scaled to the enemy's height (or the viewmodel size for weapons) and turned 180 degrees, so a glTF model that faces +Z looks along the arrow. Adjust scale, offset and rotation until it looks right. For weapons, turn on **First person** and check both the hip and **Aim** views.
4. **Save**, then press **F8** in a running game (or restart it). Closing the editor also saves.

**Clear** puts the built-in look back. **Delete** removes files from disk and clears any bindings that used them.

## Supported files

| | Formats |
| --- | --- |
| Models | `.glb` `.gltf` `.obj` (+`.mtl`) `.iqm` `.vox` `.m3d` |
| Textures | `.png` `.jpg`/`.jpeg` `.bmp` `.tga` `.gif` `.qoi` `.psd` `.hdr` `.dds` |

These are the formats raylib loads. **FBX and Blend files are not supported**: export to `.glb` from Blender (File > Export > glTF 2.0) first. `.glb` is the best choice because it holds the mesh, its textures and its animations in one file.

When CMake downloads raylib it turns on JPG, TGA, PSD and HDR support, which raylib leaves off by default. If you use your own prebuilt `lib/libraylib.a`, those formats only work if that build has them enabled (JPG also matters for `.glb` files with JPG textures inside).

## Animation

Skeletal animations from glTF/IQM/M3D files play in game and in the viewport. Pick the clip with the `<` `>` buttons under the viewport. Enemies drive the clip from their walking speed while they move, so a walk or run clip works best. Bosses and crystals play the clip in a loop. Skinning runs on the GPU (up to 128 bones).

If a model animates with stretched or twisted parts in both the editor and the game, raylib's glTF loader is not handling that file's skeleton. Re-export it from Blender with transforms applied (Ctrl+A > All Transforms) before exporting.

## Files on disk

```
Assets/
  library.ini       which model/texture each enemy and weapon uses (written by the editor, read by the game)
  Models/           imported models (any subfolders you like)
  Textures/         imported textures
```

`library.ini` is plain text, one section per enemy or weapon (for example `[enemy.brute]` or `[weapon.shotgun]`), so it is also easy to edit by hand.

## Code layout

```
Editor/
  main.cpp              window + loop
  EditorApp             layout, toolbar, dialogs, import/delete/save actions
  EditorState.h         what the panels share (library, bindings being edited, selection, status line)
  panels/
    LibraryPanel        folder tree + file details
    ViewportPanel       model visualizer (orbit camera, guides, first person preview, animation clips)
    BindingsPanel       enemy/weapon list + the selected one's settings
    ImportDialog        file browser for picking files on disk
  library/
    LibraryTree         scans Assets/ into a tree
    Importer            copies files (and the files models depend on) into the library
  ui/
    Gui                 small immediate mode widgets (buttons, sliders, text boxes, scroll lists)
    Theme.h             colors and sizes
```

The editor shares `src/assets/` with the game, so the preview uses the same loading, placement and shader code as the game. To make another enemy or weapon skinnable, add a row to the table in `src/assets/AssetSlots.cpp` and check `AssetLibrary::HasModel` where that thing is drawn (see `src/enemies/EnemySystems.cpp`).
