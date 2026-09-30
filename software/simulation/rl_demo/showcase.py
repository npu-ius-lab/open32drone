"""Smooth reference course shared by numerical evaluation and native replay."""
import math
import numpy as np

GATE_TIMES = [1.3,3.5,5.6,7.8,10.,16.,18.,20.,22.,24.]
GATE_RADIUS_M = 0.38
GATE_TUBE_RADIUS_M = 0.0125
DRONE_ENVELOPE_RADIUS_M = 0.100


def smooth(x):
    x=np.clip(x,0.,1.)
    return x*x*x*(10+x*(-15+6*x))


def path(t):
    # A C2 blend between a figure eight, climbing spiral and final precision stop.
    a=2*math.pi*t/12
    eight=np.array([1.20*math.sin(a),0.78*math.sin(2*a),1.35+0.12*math.sin(a)])
    b=0.82*(t-12)
    helix=np.array([0.82*math.cos(b),0.82*math.sin(b),1.25+0.10*(t-12)])
    f=smooth((t-12)/3)
    result=(1-f)*eight+f*helix
    g=smooth((t-25)/4)
    return (1-g)*result+g*np.array([0.,0.,1.35])


def reference(t):
    h=0.01
    p=path(t);before=path(t-h);after=path(t+h)
    v=(after-before)/(2*h);a=(after-2*p+before)/(h*h)
    yaw=0.20*math.sin(t*0.4)
    return p,v,a,yaw


def wind(t):
    return np.array([1.0,-0.7,0.15])+(np.array([1.0,1.1,0.0]) if 29<=t<31 else 0)


def chapter(t):
    if t<12:return '01  FIGURE EIGHT / THROUGH GATES'
    if t<25:return '02  CLIMBING SPIRAL'
    return '03  PRECISION STOP / GUST RECOVERY'


def gate_results(trace):
    """Conservative swept-center check near each intended plane crossing.

    Treat the entire aircraft as a 100 mm radius sphere, including swept prop tips.
    A positive clearance means that sphere passes inside the physical ring.
    """
    results=[]
    for gate_id,tg in enumerate(GATE_TIMES,1):
        pt,vel,_,_=reference(tg);normal=vel/np.linalg.norm(vel)
        candidates=[]
        for a,b in zip(trace[:-1],trace[1:]):
            if abs(a[0]-tg)>1.4:continue
            da=np.dot(a[1:4]-pt,normal);db=np.dot(b[1:4]-pt,normal)
            if da<=0<db:
                f=-da/(db-da);cross=a[1:4]+f*(b[1:4]-a[1:4])
                distance=float(np.linalg.norm(cross-pt))
                candidates.append((distance,float(a[0]+f*(b[0]-a[0]))))
        if candidates:
            radial,crossing=min(candidates)
            clearance=GATE_RADIUS_M-GATE_TUBE_RADIUS_M-DRONE_ENVELOPE_RADIUS_M-radial
            results.append({'gate':gate_id,'crossing_s':crossing,'center_offset_m':radial,
                            'conservative_clearance_m':clearance,'passed':bool(clearance>0)})
        else:results.append({'gate':gate_id,'passed':False,'reason':'no forward crossing within 1.4 s of reference'})
    return results
