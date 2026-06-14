import unreal
eal = unreal.EditorAssetLibrary
# wipe any stale A_Run (was wrongly a SkeletalMesh) + its physics asset
for p in ("/Game/Character/A_Run", "/Game/Character/A_Run_PhysicsAsset"):
    if eal.does_asset_exist(p):
        eal.delete_asset(p)
        unreal.log("deleted stale %s" % p)
# run the proven importer
exec(open(r"C:/Users/Eternal.DESKTOP-NA5STOG/Documents/UNREALGINGER/test/_tools/import_run.py").read())
