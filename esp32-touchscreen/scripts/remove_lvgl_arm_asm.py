from pathlib import Path

Import("env")

project_dir = Path(env.subst("$PROJECT_DIR"))
pioenv = env.subst("$PIOENV")
helium_file = project_dir / ".pio" / "libdeps" / pioenv / "lvgl" / "src" / "draw" / "sw" / "blend" / "helium" / "lv_blend_helium.S"

if helium_file.exists():
    helium_file.unlink()
    print(f"Removed ARM-only LVGL assembly file: {helium_file}")

