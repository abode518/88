# بناء الشيت — خطوة بخطوة

## متطلبات
- Visual Studio 2022 (مع C++ desktop tools)
- CMake 3.20+
- Git

## خطوات البناء

```bat
:: 1. جلب ImGui
git clone https://github.com/ocornut/imgui vendor/imgui

:: 2. بناء
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release

:: الناتج: build\Release\RobRunCheat.exe
```

## قبل التشغيل — لازم تعبّي الـ Offsets

```
1. شغّل اللعبة
2. شغّل il2cppdumper على GameAssembly.dll
   → ينتج dump.cs + script.json
3. افتح dump.cs وابحث عن:
   - "class PlayerController"  → offsets.h
   - "class WeaponController"  → offsets.h
   - "class PlayerEconomy"     → features.h OffExt::Money
   - "Camera" static field     → offsets.h CameraStaticField
4. استخدم Cheat Engine أو x64dbg للتحقق من كل offset
```

## تقوية الشيت أكثر (اختياري)

### تجاوز EAC kernel-level RPM detection
```
1. جلب kdmapper: github.com/TheCruZ/kdmapper
2. اكتب driver صغير يعمل RPM/WPM عبر ioctl
3. شغّل READ_MODE=MODE_DRIVER في CMakeLists.txt
4. استخدم kdmapper لتحميل الـ driver قبل الشيت
```

### مؤشرات الكشف اللي تجنبها
| الموضوع | الحل المطبق |
|---------|------------|
| DLL injection | ✅ ما في injection |
| RPM من process غير موقّع | ⚠️ استخدم driver mode |
| String scanning | ✅ XOR compile-time encryption |
| Debugger attach | ✅ anti-debug watchdog |
| Screenshot overlay detect | ✅ corner boxes بدل full rect |
| Timing analysis | ✅ random sleep بكل فريم |
| Config on disk | ✅ XOR encrypted في Microsoft folder |

## مفاتيح التشغيل
| مفتاح | وظيفة |
|-------|--------|
| INSERT | إخفاء/إظهار القائمة |
| RMB (زر يمين الماوس) | aimbot (hold) |
| END | إغلاق الشيت |
