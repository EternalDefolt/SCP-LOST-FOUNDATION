import unreal
eal = unreal.EditorAssetLibrary
ar = unreal.AssetRegistryHelpers.get_asset_registry()
for p in ("/Game/Character/A_Run",):
    if eal.does_asset_exist(p):
        a = unreal.load_asset(p)
        unreal.log("ASSET %s class=%s" % (p, a.get_class().get_name()))
    else:
        unreal.log("ASSET %s MISSING" % p)
# list everything in /Game/Character with 'Run'
for d in ar.get_assets_by_path("/Game/Character", recursive=True):
    n = str(d.asset_name)
    if "Run" in n:
        unreal.log("FOUND %s : %s" % (n, str(d.asset_class_path.asset_name)))
unreal.log("INSPECT RUN DONE")
