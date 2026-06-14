import unreal

bp = unreal.load_object(None, "/Game/Character/ABP_Bodycam")
unreal.log("BP: %s" % bp)

def list_graphs(obj):
    found = []
    for prop in ("function_graphs","ubergraph_pages","delegate_signature_graphs","macro_graphs"):
        try:
            gs = obj.get_editor_property(prop)
            for g in gs:
                found.append((prop, g.get_name()))
        except Exception as e:
            pass
    return found

unreal.log("graphs via props: %s" % list_graphs(bp))

# AnimBlueprint stores anim graphs too; try to reach EdGraph nodes through subobjects
# Walk all objects whose outer is the BP package and report graph/state node classes.
pkg = bp.get_outer()
allobj = unreal.find_all_objects() if hasattr(unreal,'find_all_objects') else []
cnt = {}
names = []
import unreal as u
# fallback: iterate loaded objects of relevant classes
for cls_name in ["AnimStateNode","AnimStateTransitionNode","AnimGraphNode_StateMachine",
                 "AnimGraphNode_SequencePlayer","AnimStateMachine","EdGraph"]:
    cls = getattr(unreal, cls_name, None)
    if cls is None:
        continue
    try:
        objs = unreal.GameplayStatics  # placeholder
    except Exception:
        pass

# Use the asset's generated class anim node properties: list sequence players' sequences
gen = bp.generated_class()
cdo = unreal.get_default_object(gen)
# Anim node structs live on the CDO as properties named "AnimGraphNode_*"; enumerate props
unreal.log("--- scanning CDO properties for anim sequences ---")
for p in dir(cdo):
    if p.lower().find("sequence") >= 0:
        unreal.log("CDO prop candidate: %s" % p)
unreal.log("READ SM DONE")
