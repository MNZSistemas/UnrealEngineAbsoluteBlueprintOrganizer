# Unreal Engine Absolute Blueprint Organizer

Organizes a Blueprint with one shortcut: layered left-to-right layout, a comment box per event or
function, hygiene (orphan nodes, no-op reroutes, duplicate casts) and member sorting/renaming, all
in a single undoable transaction. A report tab shows before/after statistics and refactoring
suggestions.

UEABO is free and complete: every feature is included, nothing is locked. It is free on Fab.

Editor-only, Win64. Unreal Engine 4.27, 5.0, 5.5, 5.6, 5.7 and 5.8.

## Install

Copy the `UnrealEngineAbsoluteBlueprintOrganizer` folder into your project's `Plugins` folder (or
the engine's `Engine/Plugins/Marketplace`), open the project and enable the plugin under
Edit > Plugins if it is not already enabled.

## Shortcuts (in the Blueprint editor)

| Shortcut | Action |
|---|---|
| Ctrl+Alt+L | Organize the focused graph |
| Ctrl+Alt+Shift+L | Organize every graph of the Blueprint and its members |

The toolbar "Organize" button offers the same actions plus the report. Shortcuts and every rule are
in Editor Preferences > Plugins > Unreal Engine Absolute Blueprint Organizer (a shortcut change
needs an editor restart). Ctrl+Z undoes a whole organize.

## Building from source

```powershell
powershell -NoProfile -File Tools\build_plugin.ps1 -Engine <v> [-NoInstall] [-Package]   # v = 4.27, 5.0, 5.5, 5.6, 5.7, 5.8
powershell -NoProfile -File Tools\run_tests.ps1 -Engine <v>                             # automation tests "Ueabo.*"
```

Or manually: `RunUAT.bat BuildPlugin -Plugin=<path>\UnrealEngineAbsoluteBlueprintOrganizer.uplugin -Package=<out> -TargetPlatforms=Win64`.

## Documentation and support

- Docs: https://ueabo.mnzsistemas.com
- Support: mnzsistemas@gmail.com

Related products: Unreal Engine Absolute MCP (https://ueamcp.mnzsistemas.com) and
Unreal Engine Absolute Project Migrator (https://ueapm.mnzsistemas.com).

License: MIT (see LICENSE). Copyright (c) 2026 MNZ Sistemas. All rights reserved.
