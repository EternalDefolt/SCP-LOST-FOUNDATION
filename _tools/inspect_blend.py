import bpy

print("=== BLEND INSPECT START ===")
print("blend file:", bpy.data.filepath)
print("Blender version:", bpy.app.version_string)

print("\n--- Objects ---")
for o in bpy.data.objects:
    print(f"OBJ name='{o.name}' type={o.type} parent={o.parent.name if o.parent else None}")
    if o.type == 'MESH':
        me = o.data
        print(f"   verts={len(me.vertices)} polys={len(me.polygons)} materials={[m.name if m else None for m in me.materials]}")
        print(f"   vertex_groups={[vg.name for vg in o.vertex_groups][:60]}")
        for mod in o.modifiers:
            print(f"   modifier: {mod.name} type={mod.type}")
    if o.type == 'ARMATURE':
        arm = o.data
        bones = arm.bones
        print(f"   ARMATURE bones={len(bones)}")
        for b in bones:
            print(f"      bone '{b.name}' parent={b.parent.name if b.parent else None}")

print("\n--- Actions (animations) ---")
for a in bpy.data.actions:
    fr = a.frame_range
    print(f"ACTION '{a.name}' frame_range={fr} fcurves={len(a.fcurves)}")

print("\n--- Armatures data ---")
for arm in bpy.data.armatures:
    print(f"ARMDATA '{arm.name}' bones={len(arm.bones)}")

print("=== BLEND INSPECT END ===")
