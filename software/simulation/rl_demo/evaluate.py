"""Held-out deterministic comparisons, including zero-residual PD baseline."""
import argparse
import json
from pathlib import Path
import torch
from dynamics import DroneEnv,ActorCritic,MODEL

p=argparse.ArgumentParser();p.add_argument('--run',type=Path,required=True);p.add_argument('--checkpoint',default='policy_final.pt')
p.add_argument('--seeds',type=int,nargs='+',default=[1001,1002,1003])
p.add_argument('--tag',default='')
args=p.parse_args();torch.set_num_threads(4)
net=ActorCritic().cuda();net.load_state_dict(torch.load(args.run/args.checkpoint,map_location='cuda',weights_only=False)['model']);net.eval()
report={'backend':'PyTorch six-DoF surrogate','checkpoint':args.checkpoint,'cases':{},'evaluation_seeds':args.seeds,
        'trials_per_seed_per_case':64,'trial_seconds':12,'rms_warmup_excluded_s':2,
        'initial_heading_rad':[-0.6,0.6],
        'cases_definition':{'nominal':'nominal mass, motors and supply; no wind',
                            'constant_wind':'3.1 V; acceleration disturbance [1.8, -1.2, 0.2] m/s^2',
                            'motor_variation':'same disturbance; mass and inertia +8%; motor gains [0.9,1.03,0.95,1.05]; lag 60 ms',
                            'gusts':'3.1 V; vertical 0.2 m/s^2; horizontal pulses at 3-6 s and 7.6-9.6 s'},
        'baseline':'same geometric PD and four motor dynamics, zero learned residual'}
for case in ('nominal','constant_wind','motor_variation','gusts'):
    report['cases'][case]={}
    for label in ('baseline_pd','trained_ppo_residual'):
        seed_results=[]
        for seed in report['evaluation_seeds']:
            env=DroneEnv(64,seed=seed,randomize=False)
            if case!='nominal':
                env.voltage[:]=3.1;env.wind[:]=torch.tensor([1.8,-1.2,0.2],device='cuda')
            if case=='motor_variation':
                env.gain[:]=torch.tensor([0.9,1.03,0.95,1.05],device='cuda')
                env.mass[:]*=1.08;env.J[:]*=1.08;env.tau[:]=0.06
            error=[];died=torch.zeros(64,dtype=torch.bool,device='cuda'); action_var=[];last=torch.zeros(64,3,device='cuda')
            with torch.inference_mode():
                for i in range(600):
                    if case=='gusts':
                        env.wind[:,0]=1.8 if 150<=i<300 else (-1.5 if 380<=i<480 else 0)
                        env.wind[:,1]=-1.2 if 150<=i<300 else 0
                    obs=env.observation();a=torch.zeros(64,3,device='cuda') if label=='baseline_pd' else net(obs)
                    _,_,_,info=env.step(a,reset=False)
                    died|=info['died'];
                    if i>=100:error.append(info['error'].clone());action_var.append(((a-last)**2).sum(1))
                    last=a.clone()
            e=torch.stack(error)
            seed_results.append({'seed':seed,'rms_position_error_m':e.square().mean().sqrt().item(),
                                 'p95_position_error_m':torch.quantile(e.flatten(),0.95).item(),
                                 'max_error_m':e.max().item(),'survived_fraction':(~died).float().mean().item(),
                                 'mean_residual_step_change':torch.stack(action_var).mean().sqrt().item()})
        report['cases'][case][label]={'seeds':seed_results,'rms_position_error_m':sum(x['rms_position_error_m'] for x in seed_results)/len(seed_results),
                                      'survived_fraction':sum(x['survived_fraction'] for x in seed_results)/len(seed_results)}
    base=report['cases'][case]['baseline_pd']['rms_position_error_m'];new=report['cases'][case]['trained_ppo_residual']['rms_position_error_m']
    report['cases'][case]['rms_improvement_percent']=100*(base-new)/base
report['status']='completed'
out=args.run/('evaluation-'+Path(args.checkpoint).stem+('-'+args.tag if args.tag else '')+'.json');out.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2),flush=True)
