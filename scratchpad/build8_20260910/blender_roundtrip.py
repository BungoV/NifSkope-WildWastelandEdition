"""Lane BUILD8, THE THIRD-PARTY LEG: import our .gltf into Blender and export
it straight back out, headless, changing nothing.

This is the trip bungo will do by hand -- our export -> Blender -> our import.
Blender's glTF importer and exporter are Khronos's reference Python
implementation shipped with the application, so neither end is our code.

  "E:/Tools/3D/Blender 4.5/blender.exe" --background --factory-startup \
      --python scratchpad/build8_20260910/blender_roundtrip.py -- IN.gltf OUT.gltf

`--factory-startup` so the user's add-ons and preferences cannot change the
answer.  Everything Blender CHANGED that we can see from inside Blender is
printed as `BL <key> <value>`: the scene frame rate, the frame range, the unit
scale and length unit, the armature's bone count, and each action's channel
count -- because "what did Blender do to it" is half of what this leg is for.

The export is deliberately as plain as the operator allows:
  export_format='GLTF_SEPARATE'   .gltf + .bin, the same shape we wrote
  export_animations=True, export_frame_range=False (the WHOLE action)
  export_yup=True                 Blender's default and ours
  export_apply=False              no modifier baking
"""
import sys

import bpy

argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
if len(argv) < 2:
    print("FAIL: usage -- IN.gltf OUT.gltf")
    sys.exit(2)
src, dst = argv[0], argv[1]

bpy.ops.wm.read_factory_settings(use_empty=True)
# --fps N BEFORE the import: Blender's glTF importer lays the animation onto the
# SCENE's frame grid, and the factory scene is 24 fps.  A 30 fps clip imported
# at 24 and exported again comes back re-timed -- measured 2026-09-10, lane
# BUILD8: 0.29 units / 4.83 degrees of pure Blender resampling.  Setting the
# scene rate to the clip's own rate is the cure, and it is what a user has to
# do by hand.
if "--fps" in argv:
    fps = float(argv[argv.index("--fps") + 1])
    bpy.context.scene.render.fps = int(round(fps))
    bpy.context.scene.render.fps_base = 1.0
    print("BL forced_scene_fps %s" % fps)

try:
    bpy.ops.import_scene.gltf(filepath=src)
except Exception as e:                                  # noqa: BLE001
    print("FAIL: Blender refused %s: %s" % (src, e))
    sys.exit(1)

sc = bpy.context.scene
print("BL in %s" % src)
print("BL scene_fps %s" % (sc.render.fps / sc.render.fps_base))
print("BL frame_start %d" % sc.frame_start)
print("BL frame_end %d" % sc.frame_end)
print("BL unit_system %s" % sc.unit_settings.system)
print("BL unit_scale_length %s" % sc.unit_settings.scale_length)
print("BL length_unit %s" % sc.unit_settings.length_unit)
print("BL objects %d" % len(bpy.data.objects))
for ob in bpy.data.objects:
    if ob.type == "ARMATURE":
        print("BL armature %s bones %d" % (ob.name, len(ob.data.bones)))
        # bone roll is Blender's own invention: glTF has no such thing, so a
        # non-zero roll on re-export is Blender's edit-bone reconstruction
        rolls = 0
        try:
            bpy.context.view_layer.objects.active = ob
            bpy.ops.object.mode_set(mode="EDIT")
            for eb in ob.data.edit_bones:
                if abs(eb.roll) > 1e-6:
                    rolls += 1
            bpy.ops.object.mode_set(mode="OBJECT")
        except Exception as e:                          # noqa: BLE001
            print("BL roll_read_failed %s" % e)
        print("BL bones_with_nonzero_roll %d" % rolls)
print("BL meshes %d" % len(bpy.data.meshes))
for a in bpy.data.actions:
    print("BL action %s range %.4f..%.4f fcurves %d"
          % (a.name, a.frame_range[0], a.frame_range[1], len(a.fcurves)))

try:
    bpy.ops.export_scene.gltf(
        filepath=dst,
        export_format="GLTF_SEPARATE",
        export_animations=True,
        export_frame_range=False,
        export_yup=True,
        export_apply=False,
        use_selection=False,
    )
except Exception as e:                                  # noqa: BLE001
    print("FAIL: Blender's exporter refused: %s" % e)
    sys.exit(1)
print("BL out %s" % dst)
print("BLENDER-ROUNDTRIP OK")
