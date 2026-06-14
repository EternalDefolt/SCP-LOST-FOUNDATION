import unreal

# Clean UE-ready FBX (mesh + skeleton + walk animation, single skeleton),
# rebuilt from Blender with UE-safe settings (no leaf bones, no Apply-Transform).
FBX = r"C:/Users/Eternal.DESKTOP-NA5STOG/Documents/UNREALGINGER/test/Art/HunyuanFull.fbx"
DEST = "/Game/Character"
SK_TARGET = "/Game/Character/SK_Hunyuan"
ANIM_TARGET = "/Game/Character/A_Walk"

eal = unreal.EditorAssetLibrary

# clean previous results so re-runs don't clash
for p in (SK_TARGET, ANIM_TARGET,
          "/Game/Character/Hunyuan", "/Game/Character/Hunyuan_Skeleton",
          "/Game/Character/Hunyuan_PhysicsAsset",
          "/Game/HunyuanWalk", "/Game/HunyuanWalk_Skeleton"):
    if eal.does_asset_exist(p):
        eal.delete_asset(p)

# ---- skeletal mesh import options ----
opts = unreal.FbxImportUI()
opts.import_mesh = True
opts.import_as_skeletal = True
opts.import_animations = True
opts.import_materials = False
opts.import_textures = False
opts.create_physics_asset = True
opts.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)

sk = opts.skeletal_mesh_import_data
sk.set_editor_property("import_morph_targets", False)
# Blender is Z-up / metres -> convert axes and units (metres -> centimetres).
sk.set_editor_property("convert_scene", True)
sk.set_editor_property("convert_scene_unit", True)
sk.set_editor_property("force_front_x_axis", False)
sk.set_editor_property("import_uniform_scale", 1.0)
sk.set_editor_property("normal_import_method",
    unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
# CRITICAL: use the real FBX bind pose, NOT frame 0 of the animation. With t0-as-ref
# a bad first animation frame collapses the whole bind pose and the skinned mesh
# renders invisible (all verts skin to a point). Default = use actual bind pose.
sk.set_editor_property("use_t0_as_ref_pose", False)

anim = opts.anim_sequence_import_data
anim.set_editor_property("convert_scene", True)
anim.set_editor_property("convert_scene_unit", True)
anim.set_editor_property("force_front_x_axis", False)
anim.set_editor_property("import_uniform_scale", 1.0)
anim.set_editor_property("import_custom_attribute", True)
anim.set_editor_property("animation_length",
    unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
anim.set_editor_property("remove_redundant_keys", False)

task = unreal.AssetImportTask()
task.filename = FBX
task.destination_path = DEST
task.automated = True
task.replace_existing = True
task.save = True
task.options = opts

unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

# ---- rename results to predictable names for the C++ paths ----
paths = list(task.imported_object_paths)
unreal.log("HUNYUAN IMPORT: created %s" % str(paths))
for p in paths:
    obj_path = p.split(".")[0]
    asset = eal.load_asset(p)
    if asset is None:
        continue
    if isinstance(asset, unreal.SkeletalMesh):
        if obj_path != SK_TARGET:
            eal.rename_asset(obj_path, SK_TARGET)
    elif isinstance(asset, unreal.AnimSequence):
        if obj_path != ANIM_TARGET:
            eal.rename_asset(obj_path, ANIM_TARGET)

eal.save_directory(DEST, only_if_is_dirty=False)
unreal.log("HUNYUAN IMPORT: done -> %s , %s" % (SK_TARGET, ANIM_TARGET))
