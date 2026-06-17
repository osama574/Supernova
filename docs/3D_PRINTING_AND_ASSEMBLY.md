# 3D Printing And Physical Assembly

The case files are in:

```text
3D-Model/
```

Files:

```text
3D-Model/pantilt_floor_nolegs.stl
3D-Model/pantilt_leg.stl
3D-Model/pantilt_screenpanel.stl
```

## Recommended Print Settings

These are safe starting settings for a normal FDM printer:

| Setting | Recommended Value |
| --- | --- |
| Material | PLA+ or PETG |
| Layer height | 0.20 mm |
| Walls/perimeters | 3 or 4 |
| Infill | 20% to 35% |
| Top/bottom layers | 4 or more |
| Supports | Enable if your slicer shows unsupported overhangs |
| Brim | Recommended for tall or narrow parts |

PETG is better if the device may sit in a warm place. PLA is easier for first prints.

## Print Checklist

1. Open the three STL files in your slicer.
2. Check that each model is flat on the build plate.
3. Slice with the settings above.
4. Print one part first and test fit before printing replacements.
5. Remove supports carefully.
6. Clean screw holes and cable paths with a small tool if needed.

## Mechanical Assembly Order

1. Place the base part `pantilt_floor_nolegs.stl` on the table.
2. Attach the leg/support part `pantilt_leg.stl` to the base.
3. Attach the screen panel part `pantilt_screenpanel.stl` to the support.
4. Test fit the touchscreen display before tightening everything.
5. Route wires so they do not rub against moving servo parts.
6. Mount servos only after checking their center position in software.

## Servo Centering Before Final Assembly

Before physically attaching arms/brackets to the servos:

1. Flash the touchscreen firmware.
2. Power the touchscreen and servo power supply.
3. Open the Motor app.
4. Let both servos move to the firmware center position.
5. Attach the mechanical servo horns/brackets while the servos are centered.

The firmware starts the servos at:

```text
Pan 135 degrees
Tilt 135 degrees
```

The software limit range is:

```text
5 degrees to 265 degrees
```

## Cable Routing

- Keep servo power wires away from the touch/display ribbon cables.
- Use strain relief for servo and LED wires.
- Do not leave exposed 5V or servo supply wires near the ESP32 pins.
- Leave access to the touchscreen USB-C port for flashing.
- Leave access to the ESP32-CAM USB/programmer port for flashing.

## Final Mechanical Check

Before powering everything together:

- Display is firmly mounted.
- Servo arms can move without hitting the case.
- LED ring wires cannot be pulled loose.
- Servo supply polarity is correct.
- All grounds are connected together.
- No loose wires can short against the display board.

