import unreal
eal = unreal.EditorAssetLibrary
ar = unreal.AssetRegistryHelpers.get_asset_registry()
for p in ("/Game/Character/A_Walk","/Game/Character/A_Idle","/Game/Character/A_Run",
          "/Game/Character/SK_Hunyuan","/Game/Character/Hunyuan_Mesh_Skeleton"):
    if eal.does_asset_exist(p):
        a = unreal.load_asset(p)
        cls = a.get_class().get_name()
        src = ""
        try:
            d = a.get_editor_property("asset_import_data")
            src = d.get_first_filename()
        except Exception as e:
            src = "n/a:%s" % e
        unreal.log("ASSET %s class=%s src=%s" % (p, cls, src))
    else:
        unreal.log("ASSET %s MISSING" % p)
unreal.log("DIAG ASSETS DONE")
