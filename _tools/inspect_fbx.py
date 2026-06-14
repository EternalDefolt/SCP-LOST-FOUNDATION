import bpy, sys

argv = sys.argv
fbx = argv[argv.index("--") + 1]

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=fbx)

print("=== FBX INSPECT START ===")
print("fbx:", fbx)
print("\n--- Objects ---")
for o in bpy.data.objects:
    print(f"OBJ name='{o.name}' type={o.type} parent={o.parent.name if o.parent else None} scale={tuple(round(v,4) for v in o.scale)} loc={tuple(round(v,3) for v in o.location)}")
    if o.type == 'MESH':
        print(f"   verts={len(o.data.vertices)} polys={len(o.data.polygons)} vgroups={len(o.vertex_groups)}")
    if o.type == 'ARMATURE':
        arm = o.data
        print(f"   ARMATURE bones={len(arm.bones)}")
        for b in arm.bones:
            print(f"      bone '{b.name}' parent={b.parent.name if b.parent else None}")

print("\n--- Actions ---")
for a in bpy.data.actions:
    print(f"ACTION '{a.name}' frame_range={tuple(a.frame_range)} fcurves={len(a.fcurves)}")
    paths = sorted({fc.data_path.split('\"')[1] for fc in a.fcurves if '\"' in fc.data_path})
    print(f"   animated_bones({len(paths)})={paths}")

print("\n--- Scene ---")
sc = bpy.context.scene
print(f"fps={sc.render.fps} frame_start={sc.frame_start} frame_end={sc.frame_end} unit_scale={sc.unit_settings.scale_length}")
print("=== FBX INSPECT END ===")
