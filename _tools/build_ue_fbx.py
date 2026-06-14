import bpy, sys, os

argv = sys.argv
src = argv[argv.index("--") + 1]
dst = argv[argv.index("--") + 2]

# Fresh scene
bpy.ops.wm.read_factory_settings(use_empty=True)

# Import the source FBX (mesh + armature + walk action all in one, single skeleton)
bpy.ops.import_scene.fbx(filepath=src, automatic_bone_orientation=False)

# Identify objects
armature = None
meshes = []
for o in list(bpy.data.objects):
    if o.type == 'ARMATURE':
        armature = o
    elif o.type == 'MESH':
        meshes.append(o)
    else:
        # drop cameras / lights / empties
        bpy.data.objects.remove(o, do_unlink=True)

print("ARMATURE:", armature.name if armature else None)
print("MESHES:", [m.name for m in meshes])

# Keep only the rigged mesh with the most vertex groups (HunyuanRigged), drop spares
keep_mesh = max(meshes, key=lambda m: len(m.vertex_groups)) if meshes else None
for m in meshes:
    if m is not keep_mesh:
        print("removing spare mesh:", m.name)
        bpy.data.objects.remove(m, do_unlink=True)

# Make sure the action is assigned to the armature so bake_anim exports it
action = None
for a in bpy.data.actions:
    action = a
    break
if armature and action:
    if not armature.animation_data:
        armature.animation_data_create()
    armature.animation_data.action = action
    print("assigned action:", action.name, "range:", tuple(action.frame_range))

# Set scene frame range to the action range so the bake captures the walk
sc = bpy.context.scene
if action:
    sc.frame_start = int(action.frame_range[0])
    sc.frame_end = int(action.frame_range[1])
print("scene frames:", sc.frame_start, "-", sc.frame_end, "fps:", sc.render.fps)

# Select armature + kept mesh only
bpy.ops.object.select_all(action='DESELECT')
if keep_mesh:
    keep_mesh.select_set(True)
armature.select_set(True)
bpy.context.view_layer.objects.active = armature

os.makedirs(os.path.dirname(dst), exist_ok=True)

# UE-friendly FBX export. Key anti-breakage flags:
#  - add_leaf_bones=False        : no extra _end bones that twist the UE skeleton
#  - bake_space_transform=False  : "Apply Transform" OFF -> animations stay intact
#  - bake_anim_use_all_bones=True + force_startend_keying : full pose every frame
#  - primary/secondary bone axis : stable bone roll for UE
bpy.ops.export_scene.fbx(
    filepath=dst,
    use_selection=True,
    object_types={'ARMATURE', 'MESH'},
    use_mesh_modifiers=True,
    mesh_smooth_type='FACE',
    add_leaf_bones=False,
    primary_bone_axis='Y',
    secondary_bone_axis='X',
    bake_space_transform=False,
    use_armature_deform_only=False,
    bake_anim=True,
    bake_anim_use_all_bones=True,
    bake_anim_use_nla_strips=False,
    bake_anim_use_all_actions=False,
    bake_anim_force_startend_keying=True,
    bake_anim_step=1.0,
    bake_anim_simplify_factor=0.0,
    apply_unit_scale=True,
    apply_scale_options='FBX_SCALE_NONE',
    global_scale=1.0,
    axis_forward='-Z',
    axis_up='Y',
    path_mode='COPY',
)
print("EXPORTED ->", dst, "exists:", os.path.exists(dst), "size:", os.path.getsize(dst) if os.path.exists(dst) else -1)
print("BUILD DONE")
