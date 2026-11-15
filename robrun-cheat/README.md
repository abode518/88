# Rob Run External Cheat

External overlay — no DLL injection → survives EAC usermode driver scans.

## Build

MSVC 2022, x64, C++20:
1. Clone [ImGui](https://github.com/ocornut/imgui) into `vendor/imgui/`
2. Add to project: all `imgui/*.cpp` + `imgui_impl_win32.cpp` + `imgui_impl_dx11.cpp`
3. Link: `d3d11.lib dxgi.lib user32.lib`
4. Add `include/` and `vendor/imgui/` to include paths
5. Build `src/main.cpp` + `src/overlay.cpp`

## Offset Dumping (do each patch)

```
1. il2cppdumper on GameAssembly.dll → dump.cs + script.json
2. grep dump.cs for:
   - PlayerController → PlayerList static, health, team, name offsets
   - WeaponController → recoilPitch, recoilYaw, currentSpread
   - LootItem / PickupBase → transform, itemName
   - Camera.main static field + viewMatrix offset
3. Update include/offsets.h
```

## Controls

| Key     | Action          |
|---------|-----------------|
| INSERT  | toggle menu     |
| RMB     | aimbot (hold)   |
| END     | exit            |

## Features

- **ESP**: corner boxes, health bar, name + distance, loot, radar minimap
- **Aimbot**: FOV circle, smooth aim, head/chest target bone
- **Recoil control**: zeroes recoilPitch/Yaw each frame via WPM
- **External**: no injection, overlay window masquerades as generic D3D app

## Notes

- Recoil WPM only works if Rob Run is client-authoritative on recoil. If server-validated → SendInput-based recoil compensation needed instead (track pattern, counter-move mouse).
- EAC kernel-mode watches for RPM from unsigned processes on some configs. Driver-based read (kdmapper + custom driver) is the hardened path if RPM gets flagged.
