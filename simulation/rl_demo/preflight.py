"""Fast course evaluation before the independent native PhysX rollout."""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
import torch
from dynamics import DroneEnv,ActorCritic
from showcase import reference,wind,gate_results,GATE_TIMES,GATE_RADIUS_M,GATE_TUBE_RADIUS_M,DRONE_ENVELOPE_RADIUS_M

p=argparse.ArgumentParser();p.add_argument('--run',type=Path,required=True)
p.add_argument('--checkpoint',default='policy_final.pt');args=p.parse_args();torch.set_num_threads(2)
net=ActorCritic();net.load_state_dict(torch.load(args.run/args.checkpoint,map_location='cpu',weights_only=False)['model']);net.eval()
report={'backend':'PyTorch surrogate; geometric clearance only','checkpoint':args.checkpoint,
        'checkpoint_sha256':hashlib.sha256((args.run/args.checkpoint).read_bytes()).hexdigest(),'results':{}}
for mode in ('baseline_pd','trained_ppo_residual'):
    env=DroneEnv(1,device='cpu',randomize=False)
    def ref_at(t):
        return tuple(torch.tensor(v,dtype=torch.float32).reshape(1,-1) if i<3 else torch.tensor([v],dtype=torch.float32) for i,v in enumerate(reference(t)))
    r=ref_at(0);env.p[:]=r[0];env.v[:]=r[1];env.q[:]=torch.tensor([1.,0.,0.,0.]);env.w[:]=0;env.voltage[:]=3.1;env.integral[:]=0;env.prev[:]=0
    trace=[]
    with torch.inference_mode():
        for i in range(round(34/env.dt)):
            t=i*env.dt;r=ref_at(t);env.wind[:]=torch.tensor(wind(t),dtype=torch.float32)
            act=net(env.observation(r)) if mode=='trained_ppo_residual' else torch.zeros(1,3)
            _,_,_,info=env.step(act,reset=False,ref=r)
            trace.append([t,*env.p[0].tolist(),*env.q[0].tolist(),*env.v[0].tolist(),*env.w[0].tolist(),*r[0][0].tolist(),float(info['error'][0]),*info['command'][0].tolist(),*act[0].tolist()])
    x=np.asarray(trace);gates=gate_results(x)
    clearances=[]
    for tg in GATE_TIMES:
        pt,vel,_,_=reference(tg);normal=vel/np.linalg.norm(vel)
        d=x[:,1:4]-pt;axial=d@normal;radial=np.linalg.norm(d-axial[:,None]*normal,axis=1)
        clearances.append(float(np.min(np.sqrt(axial**2+(radial-GATE_RADIUS_M)**2)-GATE_TUBE_RADIUS_M-DRONE_ENVELOPE_RADIUS_M)))
    report['results'][mode]={'rms_error_m':float(np.sqrt(np.mean(x[100:,17]**2))),
                            'max_error_m':float(x[:,17].max()),'min_height_m':float(x[:,3].min()),
                            'gates_passed':sum(g['passed'] for g in gates),'gates':gates,
                            'min_spherical_clearance_to_any_ring_m':min(clearances)}
    np.save(args.run/f'preflight-{Path(args.checkpoint).stem}-{mode}.npy',x)
out=args.run/f'preflight-{Path(args.checkpoint).stem}.json';out.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
