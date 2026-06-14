import unreal
ar = unreal.AssetRegistryHelpers.get_asset_registry()
opt = unreal.AssetRegistryDependencyOptions()
opt.include_hard_package_references = True
deps = ar.get_dependencies("/Game/Character/ABP_Bodycam", opt)
for d in deps:
    unreal.log("DEP %s" % d)
unreal.log("ABP DEPS DONE")
