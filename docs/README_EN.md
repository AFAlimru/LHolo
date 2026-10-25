# LHolo

[简体中文](../README.md)

`LHolo` is a client-side structure projection mod for [LeviLamina](https://github.com/LiteLDev/LeviLamina)

## Development architecture

OreUI is being introduced feature by feature. Chat `LHolo` opens the new menu;
Browse, load, close, restore, selection editing, structure export, transforms, render settings, HUD, hotkeys, language and experimental settings are connected. OreUI scale remains a local placeholder. Selecting a file alone does not load it. Material lists are connected. The configured menu hotkey (Alt+M by default) opens the same OreUI main menu; the legacy ImGui menu has no opening entry point. See the [development guide](../DEVELOPMENT.md),
[OreUI migration architecture](OREUI_MIGRATION.md), and [frontend guide](../frontend/README.md) (Chinese).

## Versioning

Example version: `v26.51.2`

- `26.51`: the corresponding Minecraft game version
- `.2`: the mod's second version for that game version

## Quick Start

1. After installing the mod, type `LHolo` in chat for the OreUI menu, or press `Alt + M` for the existing functional menu
2. The mod is operated through a GUI and hotkeys (for the full hotkey list, just check the mod menu)
3. Please disable Vibrant Visuals while using this mod

## Main Features

**Supported files**

- Both `.mcstructure` and `.litematic` (Java Edition Litematica schematics) files

**Correction**

- Blue = projected block not placed yet
- Red = wrong block placed
- Yellow = wrong orientation or block state
- Magenta = extra block

**Layered building**

- Slice the structure along the X or Y axis
- Layer display grouped by material

**Structure transforms**

- Move
- Rotate
- Mirror

**HUD display**

- The screen corners can show: the schematic file name, the layer currently displayed, build progress (placed / total), counts of each error type, the current assisted placement mode, and more.
- The HUD can sit in any of the four screen corners, and every item can be toggled on or off individually.

**Language**

- 简体中文 (Simplified Chinese)
- English

## Showcase

**Mod menu**

![image-20260918202008186](images/image-20260918202008186.png)

**Loading a structure**

![image-20260918200440220](images/image-20260918200440220.png)

**Correction**

![image-20260918201002570](images/image-20260918201002570.png)

**Material list**

![image-20260918201318315](images/image-20260918201318315.png)

**Language setting**

How to find it: open the mod main menu -> Interface -> Language

![image-20260918202312106](images/image-20260918202312106.png)
