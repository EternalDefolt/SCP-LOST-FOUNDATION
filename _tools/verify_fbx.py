import bpy, sys

argv = sys.argv
fbx = argv[argv.index("--") + 1]

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=fbx)

print("=== VERIFY START ===")
arm = next((o for o in bpy.data.objects if o.type == 'ARMATURE'), None)
mesh = next((o for o in bpy.data.objects if o.type == 'MESH'), None)
print("bones:", len(arm.data.bones))
leaf = [b.name for b in arm.data.bones if b.name.endswith('_end')]
print("leaf_bones:", leaf)
print("mesh:", mesh.name, "verts:", len(mesh.data.vertices))

# overall world-space height of the mesh (scale sanity, in Blender meters)
import mathutils
coords = [ (mesh.matrix_world @ v.co) for v in mesh.data.vertices ]
zs = [c.z for c in coords]
print("mesh height (m): %.3f  (min z %.3f, max z %.3f)" % (max(zs)-min(zs), min(zs), max(zs)))

act = next((a for a in bpy.data.actions), None)
print("action:", act.name if act else None, "range:", tuple(act.frame_range) if act else None)

sc = bpy.context.scene
f0 = int(act.frame_range[0]); f1 = int(act.frame_range[1]); fm = (f0+f1)//2

def bone_head_world(bone_name, frame):
    sc.frame_set(frame)
    bpy.context.view_layer.update()
    pb = arm.pose.bones[bone_name]
    return arm.matrix_world @ pb.head

for bn in ["RightHand", "LeftFoot", "RightUpLeg", "Head"]:
    p0 = bone_head_world(bn, f0)
    pm = bone_head_world(bn, fm)
    p1 = bone_head_world(bn, f1)
    d = (pm - p0).length + (p1 - pm).length
    print(f"bone {bn}: total motion over anim = {d:.4f} m   (f{f0}->{p0.to_tuple(3)} fm->{pm.to_tuple(3)})")

print("=== VERIFY END ===")
