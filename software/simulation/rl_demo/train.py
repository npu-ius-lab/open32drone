"""Train residual PPO, retaining the initial policy, checkpoints and metrics."""
import argparse
import csv
import hashlib
import json
import time
import shutil
from pathlib import Path
import torch
from dynamics import DroneEnv,ActorCritic,MODEL

p=argparse.ArgumentParser()
p.add_argument('--output',type=Path,required=True)
p.add_argument('--iterations',type=int,default=300)
p.add_argument('--envs',type=int,default=1024)
p.add_argument('--seed',type=int,default=7)
p.add_argument('--resume',type=Path)
args=p.parse_args()
args.output.mkdir(parents=True,exist_ok=True)
(args.output/'source').mkdir(exist_ok=True)
for source in Path(__file__).parent.iterdir():
    if source.suffix in ('.py','.json'):shutil.copy2(source,args.output/'source'/source.name)
torch.set_num_threads(4)
env=DroneEnv(args.envs,seed=args.seed)
net=ActorCritic().cuda()
if args.resume:
    net.load_state_dict(torch.load(args.resume,map_location='cuda',weights_only=False)['model'])
    net.log_std.data.clamp_(min=-1.7)
optimizer=torch.optim.Adam(net.parameters(),lr=3e-4)
config={'algorithm':'PPO residual acceleration + geometric PD + four lagged motors',
        'model':MODEL,'seed':args.seed,'num_envs':args.envs,'iterations':args.iterations,'rollout_steps':64,
        'torch':torch.__version__,'gpu':torch.cuda.get_device_name(0),'train_backend':'PyTorch six-DoF surrogate',
        'observation_dim':35,'action_dim':3,'residual_scale_m_s2':4.,'native_isaac_validation':'separate required gate',
        'gradient_clipping':'actor and critic clipped separately; prevents critic scale suppressing actor learning',
        'randomization':{'mass_percent':5,'inertia_percent':15,'motor_gain_percent':5,'voltage_V':[2.85,3.8],
                         'motor_lag_s':[0.025,0.065],'heading_start_rad':[-0.6,0.6],
                         'wind_acceleration_m_s2':[[-2,2],[-2,2],[-0.5,0.5]],
                         'episodes':'25% nominal calm, 25% constant disturbance, 50% changing disturbance every 2.5 s'},
        'resume_checkpoint':str(args.resume) if args.resume else None,
        'resume_sha256':hashlib.sha256(args.resume.read_bytes()).hexdigest() if args.resume else None}
(args.output/'config.json').write_text(json.dumps(config,indent=2)+'\n')
def save(name,iteration):
    torch.save({'model':net.state_dict(),'iteration':iteration,'config':config},args.output/name)
save('policy_initial.pt',0)
obs=env.observation()
start=time.monotonic()
f=(args.output/'training.csv').open('w',buffering=1)
writer=csv.DictWriter(f,fieldnames=['iteration','steps','elapsed_s','reward_mean','tracking_error_m','failure_fraction','policy_std','policy_loss','value_loss'])
writer.writeheader()
for iteration in range(1,args.iterations+1):
    ob,ac,lp,va,rw,dn=[],[],[],[],[],[]
    errors=[]; failures=[]
    with torch.no_grad():
        for t in range(64):
            dist=net.distribution(obs); action=dist.sample()
            ob.append(obs);ac.append(action);lp.append(dist.log_prob(action).sum(-1));va.append(net.critic(obs).squeeze(-1))
            obs,reward,done,info=env.step(action)
            rw.append(reward);dn.append(done.float());errors.append(info['error']);failures.append(info['died'].float())
        last=net.critic(obs).squeeze(-1)
        advantages=torch.zeros(64,args.envs,device='cuda'); gae=torch.zeros(args.envs,device='cuda')
        for t in reversed(range(64)):
            nextv=last if t==63 else va[t+1]
            mask=1-dn[t]
            delta=rw[t]+0.99*nextv*mask-va[t]
            gae=delta+0.99*0.95*mask*gae;advantages[t]=gae
        values=torch.stack(va);returns=advantages+values
        adv=advantages.flatten();adv=(adv-adv.mean())/(adv.std()+1e-8)
        bobs=torch.stack(ob).flatten(0,1);bactions=torch.stack(ac).flatten(0,1)
        blogp=torch.stack(lp).flatten();bret=returns.flatten()
    for epoch in range(4):
        indices=torch.randperm(len(adv),device='cuda')
        for inds in indices.chunk(8):
            dist=net.distribution(bobs[inds]); logp=dist.log_prob(bactions[inds]).sum(-1)
            ratio=(logp-blogp[inds]).exp()
            pl=-torch.minimum(ratio*adv[inds],ratio.clamp(0.8,1.2)*adv[inds]).mean()
            vl=0.5*((net.critic(bobs[inds]).squeeze(-1)-bret[inds])**2).mean()
            loss=pl+vl-0.001*dist.entropy().sum(-1).mean()
            optimizer.zero_grad();loss.backward()
            torch.nn.utils.clip_grad_norm_(list(net.actor.parameters())+[net.log_std],1.0)
            torch.nn.utils.clip_grad_norm_(net.critic.parameters(),1.0)
            optimizer.step()
    row={'iteration':iteration,'steps':iteration*64*args.envs,'elapsed_s':round(time.monotonic()-start,3),
         'reward_mean':torch.stack(rw).mean().item(),'tracking_error_m':torch.stack(errors).mean().item(),
         'failure_fraction':torch.stack(failures).mean().item(),'policy_std':net.log_std.exp().mean().item(),
         'policy_loss':pl.item(),'value_loss':vl.item()}
    writer.writerow(row)
    if iteration%10==0 or iteration==1:print(json.dumps(row),flush=True)
    if iteration%50==0:save(f'policy_{iteration:04d}.pt',iteration)
save('policy_final.pt',args.iterations)
net.eval(); example=torch.zeros(1,35,device='cuda')
torch.jit.trace(net,example).save(str(args.output/'actor.pt'))
summary={'status':'training_completed','iterations':args.iterations,'steps':args.iterations*64*args.envs,
         'elapsed_s':time.monotonic()-start,'last_metrics':row,'checkpoint_sha256':hashlib.sha256((args.output/'policy_final.pt').read_bytes()).hexdigest()}
(args.output/'training-summary.json').write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps(summary),flush=True)
f.close()
