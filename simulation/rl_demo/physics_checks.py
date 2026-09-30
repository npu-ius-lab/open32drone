"""Analytical checks for gravity, hover balance, motor moments and decay."""
import argparse
import json
from pathlib import Path
import torch
from dynamics import DroneEnv,MODEL,quat_from_yaw

p=argparse.ArgumentParser();p.add_argument('--output',type=Path,required=True);args=p.parse_args()
torch.set_num_threads(2)
def fresh():
    e=DroneEnv(1,device='cpu',randomize=False)
    e.p[:]=torch.tensor([0.,0.,2.]);e.v[:]=0;e.q[:]=torch.tensor([1.,0.,0.,0.]);e.w[:]=0;e.wind[:]=0
    return e
checks={};e=fresh();e.motor[:]=0
for _ in range(20):e.integrate(torch.zeros(1,4))
expected=2-0.5*9.80665*0.1**2
checks['gravity_0_1s']={'error_m':abs(e.p[0,2].item()-expected),'passed':abs(e.p[0,2].item()-expected)<0.004}
e=fresh()
# CAD rotor coordinates are slightly asymmetric about the CG. Allocate zero
# moment as well as weight support, rather than assuming four equal forces.
e.motor[:]=torch.tensor([[e.mass0*9.80665,0.,0.,0.]])@e.inv_mix.T
u=e.motor.clone()/MODEL['max_force_per_motor_N']
for _ in range(200):e.integrate(u)
checks['balanced_hover']={'position_error_m':torch.linalg.norm(e.p-torch.tensor([0,0,2])).item(),'passed':torch.linalg.norm(e.p-torch.tensor([0,0,2])).item()<0.001}
e=fresh();e.motor[:]=0;e.motor[0,0]=0.1
wrench=e.motor@e.mix.T
checks['rear_left_moment_signs']={'wrench':wrench.tolist(),'passed':bool(wrench[0,1]>0 and wrench[0,2]>0 and wrench[0,3]>0)}
e=fresh();e.motor[:]=0.2
_,_=e.actuator_wrench(torch.zeros(1,4))
checks['motor_decay']={'remaining_force_N':e.motor[0,0].item(),'passed':bool(0<e.motor[0,0]<0.2)}
e=fresh();e.q[:]=quat_from_yaw(torch.tensor([3.0]))
reference=(e.p.clone(),torch.zeros(1,3),torch.zeros(1,3),torch.zeros(1))
command=e.control(torch.zeros(1,3),reference)
collective=float(command.sum()*MODEL['max_force_per_motor_N'])
checks['saturated_yaw_preserves_collective']={'collective_N':collective,'weight_N':e.mass0*9.80665,
    'passed':abs(collective-e.mass0*9.80665)<1e-5 and bool(((command>=0)&(command<=1)).all())}
e=fresh();e.w[:]=torch.tensor([0.2,-0.1,0.3]);e.motor[:]=0
for _ in range(200):e.integrate(torch.zeros(1,4))
checks['quaternion_unit_norm']={'norm':e.q.norm().item(),'passed':abs(e.q.norm().item()-1)<1e-6}
report={'status':'passed' if all(x['passed'] for x in checks.values()) else 'failed','checks':checks}
args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
raise SystemExit(0 if report['status']=='passed' else 1)
