# ConfirmHotkey — StarRupture Client Plugin


Maps a configurable hotkey (default `E`) to the primary confirm button in single-button interior UIs, so you don't have to mouse-click them after staging items.

**Currently verified:**

| Building | Blueprint widget | Button | Primary action |
|---|---|---|---|
| Recycler | `WBP_Recycler_C` | **RECYCLE** | Recycle items |
| Analyzing Station | `WBP_Analyzer_C` | **CLAIM** | Claim analysis |

Both widgets inherit from the game's generic confirm-action base class `SDK::UCrUW_Analyzer`. The plugin scans `UObject::GObjects` on each keypress, finds the first visible `UCrUW_Analyzer`-derived widget, and invokes its `ClaimButton->ButtonClicked()` (for the UI presentation — sound, animation) plus `HandleClaimClicked()` (for the gameplay effect). **Any future single-button interior UI the game ships that also inherits from `UCrUW_Analyzer` should work automatically, no plugin update required.**

**Scope rule — single-button UIs only.** The plugin deliberately avoids binding a hotkey inside any UI with multiple action buttons, because a shared hotkey in that context is unpredictable (can't know which button will fire).

**Target:** Client only. The plugin stays loaded but inactive on the dedicated server binary.

---

## Installation

1. Build `Client Release|x64` (see below) or grab the built DLL.
2. Copy `ConfirmHotkey.dll` to `<game_dir>\Binaries\Win64\ModLoader\Plugins\`. If you use the release ZIP, extract it into `<game_dir>\Binaries\Win64\ModLoader\` instead — it contains a `Plugins\` folder with `ConfirmHotkey.dll` and `ConfirmHotkey.json`; the `.json` lets the ModLoader auto-update the plugin.
3. Launch the game once — `ModLoader\Plugins\config\ConfirmHotkey.ini` is generated with defaults.
4. Edit the INI if you want a different key.

> **Requires [StarRupture-ModLoader](https://github.com/AlienXAXS/StarRupture-ModLoader) v1.23.0 or newer** (plugin interface **v75**). It is built for StarRupture **Hotfix 0.3.5** (game build CL 127004), and it also runs on the other game builds the ModLoader's version map covers (see *Game updates* below). Older ModLoaders will refuse this build: on `v1.22.x`, install the previous release, v2.4.0. If you are still on game build CL 126119, install [v2.3.0](https://github.com/NasserHabib/StarRupture-Plugin-ConfirmHotkey/releases/tag/v2.3.0) instead; it needs ModLoader v1.21.2 or newer.

> **Game updates.** The two game values this plugin reads (`UCrUW_Analyzer::ClaimButton` and `UCrUW_ActionButton::OverlayButton`) come from the ModLoader's version map for the game build that is running, not from numbers built into the DLL. After a StarRupture update that moves them, the plugin keeps working as soon as the ModLoader knows the new build, with no new ConfirmHotkey release. If the ModLoader cannot supply them for a build, the plugin does not act on it, rather than reading the wrong memory.

> **Upgrading by hand?** Older ModLoader versions used `Binaries\Win64\Plugins\`. The ModLoader still moves files from that folder into `ModLoader\Plugins\` when the game starts, but it does not replace a file that is already there. Put the new DLL directly in `ModLoader\Plugins\`.

## Config (`ModLoader\Plugins\config\ConfirmHotkey.ini`)

```ini
[General]
Enabled=true

[PluginSettings]
ConfirmHotkey=E
```

The hotkey is registered through the ModLoader's `IPluginInputEvents::RegisterKeybindByName`, which accepts plain key names and combo strings:

| Value | Effect |
|---|---|
| `E` | Bare key |
| `F5` | Function key |
| `Ctrl+E` | Modifier combo |
| `Shift+F5` | Modifier combo |
| `Ctrl+Shift+Delete` | Multi-modifier combo |

Modifier tokens (`Ctrl` / `Shift` / `Alt`) are case-insensitive. A bare key (e.g. `E`) fires only when **no** modifier is held, so it will not double-fire on the game's own `Shift+E`; if you want a combo, configure the combo. The plugin's schema declares this entry as `ConfigValueType::Keybind`, so the in-game plugin config menu renders a keybind-picker — you can rebind without editing the INI, and the change takes effect immediately (the ModLoader auto re-registers the binding).

> **Upgrading from an older build?** The config key was renamed from `RecycleHotkey` to `ConfirmHotkey`. `InitializeFromSchema` will add the new key automatically on first launch after upgrade; the old `RecycleHotkey=...` line remains in the INI as an inert dangling entry (nothing reads it). Delete it if you want a clean file.

## How it works

1. `OnPluginLoadHooks` asks the ModLoader for the `ClaimButton` and `OverlayButton` offsets of the running game build. `PluginInit` then takes the SDK globals from the loader, stores `IPluginSelf*`, inits config and verifies the client binary.
2. `ModCore::Initialize` registers the configured hotkey via `Input->RegisterKeybindByName(..., EModKeyEvent::Pressed, ...)`. The ModLoader (interface v30+) tracks the registration and automatically re-registers the keybind when the user rebinds it in the in-game config UI — no manual config-change subscription needed.
3. On keypress, the callback walks `UObject::GObjects` in a single pass, filtering to non-CDO objects whose class `IsA SDK::UCrUW_Analyzer`. The first one whose `UWidget::IsVisible()` returns true wins — `ClaimButton->ButtonClicked()` fires for the sound and animation, then `HandleClaimClicked()` fires for the gameplay effect. Both paths are SEH-wrapped so a mid-teardown widget can't take the game down.
4. The success log captures the widget's actual Blueprint class name (`WBP_Recycler_C`, `WBP_Analyzer_C`, …) so you can tell from the log which building fired.
5. `PluginShutdown` unregisters the keybind and clears `IPluginSelf`.

No low-level hooks, no pattern scanning — pure typed-hook consumer. The only game memory it reads directly is the two members above, at the offsets the ModLoader gives it.

## Extending to new UIs

**Happy path (likely works with zero code changes):** the game adds a new single-button interior UI, its Blueprint widget inherits from `UCrUW_Analyzer`, and it follows the same `ClaimButton` + `HandleClaimClicked` convention. The plugin's `IsA` match picks it up automatically — just open that UI, press your hotkey, and check the log for a `Confirmed action on WBP_<new>_C` line. Add the building's row to the "Currently verified" table when you confirm it.

**If it doesn't inherit from `UCrUW_Analyzer`:** find the base class via the `[diag]` log (the plugin logs every `IsA` match at DEBUG level with the actual class name). Then add a second branch in `ModCore::OnConfirmHotkey`:

1. Confirm the new widget class has **exactly one** `Handle*Clicked` method in the Dumper-7 SDK. Multi-button widgets are out of scope by design — the existing single-button rule makes the hotkey semantics predictable.
2. Verify the class declaration is reachable from an `#include` already in `ModCore.cpp` (or add the relevant header + `_functions.cpp`).
3. Add a parallel `SafeInvokeX` helper — mirror `SafeInvokeClaim`'s shape: `ButtonClicked()` on the wrapper (if any) plus the direct handler call, both inside a single `__try`/`__except`.
4. Add a second `if (Obj->IsA(SDK::U<NewBase>::StaticClass())) { ... }` block inside the GObjects scan loop.
5. If target count reaches ~4 distinct base classes, refactor the scan loop into a `HotkeyTarget[]` table.

## Building from source

Requires Visual Studio 2022 with the v143 toolset and the [StarRupture-Plugin-SDK](https://github.com/AlienXAXS/StarRupture-Plugin-SDK) (with the `StarRupture SDK` submodule initialized).

Open `ConfirmHotkey.sln` directly and build the `Client Release|x64` configuration. The standalone solution imports a local `Shared.props` whose SDK path defaults point at a sibling `..\StarRupture-Plugin-SDK\` checkout, so if you have the two repos side-by-side the build works with no extra setup. If your SDK lives elsewhere, override the paths on the MSBuild command line:

```
msbuild ConfirmHotkey.sln /p:Configuration="Client Release" /p:Platform=x64 ^
  /p:GameSDKRoot="C:\path\to\StarRupture-Game-SDK" ^
  /p:PluginSDKInclude="C:\path\to\StarRupture-Plugin-SDK\include"
```

Output: `build\Client Release\Plugins\ConfirmHotkey.dll`.

`Shared.props` records the game build the Game SDK was generated for (CL 127004) so the DLL can tell the ModLoader which build it was compiled against. If you build against a Game SDK for another build, pass `/p:PluginGameBuiltCL=<CL>`.

Only `Client Debug` and `Client Release` configurations exist — the plugin is client-only by design (`plugin.cpp` rejects non-client host binaries at init time).

## Files

- `plugin.{h,cpp}` — the ABI exports (including `OnPluginLoadHooks`) + `IPluginSelf` lifecycle + client-binary guard.
- `ModCore.{h,cpp}` — hotkey registration, map-resolved `ClaimButton`/`OverlayButton` reads, GObjects scan, per-target invocation.
- `plugin_config.h` — INI schema + typed accessors (`ConfirmHotkeyConfig::Config`).
- `plugin_helpers.h` — `LOG_*` macros and `GetHooks/GetConfig` wrappers.
- `dllmain.cpp` — stock Windows DLL entry.

## Disclaimer

Use at your own risk. Learning project — no warranty, no support.
