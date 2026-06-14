import unreal
ar = unreal.AssetRegistryHelpers.get_asset_registry()
for d in ar.get_assets_by_path("/Game", recursive=True):
    cls=str(d.asset_class_path.asset_name)
    if "Blend" in cls or "Blend" in str(d.asset_name) or cls in ("AnimSequence","AnimMontage"):
        unreal.log("ASSET %s : %s @ %s" % (d.asset_name, cls, d.package_name))
unreal.log("FIND BS DONE")
