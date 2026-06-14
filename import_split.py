import unreal

# Canonical Epic workflow: import the skeletal MESH first (builds the skeleton),
# then import the ANIMATION separately against that existing skeleton.
MESH_FBX = r"C:/Users/Eternal.DESKTOP-NA5STOG/Documents/UNREALGINGER/test/Art/Hunyuan_Mesh.fbx"
ANIM_FBX = r"C:/Users/Eternal.DESKTOP-NA5STOG/Documents/UNREALGINGER/test/Art/Hunyuan_Walk.fbx"
DEST = "/Game/Character"
SK_TARGET = "/Game/Character/SK_Hunyuan"
SKEL_TARGET = "/Game/Character/SK_Hunyuan_Skeleton"
ANIM_TARGET = "/Game/Character/A_Walk"

eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

# wipe the folder for a clean run (delete each asset individually; directory delete
# leaves referenced assets behind)
if eal.does_directory_exist(DEST):
    for ap in eal.list_assets(DEST, recursive=True, include_folder=False):
        eal.delete_asset(ap.split(".")[0])
    eal.delete_directory(DEST)

# ---------------- 1) MESH (no animation) ----------------
msk = unreal.FbxSkeletalMeshImportData()
msk.set_editor_property("import_morph_targets", False)
msk.set_editor_property("convert_scene", True)
msk.set_editor_property("convert_scene_unit", True)
msk.set_editor_property("force_front_x_axis", False)
msk.set_editor_property("import_uniform_scale", 1.0)
msk.set_editor_property("use_t0_as_ref_pose", False)  # real bind pose, never collapse
msk.set_editor_property("normal_import_method",
    unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)

mui = unreal.FbxImportUI()
mui.import_mesh = True
mui.import_as_skeletal = True
mui.import_animations = False          # MESH ONLY
mui.import_materials = False
mui.import_textures = False
mui.create_physics_asset = True
mui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
mui.set_editor_property("skeletal_mesh_import_data", msk)

mtask = unreal.AssetImportTask()
mtask.filename = MESH_FBX
mtask.destination_path = DEST
mtask.automated = True
mtask.replace_existing = True
mtask.save = True
mtask.options = mui
tools.import_asset_tasks([mtask])
unreal.log("SPLIT IMPORT: mesh created %s" % str(list(mtask.imported_object_paths)))

# grab the skeleton straight off the imported skeletal mesh, then rename mesh
skeleton = None
for p in list(mtask.imported_object_paths):
    obj_path = p.split(".")[0]
    a = eal.load_asset(p)
    if isinstance(a, unreal.SkeletalMesh):
        skeleton = a.get_editor_property("skeleton")   # the auto-created skeleton
        if obj_path != SK_TARGET:
            eal.rename_asset(obj_path, SK_TARGET)
unreal.log("SPLIT IMPORT: skeleton = %s" % (skeleton.get_path_name() if skeleton else "NONE"))

# ---------------- 2) ANIMATION against that skeleton ----------------
ask = unreal.FbxAnimSequenceImportData()
ask.set_editor_property("convert_scene", True)
ask.set_editor_property("convert_scene_unit", True)
ask.set_editor_property("force_front_x_axis", False)
ask.set_editor_property("import_uniform_scale", 1.0)
ask.set_editor_property("import_custom_attribute", True)
ask.set_editor_property("animation_length",
    unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
ask.set_editor_property("remove_redundant_keys", False)

aui = unreal.FbxImportUI()
aui.import_mesh = False
aui.import_as_skeletal = True
aui.import_animations = True
aui.import_materials = False
aui.import_textures = False
aui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)
aui.set_editor_property("skeleton", skeleton)         # CRITICAL: existing skeleton
aui.set_editor_property("anim_sequence_import_data", ask)

atask = unreal.AssetImportTask()
atask.filename = ANIM_FBX
atask.destination_path = DEST
atask.automated = True
atask.replace_existing = True
atask.save = True
atask.options = aui
tools.import_asset_tasks([atask])
unreal.log("SPLIT IMPORT: anim created %s" % str(list(atask.imported_object_paths)))

for p in list(atask.imported_object_paths):
    obj_path = p.split(".")[0]
    a = eal.load_asset(p)
    if isinstance(a, unreal.AnimSequence) and obj_path != ANIM_TARGET:
        eal.rename_asset(obj_path, ANIM_TARGET)

eal.save_directory(DEST, only_if_is_dirty=False)
unreal.log("SPLIT IMPORT: done -> %s , %s" % (SK_TARGET, ANIM_TARGET))
