"""Isaac Sim native force-driven closed loop; optional native RTX recording.

Each pose comes from PhysX after the learned policy and motor forces run.
Only the camera, course markers and decorative rotor graphics are animated.
"""
import argparse
import hashlib
import json
import math
import subprocess
import sys
import traceback
from pathlib import Path

p=argparse.ArgumentParser()
p.add_argument('--package',type=Path,required=True);p.add_argument('--run',type=Path,required=True)
p.add_argument('--output',type=Path,required=True);p.add_argument('--seconds',type=float,default=34)
p.add_argument('--baseline',action='store_true');p.add_argument('--record',action='store_true')
p.add_argument('--visible',action='store_true');p.add_argument('--hover',action='store_true')
p.add_argument('--checkpoint',default='policy_final.pt')
args=p.parse_args();args.output.mkdir(parents=True,exist_ok=True)
from isaacsim import SimulationApp
app=SimulationApp({'headless':not args.visible,'width':1280,'height':720,
                   'window_width':1920,'window_height':1080,'renderer':'RayTracedLighting','multi_gpu':False,
                   'anti_aliasing':2 if args.hover else 3})
import numpy as np
import torch
import carb
from pxr import Gf, Sdf, UsdGeom, UsdLux, UsdShade, UsdPhysics, PhysxSchema
from isaacsim.core.api import World
from isaacsim.core.prims import RigidPrim
from isaacsim.core.utils.stage import get_current_stage
from dynamics import DroneEnv,ActorCritic,MODEL,rotation
from showcase import (reference,wind,chapter,path,gate_results,GATE_TIMES,
                      GATE_RADIUS_M,GATE_TUBE_RADIUS_M,DRONE_ENVELOPE_RADIUS_M)

torch.set_num_threads(4)
report={'status':'started','physics_backend':'Isaac Sim / PhysX','force_driven':True,
        'state_teleport_during_rollout':False,'baseline':args.baseline,'checkpoint':args.checkpoint,
        'model':MODEL,'trace_columns':['t','px','py','pz','qw','qx','qy','qz','vx','vy','vz','wx','wy','wz',
                                     'target_x','target_y','target_z','error_m','u_rl','u_rr','u_fr','u_fl','residual_x','residual_y','residual_z']}
report['checkpoint_sha256']=hashlib.sha256((args.run/args.checkpoint).read_bytes()).hexdigest()
report['course']={'gate_radius_m':GATE_RADIUS_M,'gate_tube_radius_m':GATE_TUBE_RADIUS_M,
                  'drone_envelope_radius_m':DRONE_ENVELOPE_RADIUS_M,'physical_gate_collisions':not args.hover}
report['source_sha256']={f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in Path(__file__).parent.iterdir() if f.suffix in ('.py','.json')}
encoder=None;trace=[];image_times=[]
try:
    world=World(physics_dt=MODEL['physics_dt_s'],rendering_dt=MODEL['control_dt_s'],stage_units_in_meters=1.)
    stage=get_current_stage()
    root=UsdGeom.Xform.Define(stage,'/World/Drone').GetPrim()
    visual=UsdGeom.Xform.Define(stage,'/World/Drone/Visual')
    visual.GetPrim().GetReferences().AddReference(str((args.package/'USD/open32droe/robot.usd').resolve()))
    # Aggregate the assembly for this rough flight model without editing the source asset.
    for prim in list(stage.Traverse()):
        if not str(prim.GetPath()).startswith('/World/Drone/Visual'):continue
        if prim.IsA(UsdPhysics.Joint):prim.SetActive(False);continue
        for api in (UsdPhysics.RigidBodyAPI,UsdPhysics.MassAPI,UsdPhysics.ArticulationRootAPI,
                    PhysxSchema.PhysxRigidBodyAPI,PhysxSchema.PhysxArticulationAPI):
            if prim.HasAPI(api):prim.RemoveAPI(api)
    visual.ClearXformOpOrder();visual.AddTranslateOp().Set(Gf.Vec3d(0,0,0.006673))
    UsdPhysics.RigidBodyAPI.Apply(root)
    mass=UsdPhysics.MassAPI.Apply(root);mass.CreateMassAttr(MODEL['mass_kg'])
    mass.CreateCenterOfMassAttr(Gf.Vec3f(0,0,0));mass.CreateDiagonalInertiaAttr(Gf.Vec3f(*MODEL['inertia_diagonal_kg_m2']))
    phys=PhysxSchema.PhysxRigidBodyAPI.Apply(root);phys.CreateLinearDampingAttr(0);phys.CreateAngularDampingAttr(0)
    phys.CreateMaxAngularVelocityAttr(2000)
    world.scene.add_default_ground_plane(z_position=-0.02)

    def material(name,color,emission=0.):
        mat=UsdShade.Material.Define(stage,'/World/Materials/'+name)
        shader=UsdShade.Shader.Define(stage,str(mat.GetPath())+'/Shader');shader.CreateIdAttr('UsdPreviewSurface')
        shader.CreateInput('diffuseColor',Sdf.ValueTypeNames.Color3f).Set(Gf.Vec3f(*color))
        shader.CreateInput('roughness',Sdf.ValueTypeNames.Float).Set(0.45)
        shader.CreateInput('emissiveColor',Sdf.ValueTypeNames.Color3f).Set(Gf.Vec3f(*(np.array(color)*emission)))
        mat.CreateSurfaceOutput().ConnectToSource(shader.ConnectableAPI(),'surface');return mat
    cyan=material('CourseCyan',(0.05,0.6,0.85),0.8)
    gold=material('CourseGold',(1.0,0.38,0.05),0.5)
    gridmat=material('Grid',(0.07,0.12,0.19),0.2)
    groundmat=material('Floor',(0.028,0.043,0.075),0.)
    for prim in stage.Traverse():
        if 'groundplane' in str(prim.GetPath()).lower() and prim.IsA(UsdGeom.Gprim):
            UsdShade.MaterialBindingAPI.Apply(prim).Bind(groundmat)
    # A visible surface with a known material, above the default physics plane.
    floor=UsdGeom.Cube.Define(stage,'/World/DarkFloor');floor.CreateSizeAttr(1)
    floor.AddTranslateOp().Set(Gf.Vec3f(0,0,-0.02));floor.AddScaleOp().Set(Gf.Vec3f(12,12,0.01))
    UsdShade.MaterialBindingAPI.Apply(floor.GetPrim()).Bind(groundmat)
    def line(name,points,width,mat,closed=False):
        curve=UsdGeom.BasisCurves.Define(stage,'/World/Course/'+name)
        curve.CreateTypeAttr('linear');curve.CreateWrapAttr('periodic' if closed else 'nonperiodic')
        curve.CreatePointsAttr([Gf.Vec3f(*v) for v in points]);curve.CreateCurveVertexCountsAttr([len(points)])
        curve.CreateWidthsAttr([width]);curve.SetWidthsInterpolation('constant');UsdShade.MaterialBindingAPI.Apply(curve.GetPrim()).Bind(mat)
        return curve
    for i,x in enumerate(np.arange(-4,4.01,0.4)):
        line('gridx'+str(i),[(x,-4,0),(x,4,0)],0.003,gridmat)
        line('gridy'+str(i),[(-4,x,0),(4,x,0)],0.003,gridmat)
    # The full reference is shown in the edited video's map; drawing it as a
    # tube in the 3D scene can obscure this very small aircraft.
    if args.hover:
        line('HoverTargetX',[(-0.09,0,1.35),(0.09,0,1.35)],0.004,gold)
        line('HoverTargetY',[(0,-0.09,1.35),(0,0.09,1.35)],0.004,gold)
        line('HoverGuide',[(0,0,0),(0,0,1.35)],0.002,gridmat)
    for i,tg in enumerate([] if args.hover else GATE_TIMES):
        pt,vel,_,_=reference(tg);normal=vel/np.linalg.norm(vel)
        side=np.cross(normal,[0,0,1]);side/=np.linalg.norm(side);up=np.cross(side,normal)
        points=[pt+GATE_RADIUS_M*(math.cos(a)*side+math.sin(a)*up) for a in np.linspace(0,2*math.pi,48,endpoint=False)]
        mat=cyan if i%2==0 else gold
        line('Gate'+str(i),points,2*GATE_TUBE_RADIUS_M,mat,True)
        for j,(a,b) in enumerate(zip(points,points[1:]+points[:1])):
            shape=UsdGeom.Capsule.Define(stage,f'/World/Colliders/Gate{i}/Segment{j}')
            delta=b-a;shape.CreateRadiusAttr(GATE_TUBE_RADIUS_M);shape.CreateHeightAttr(float(np.linalg.norm(delta)))
            shape.AddTranslateOp().Set(Gf.Vec3d(*map(float,(a+b)/2)))
            shape.AddOrientOp().Set(Gf.Quatf(Gf.Rotation(Gf.Vec3d(0,0,1),Gf.Vec3d(*map(float,delta))).GetQuat()))
            UsdPhysics.CollisionAPI.Apply(shape.GetPrim())
            shape.CreateVisibilityAttr('invisible')
    trail=line('ActualFlightTrail',[(0,0,1.35),(0,0,1.35)],0.010,cyan)
    if args.hover:trail.CreateVisibilityAttr('invisible')
    dome=UsdLux.DomeLight.Define(stage,'/World/Dome');dome.CreateIntensityAttr(400)
    key=UsdLux.DistantLight.Define(stage,'/World/Key');key.CreateIntensityAttr(900)
    UsdGeom.Xformable(key).AddRotateXYZOp().Set(Gf.Vec3f(-25,-30,-30))
    camera=UsdGeom.Camera.Define(stage,'/World/Camera');camera.CreateFocalLengthAttr(27)
    camera.CreateHorizontalApertureAttr(36);camera.CreateVerticalApertureAttr(20.25)
    camera.CreateClippingRangeAttr(Gf.Vec2f(0.01,30));cameraop=camera.AddTransformOp()
    world.reset()
    body=RigidPrim(prim_paths_expr='/World/Drone',name='flight_body',reset_xform_properties=False);body.initialize()
    env=DroneEnv(1,device='cpu',randomize=False)
    env.voltage[:]=3.1
    net=ActorCritic();net.load_state_dict(torch.load(args.run/args.checkpoint,map_location='cpu',weights_only=False)['model']);net.eval()
    def ref_at(t):
        r=(np.array([0.,0.,1.35]),np.zeros(3),np.zeros(3),0.) if args.hover else reference(t)
        return tuple(torch.tensor(v,dtype=torch.float32).reshape(1,-1) if i<3 else torch.tensor([v],dtype=torch.float32) for i,v in enumerate(r))
    start=ref_at(0);env.p[:]=start[0];env.v[:]=start[1];env.q[:]=torch.tensor([1.,0.,0.,0.]);env.t[:]=0
    body.set_world_poses(positions=env.p.numpy(),orientations=env.q.numpy())
    body.set_velocities(np.concatenate((env.v.numpy(),np.zeros((1,3))),axis=1).astype(np.float32))
    history=[]
    if args.record:
        import omni.replicator.core as rep
        # Zero-time render subframes must not create exaggerated motion streaks.
        carb.settings.get_settings().set_bool('/omni/replicator/captureMotionBlur',False)
        carb.settings.get_settings().set_bool('/rtx/post/motionblur/enabled',False)
        from PIL import Image,ImageDraw,ImageFont
        from omni.kit.viewport.utility import get_active_viewport
        viewport=get_active_viewport();viewport.camera_path=str(camera.GetPath());viewport.set_texture_resolution((1280,720))
        product=rep.create.render_product(str(camera.GetPath()),(1280,720))
        rgb=rep.AnnotatorRegistry.get_annotator('rgb');rgb.attach([product])
        font=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',22)
        fontbig=ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf',28)
        encoder=subprocess.Popen(['/usr/bin/ffmpeg','-y','-hide_banner','-loglevel','error','-f','rawvideo','-pix_fmt','rgb24','-s','1280x720','-r',str(round(1/env.dt)),'-i','pipe:0','-an','-c:v','libx264','-preset','fast','-crf','18','-pix_fmt','yuv420p','-movflags','+faststart',str(args.output/'native_demo.mp4')],stdin=subprocess.PIPE)
    cameraeye=np.array([-1.0,-1.1,2.0]);nextframe=0.
    cameraop.Set(Gf.Matrix4d().SetLookAt(Gf.Vec3d(*cameraeye),Gf.Vec3d(0,0,1.35),Gf.Vec3d(0,0,1)).GetInverse())
    if args.record:
        world.pause()
        for _ in range(40):world.render()
        world.play()
    steps=round(args.seconds/env.dt)
    with torch.inference_mode():
        for i in range(steps):
            t=i*env.dt;r=ref_at(t)
            pos,q=body.get_world_poses();vel=body.get_velocities()
            env.p[:]=torch.from_numpy(pos.copy());env.q[:]=torch.from_numpy(q.copy());env.v[:]=torch.from_numpy(vel[:,:3].copy())
            env.w[:]=torch.bmm(rotation(env.q).transpose(1,2),torch.from_numpy(vel[:,3:].copy())[:,:,None]).squeeze(-1)
            env.wind[:]=torch.tensor(wind(t),dtype=torch.float32)
            act=torch.zeros(1,3) if args.baseline else net(env.observation(r))
            command=env.control(act,r)
            for _ in range(round(env.dt/env.h)):
                pos,q=body.get_world_poses();vel=body.get_velocities()
                env.q[:]=torch.from_numpy(q.copy());env.v[:]=torch.from_numpy(vel[:,:3].copy())
                force,torque_b=env.actuator_wrench(command)
                torque_w=torch.bmm(rotation(env.q),torque_b[:,:,None]).squeeze(-1)
                body.apply_forces_and_torques_at_pos(forces=force.numpy(),torques=torque_w.numpy(),is_global=True)
                world.step(render=False)
            pos,q=body.get_world_poses();vel=body.get_velocities()
            err=float(np.linalg.norm(pos[0]-r[0].numpy()[0]))
            trace.append([t,*pos[0],*q[0],*vel[0],*r[0].numpy()[0],err,*command.numpy()[0],*act.numpy()[0]])
            history.append(pos[0].copy())
            if not np.isfinite(trace[-1]).all() or pos[0,2]<0.08 or err>3:
                raise RuntimeError(f'Flight failure at {t:.2f}s; height={pos[0,2]:.3f}; error={err:.3f}')
            if args.record:
                # Position is produced by physics; only the filming camera follows it.
                if args.hover:
                    cameraeye=np.array([-0.60,-0.80,1.95]);target=np.array([0.09,-0.10,1.27])
                else:
                    wanted=pos[0]+np.array([-0.48,-0.68,0.62])
                    cameraeye=0.92*cameraeye+0.08*wanted
                    target=pos[0]+0.10*r[1].numpy()[0]
                cameraop.Set(Gf.Matrix4d().SetLookAt(Gf.Vec3d(*map(float,cameraeye)),Gf.Vec3d(*map(float,target)),Gf.Vec3d(0,0,1)).GetInverse())
                visible_trail=history[-90:] if len(history)>1 else history*2
                trail.CreatePointsAttr([Gf.Vec3f(*map(float,v)) for v in visible_trail]);trail.CreateCurveVertexCountsAttr([len(visible_trail)])
                world.render();rep.orchestrator.step(rt_subframes=2,delta_time=0.,pause_timeline=True);world.play()
                pixels=np.asarray(rgb.get_data())[:,:,:3]
                image=Image.fromarray(pixels);draw=ImageDraw.Draw(image)
                draw.rectangle((0,0,1280,105),fill=(10,16,30));draw.rectangle((0,635,1280,720),fill=(10,16,30))
                draw.text((30,18),'OPEN32DRONE  /  LEARN TO FLY',font=fontbig,fill=(235,245,255))
                draw.text((30,62),'HOVER / CONSTANT DISTURBANCE' if args.hover else chapter(t),font=font,fill=(70,220,250))
                mode='BASELINE PD' if args.baseline else 'PPO RESIDUAL + GEOMETRIC PD'
                draw.text((30,651),mode+'    |    ISAAC SIM / PHYSX',font=font,fill=(245,245,250))
                draw.text((30,684),f'{t:05.2f} s    Tracking error {err*100:05.1f} cm    81 g    60 mm    Rough model',font=font,fill=(170,191,212))
                encoder.stdin.write(np.asarray(image).tobytes());image_times.append(t)
                if len(image_times) in (60,300,600,1000,1500):image.save(args.output/f'preview_{len(image_times):04d}.png')
            if i%100==0:print(f'NATIVE_FLIGHT t={t:.2f} error={err:.4f}',flush=True)
    values=np.asarray(trace)
    np.save(args.output/'native_trace.npy',values)
    report.update(status='passed',simulated_seconds=args.seconds,rms_error_m=float(np.sqrt(np.mean(values[100:,17]**2))),
                  max_error_m=float(values[:,17].max()),frames=len(image_times),render_sample_times_s=image_times,
                  min_height_m=float(values[:,3].min()),max_speed_m_s=float(np.linalg.norm(values[:,8:11],axis=1).max()))
    if not args.hover:
        report['gates']=gate_results(values)
        report['gates_passed']=sum(g['passed'] for g in report['gates'])
        report['gates_total']=len(GATE_TIMES)
        report['course_status']='passed' if report['gates_passed']==len(GATE_TIMES) else 'incomplete'
except Exception:
    report['status']='failed';report['exception']=traceback.format_exc();traceback.print_exc()
finally:
    if trace:np.save(args.output/'native_trace.npy',np.asarray(trace))
    if encoder is not None:
        encoder.stdin.close();report['encoder_exit_code']=encoder.wait(timeout=30)
        if report['encoder_exit_code']!=0:report['status']='failed';report['encoding_error']='ffmpeg failed'
    (args.output/'native_result.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k not in ('model','render_sample_times_s')}),flush=True)
    app.close()
sys.exit(0 if report['status']=='passed' else 1)
