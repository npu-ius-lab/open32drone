"""Vectorized six-DoF surrogate and shared residual controller, SI units.

The learned policy supplies a bounded acceleration correction to a geometric
PD controller. Four individual motor forces include lag and supply variation.
"""
import json
import math
from pathlib import Path
import torch

MODEL = json.loads(Path(__file__).with_name('model.json').read_text())
OBS_DIM = 35
ACT_DIM = 3


def rotation(q):
    w, x, y, z = q.unbind(-1)
    return torch.stack((1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w),
                        2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w),
                        2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y)), -1).reshape(-1, 3, 3)


def quat_from_yaw(yaw):
    z = torch.zeros_like(yaw)
    return torch.stack((torch.cos(yaw/2), z, z, torch.sin(yaw/2)), -1)


class DroneEnv:
    def __init__(self, n=1024, device='cuda', seed=7, randomize=True):
        self.n, self.device = n, device
        torch.manual_seed(seed)
        self.dt, self.h = MODEL['control_dt_s'], MODEL['physics_dt_s']
        self.mass0 = MODEL['mass_kg']
        self.J0 = torch.tensor(MODEL['inertia_diagonal_kg_m2'], device=device)
        self.xy = torch.tensor(MODEL['rotor_xy_m'], device=device)
        self.mix = torch.stack((torch.ones(4, device=device), self.xy[:,1], -self.xy[:,0],
                                torch.tensor([1.,-1.,1.,-1.], device=device)*MODEL['yaw_moment_per_force_m']))
        self.inv_mix = torch.linalg.inv(self.mix)
        self.p = torch.zeros(n,3,device=device)
        self.v = torch.zeros_like(self.p)
        self.q = torch.zeros(n,4,device=device); self.q[:,0] = 1
        self.w = torch.zeros_like(self.p)
        self.integral = torch.zeros_like(self.p)
        self.prev = torch.zeros_like(self.p)
        self.motor = torch.zeros(n,4,device=device)
        self.mass = torch.ones(n,1,device=device)*self.mass0
        self.J = self.J0.expand(n,-1).clone()
        self.voltage = torch.ones(n,1,device=device)*MODEL['nominal_voltage_V']
        self.gain = torch.ones(n,4,device=device)
        self.tau = torch.ones(n,1,device=device)*MODEL['motor_time_constant_s']
        self.wind = torch.zeros_like(self.p)
        self.wind_mode = torch.zeros(n,device=device,dtype=torch.long)
        self.t = torch.zeros(n,device=device)
        self.phase = torch.zeros(n,3,device=device)
        self.radius = torch.ones(n,3,device=device)
        self.freq = torch.ones(n,1,device=device)
        self.randomize = randomize
        self.steps = torch.zeros(n,device=device,dtype=torch.long)
        self.reset(torch.arange(n,device=device))

    def reference(self):
        t = self.t[:,None]
        f = self.freq*torch.tensor([1.,1.7,0.7],device=self.device)
        phase = t*f+self.phase
        p = self.radius*torch.sin(phase)
        p[:,2] += 1.5
        v = self.radius*f*torch.cos(phase)
        a = -self.radius*f*f*torch.sin(phase)
        return p,v,a,torch.zeros(self.n,device=self.device)

    def reset(self, ids):
        k = len(ids)
        if not k:return
        def rand(*shape):return torch.rand(*shape,device=self.device)
        self.t[ids] = rand(k)*20
        self.phase[ids] = rand(k,3)*2*math.pi
        self.radius[ids] = rand(k,3)*torch.tensor([1.3,0.8,0.3],device=self.device)+torch.tensor([0.1,0.1,0.0],device=self.device)
        self.freq[ids] = 0.35+rand(k,1)*0.70
        p,v,_,_ = self.reference()
        self.p[ids] = p[ids]+(rand(k,3)-0.5)*0.25
        self.v[ids] = v[ids]+(rand(k,3)-0.5)*0.2
        # The showcase starts approximately level, with at most 35 degrees of
        # heading error. Extreme starts remain a separate stress test.
        self.q[ids] = quat_from_yaw((rand(k)-0.5)*1.2)
        self.w[ids] = 0
        self.integral[ids] = 0; self.prev[ids] = 0
        self.motor[ids] = self.mass0*9.80665/4
        self.steps[ids] = 0
        if self.randomize:
            self.mass[ids] = self.mass0*(0.95+0.10*rand(k,1))
            self.J[ids] = self.J0*(0.85+0.30*rand(k,3))
            self.voltage[ids] = 2.85+0.95*rand(k,1)
            self.gain[ids] = 0.95+0.10*rand(k,4)
            self.tau[ids] = 0.025+0.04*rand(k,1)
            self.wind[ids] = (rand(k,3)-0.5)*torch.tensor([4.,4.,1.0],device=self.device)
            self.wind_mode[ids] = torch.randint(0,4,(k,),device=self.device)
            calm=ids[self.wind_mode[ids]==0]
            self.wind[calm]=0;self.mass[calm]=self.mass0;self.gain[calm]=1
            self.voltage[calm]=MODEL['nominal_voltage_V'];self.tau[calm]=MODEL['motor_time_constant_s'];self.J[calm]=self.J0

    def observation(self, ref=None):
        if ref is None:ref=self.reference()
        p,v,a,yaw = ref
        obs = torch.cat(((p-self.p)/2, (v-self.v)/3, rotation(self.q).flatten(1), self.w/6,
                         v/3, a/6, self.prev, self.integral/2,
                         self.motor/MODEL['max_force_per_motor_N'], (self.voltage-3.3)),1)
        return obs.clamp(-5,5)

    def control(self, action, ref=None):
        if ref is None:ref=self.reference()
        p,v,a,yaw = ref
        ep = p-self.p
        self.integral = (self.integral+ep*self.dt).clamp(-2,2)
        self.prev = action.clamp(-1,1)
        acc = a+5.0*ep+3.4*(v-self.v)+self.prev*4.0
        acc[:,2] += 9.80665
        acc[:,2] = acc[:,2].clamp(3.,18.)
        acc[:,:2] = acc[:,:2].clamp(-12.,12.)
        desired_z = torch.nn.functional.normalize(acc,dim=1)
        R = rotation(self.q)
        desired_b = torch.bmm(R.transpose(1,2),desired_z[:,:,None]).squeeze(-1)
        error = torch.stack((-desired_b[:,1],desired_b[:,0],torch.zeros_like(yaw)),1)
        actual_yaw = torch.atan2(R[:,1,0],R[:,0,0])
        error[:,2] = torch.atan2(torch.sin(yaw-actual_yaw),torch.cos(yaw-actual_yaw))
        torque = self.J0*(55.0*error-12.0*self.w)
        total = (acc*R[:,:,2]).sum(1).clamp(0,19)*self.mass0
        # Preserve collective when a large yaw target exceeds motor authority.
        # Scale moment allocation, not individual motors independently.
        collective=total[:,None]/4
        delta=torque@self.inv_mix[:,1:].T
        upper=(MODEL['max_force_per_motor_N']-collective)/delta.clamp(min=1e-6)
        lower=collective/(-delta).clamp(min=1e-6)
        scale=torch.minimum(torch.where(delta>0,upper,lower).amin(1,keepdim=True),torch.ones_like(collective)).clamp(0,1)
        forces=collective+scale*delta
        return (forces/MODEL['max_force_per_motor_N']).clamp(0,1)

    def actuator_wrench(self, command):
        maxforce = MODEL['max_force_per_motor_N']*(self.voltage/MODEL['nominal_voltage_V'])*self.gain
        desired = command*maxforce
        self.motor += (desired-self.motor)*(1-torch.exp(-self.h/self.tau))
        wrench = self.motor@self.mix.T
        R=rotation(self.q)
        force = R[:,:,2]*wrench[:,:1] + self.mass*(self.wind-MODEL['linear_drag_per_s']*self.v)
        return force,wrench[:,1:]

    def integrate(self, command):
        force,torque = self.actuator_wrench(command)
        acc = force/self.mass
        acc[:,2] -= 9.80665
        self.v += acc*self.h
        self.p += self.v*self.h
        self.w += (torque-torch.cross(self.w,self.J*self.w,dim=1))/self.J*self.h
        self.w = self.w.clamp(-30,30)
        qw,qv = self.q[:,:1],self.q[:,1:]
        dq = torch.cat((-(qv*self.w).sum(1,keepdim=True),qw*self.w+torch.cross(qv,self.w,dim=1)),1)
        self.q = torch.nn.functional.normalize(self.q+0.5*self.h*dq,dim=1)

    def step(self, action, reset=True, ref=None):
        if self.randomize:
            gust_ids=torch.where((self.wind_mode>=2)&(self.steps>0)&(self.steps%125==0))[0]
            self.wind[gust_ids]=(torch.rand(len(gust_ids),3,device=self.device)-0.5)*torch.tensor([4.,4.,1.0],device=self.device)
        reference = self.reference() if ref is None else ref
        previous = self.prev.clone()
        command = self.control(action,reference)
        for _ in range(round(self.dt/self.h)):self.integrate(command)
        self.t += self.dt; self.steps += 1
        newref = self.reference() if ref is None else ref
        ep = newref[0]-self.p; ev=newref[1]-self.v
        err2=(ep*ep).sum(1)
        reward = 1.0-3.0*err2-0.12*(ev*ev).sum(1)-0.03*(self.prev*self.prev).sum(1)-0.04*((self.prev-previous)**2).sum(1)
        died=(self.p[:,2]<0.15)|(self.p[:,2]>4.0)|(err2>9.)
        done=died|(self.steps>=600)
        reward -= died.float()*15
        info={'error':torch.sqrt(err2), 'died':died, 'command':command}
        if reset:self.reset(torch.where(done)[0])
        return self.observation(newref if ref is not None else None),reward,done,info


class ActorCritic(torch.nn.Module):
    def __init__(self):
        super().__init__()
        def net(out):
            return torch.nn.Sequential(torch.nn.Linear(OBS_DIM,128),torch.nn.ELU(),
                                       torch.nn.Linear(128,128),torch.nn.ELU(),torch.nn.Linear(128,out))
        self.actor=net(ACT_DIM); self.critic=net(1)
        self.log_std=torch.nn.Parameter(torch.ones(ACT_DIM)*-0.7)
        torch.nn.init.zeros_(self.actor[-1].weight); torch.nn.init.zeros_(self.actor[-1].bias)

    def distribution(self,obs):
        return torch.distributions.Normal(self.actor(obs),self.log_std.exp().expand(len(obs),-1))

    def forward(self,obs):return self.actor(obs).clamp(-1,1)
