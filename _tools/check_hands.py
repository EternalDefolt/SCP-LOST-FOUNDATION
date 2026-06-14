import unreal, math
S = unreal.AnimSequenceService
for path in ("/Game/Character/A_Run","/Game/Character/A_Walk"):
    anim = unreal.load_asset(path)
    dur = anim.get_editor_property("sequence_length")
    ctrl = anim.get_editor_property("controller")
    n = ctrl.get_model_interface().get_number_of_keys()
    tt = dur * 12 / max(n-1,1)
    P = {str(p.bone_name): p.transform.translation for p in S.get_pose_at_time(path, tt, True)}
    def d(a,b):
        va,vb=P.get(a),P.get(b)
        if va is None or vb is None: return -1
        return math.sqrt((va.x-vb.x)**2+(va.y-vb.y)**2+(va.z-vb.z)**2)
    L=d("LeftHand_007","LeftHand"); R=d("RightHand_007","RightHand")
    Lsp=d("LeftHand_007","LeftHand_019"); Rsp=d("RightHand_007","RightHand_019")
    unreal.log("%s idxtip->wrist L=%.2f R=%.2f | tip-spread(idx..pinky) L=%.2f R=%.2f" % (path,L,R,Lsp,Rsp))
unreal.log("CHECK HANDS DONE")
