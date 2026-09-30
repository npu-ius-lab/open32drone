"""Execute production firmware functions on the host; never connect to hardware."""
import math
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def function(path, signature):
    text = (ROOT / path).read_text()
    start = text.index(signature)
    brace = text.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


CPP = r'''
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cassert>
#include <algorithm>
using std::isfinite;
using std::isnan;
using std::abs;
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#define constrain(x,a,b) min(max(x,a),b)
uint32_t clockMs=1000;
uint32_t millis(){return clockMs;}
void print(const char*, ...) {}
struct Vec {float x=0,y=0,z=0;};
'''


def run_cpp(body):
    compiler = shutil.which('c++')
    if not compiler:
        raise RuntimeError('A C++ compiler is required for flight-safety regressions')
    with tempfile.TemporaryDirectory(prefix='open32drone-safety-') as directory:
        source = Path(directory) / 'test.cpp'
        binary = Path(directory) / 'test'
        source.write_text(CPP + body)
        compiled = subprocess.run([compiler, '-std=c++17', '-O0', str(source), '-o', str(binary)],
                                  capture_output=True, text=True)
        if compiled.returncode:
            raise AssertionError(compiled.stderr)
        result = subprocess.run([str(binary)], capture_output=True, text=True)
        if result.returncode:
            raise AssertionError(result.stderr or result.stdout)


class SafetyRegressions(unittest.TestCase):
    def test_tof_dropout_does_not_accumulate_thrust(self):
        prefix = r'''
bool armed=true, assistedTakeoffGroundIdle=false, offboardLocalActive=false;
bool offboardUseAltitude=false, offboardUseVerticalSpeed=false, tofHealthy=false;
const int ALT_HOLD=4, POS_HOLD=5, AUTO=3, AUTO_TAKEOFF=1;
int mode=AUTO, autoFlightPhase=AUTO_TAKEOFF, voltagePin=-1;
float dt=1.0f/300, voltageCompensationMax=1, opticalFlowHeight=.65f;
float autoFlightTargetHeight=.65f, offboardTargetZ=.65f, offboardTargetVZ=0;
float thrustTarget=.54f, autoTakeoffThrustLimit=.60f, controlThrottle=.5f;
float tofSampleDt=.02f;
uint32_t tofTimestamp=900, tofSequence=1;
Vec position{0,0,.65f}, velocity{}, attitudeBodyUp{0,0,1};
const float FLOW_SENSOR_MIN_HEIGHT=.015f;
bool autoAltitudeActive(){return true;}
bool autoLandingFlareCutActive(){return false;}
bool pilotControlFresh(){return false;}
bool heightEstimateFresh(){return tofHealthy;}
float voltageThrustCompensationFactor(){return 1;}
'''
        main = r'''
int main(){
  altitudeHoldEngaged=true; altitudeHoldCorrection=.05f;
  float previous=thrustTarget;
  for(int i=0;i<300;i++){
    updateAltitudeHoldControl();
    assert(thrustTarget <= previous+1e-5f);
    assert(thrustTarget <= autoTakeoffThrustLimit);
    previous=thrustTarget;
  }
  assert(thrustTarget < .51f);
}
'''
        run_cpp(prefix + (ROOT/'firmware/control_altitude.ino').read_text() + main)

    def test_height_decay_depends_on_elapsed_time(self):
        prefix = r'''
template<class T> struct LowPassFilter {LowPassFilter(float){} void reset(){} T update(T x){return x;}};
Vec position{0,0,.65f}, velocity{};
bool tofHealthy=false, heightEstimateValid=false;
float opticalFlowHeight=.65f, tofSampleDt=.02f, dt;
uint32_t tofSequence=1, tofTimestamp=0;
const float FLOW_HEIGHT_MAX_RATE=2, FLOW_HEIGHT_JUMP_REJECT=.45f;
bool motorsActive(){return true;}
bool tofRangeFresh(){return tofHealthy;}
'''
        main = r'''
int main(){
 float results[3]; int hz[3]={150,300,600};
 for(int j=0;j<3;j++){
  dt=1.0f/hz[j]; velocity.z=1;
  for(int i=0;i<hz[j]/10;i++) estimateHeight();
  results[j]=velocity.z;
  assert(!heightEstimateValid); assert(position.z==.65f);
 }
 assert(abs(results[0]-results[1])<1e-5f);
 assert(abs(results[1]-results[2])<1e-5f);
}
'''
        run_cpp(prefix + function('firmware/estimate.ino', 'void estimateHeight()') + main)

    def test_disarm_and_manual_handover_require_correct_state(self):
        run_cpp('''
bool armed=true, flightGroundConfirmed=false, offboardActive=true, rcFailsafeActive=false;
const int AUTO=3, STAB=2; int mode=AUTO;
''' + function('firmware/safety.ino', 'bool ordinaryDisarmAllowed()') +
                function('firmware/safety.ino', 'bool mavlinkManualHandoverAllowed()') + '''
int main(){
 assert(!ordinaryDisarmAllowed()); flightGroundConfirmed=true; assert(ordinaryDisarmAllowed());
 flightGroundConfirmed=false; armed=false; assert(ordinaryDisarmAllowed());
 assert(!mavlinkManualHandoverAllowed()); mode=STAB; assert(mavlinkManualHandoverAllowed());
 rcFailsafeActive=true; assert(!mavlinkManualHandoverAllowed());
}
''')

    def test_offboard_does_not_fall_back_to_direct_throttle_on_stale_sticks(self):
        run_cpp('''
bool rcFailsafeActive=false, offboardActive=true, mavlinkArmSession=true;
bool rcPilotActive=false, rcLinkHealthy=false;
float controlRoll=.3f,controlPitch=0,controlYaw=0,controlThrottle=.5f;
const int AUTO=3,STAB=2; int mode=AUTO, releases=0;
bool autoFlightActive(){return false;}
void releaseOffboardControl(){releases++;offboardActive=false;}
''' + function('firmware/safety.ino', 'void autoFailsafe()') + '''
int main(){
 autoFailsafe(); assert(mode==AUTO && releases==0);
 mavlinkArmSession=false;rcPilotActive=rcLinkHealthy=true;
 autoFailsafe(); assert(mode==STAB && releases==1);
}
''')

    def test_peer_changes_only_after_valid_ground_heartbeat(self):
        run_cpp('''
using IPAddress=unsigned;
IPAddress udpIncomingIP=11, udpRemoteIP=0;
uint16_t udpIncomingPort=40000, udpPeerPort=14550;
bool udpPeerSelected=false,armed=false;
uint32_t udpPeerLastValidMs=0; const uint32_t UDP_PEER_GROUND_LEASE_MS=3000;
''' + function('firmware/wifi.ino', 'bool acceptWiFiPeer(bool controllerHeartbeat)') + '''
int main(){
 assert(!acceptWiFiPeer(false)); assert(udpRemoteIP==0 && udpPeerPort==14550);
 assert(acceptWiFiPeer(true)); assert(udpRemoteIP==11 && udpPeerPort==40000);
 armed=true;udpIncomingIP=22;udpIncomingPort=50000;clockMs+=4000;
 assert(!acceptWiFiPeer(true)); assert(udpRemoteIP==11 && udpPeerPort==40000);
 armed=false;assert(!acceptWiFiPeer(false));assert(acceptWiFiPeer(true));
 udpIncomingIP=33;clockMs+=100;assert(!acceptWiFiPeer(true));
 udpIncomingIP=22;assert(acceptWiFiPeer(false));
}
''')
        source = (ROOT/'firmware/mavlink.ino').read_text()
        receive = function('firmware/mavlink.ino', 'void receiveMavlink()')
        self.assertLess(receive.index('mavlink_parse_char'), receive.index('acceptWiFiPeer'))
        self.assertLess(receive.index('acceptWiFiPeer'), receive.index('mavlinkLastRxMs ='))
        self.assertIn('mavlink_reset_channel_status', receive)
        raw = function('firmware/wifi.ino', 'int receiveWiFi(uint8_t *buf, int len)')
        self.assertNotIn('udpRemoteIP =', raw)
        self.assertNotIn('udpPeerPort =', raw)

    def test_loop_watchdog_stops_all_pwm_before_reset(self):
        run_cpp('''
bool controlWatchdogArmed=false,controlWatchdogFault=false;
uint32_t controlWatchdogLastMs=1000; const uint32_t CONTROL_WATCHDOG_TIMEOUT_MS=100;
const int LEDC_LOW_SPEED_MODE=0;using ledc_channel_t=int;
int stopped=0,resets=0;
void ledc_stop(int,int channel,int duty){assert(duty==0);stopped|=1<<channel;}
void esp_restart(){assert(stopped==30);resets++;}
''' + function('firmware/loop_watchdog.ino', 'void controlWatchdogTick(void *)') +
                function('firmware/loop_watchdog.ino', 'void completeControlLoopWatchdog()') + '''
int main(){
 clockMs=5000;controlWatchdogTick(nullptr);assert(resets==0);
 controlWatchdogArmed=true;completeControlLoopWatchdog();
 clockMs+=100;controlWatchdogTick(nullptr);assert(resets==0);
 clockMs++;controlWatchdogTick(nullptr);assert(resets==1 && controlWatchdogFault);
}
''')

    def test_blind_descent_has_fixed_duration_and_no_range_recovery_rebound(self):
        prefix = '''
struct Vector {float x=0,y=0,z=0;Vector(){}Vector(float a,float b,float c):x(a),y(b),z(c){} };
struct Quaternion {bool valid(){return true;}static Quaternion fromEuler(Vector){return {};}};
struct PID {void reset(){}};
Quaternion attitude,attitudeTarget;Vector attitudeEuler,ratesExtra;
PID rollPID,pitchPID,yawPID,rollRatePID,pitchRatePID,yawRatePID;
bool failsafeAttitudeCaptured=false,blindDescentActive=false,fresh=false,flightGroundConfirmed=false;
bool autoTakeoffTargetValid=false,autoLandingTargetValid=false,active=false;
float failsafeYaw=0,blindDescentInitialThrust=0,thrustTarget=.5f,rcFailsafeThrust=0,descendTime=5;
double t=10,blindDescentStart=0;
const int AUTO=3,ALT_HOLD=4,AUTO_FLIGHT_IDLE=0,AUTO_SOURCE_FAILSAFE=3;
int mode=3,autoFlightPhase=0,autoFlightReturnMode=4,disarmed=0,landings=0;
bool tofRangeFresh(){return fresh;}bool autoFlightActive(){return active;}
float altitudeHoverFeedForward(){return .49f;}
void beginAutomaticLanding(int source){assert(source==AUTO_SOURCE_FAILSAFE);active=true;landings++;}
void releaseOffboardControl(){}
void forceDisarm(const char*){disarmed++;thrustTarget=0;}
'''
        main = '''
int main(){
 fresh=true;descend();assert(landings==1 && thrustTarget==.5f);descend();assert(landings==1);
 for(int hz: {150,300,600}) {
  blindDescentActive=false;fresh=false;active=false;thrustTarget=.4f;disarmed=0;t=10;
  descend(); assert(thrustTarget==.4f);
  for(int i=1;i<hz*5;i++){
   t=10+(double)i/hz; if(i==hz)fresh=true;
   float before=thrustTarget;descend();assert(disarmed==0);assert(thrustTarget<=before);
  }
  t=15;descend();assert(disarmed==1 && thrustTarget==0);
 }
 flightGroundConfirmed=true;thrustTarget=0;disarmed=0;fresh=true;
 descend();assert(disarmed==1 && thrustTarget==0);
}
'''
        run_cpp(prefix + function('firmware/safety.ino', 'void descend()') + main)

    def test_ground_state_covers_offboard_and_requires_continuous_contact(self):
        prefix = '''
float dt=.01f,thrustTarget=0,autoFlightGroundRange=.05f,opticalFlowHeight=.05f,controlThrottle=0;
double t=0; const float ONE_G=9.80665f,FLOW_SENSOR_MIN_HEIGHT=.015f;
float radians(float d){return d*.01745329252f;}
struct V {float n;bool valid(){return true;}float norm(){return n;}};
V acc{ONE_G},rates{0};Vec velocity;
bool armed=false,assistedTakeoffGroundIdle=true,flightWasAirborne=false;
bool rcFailsafeActive=false,flightGroundConfirmed=false,fresh=true;
bool imuHealthy=true,blindDescentActive=false,tofRangeInBlindZone=false;
uint32_t imuLastSampleMs=1000;
bool tofPacketFresh(){return false;}bool motorsActive(){return armed;}
const int AUTO_LAND_DESCEND=2,AUTO_LAND_FLARE=3,STAB=2;int autoFlightPhase=0,mode=3;
bool tofRangeFresh(){return fresh;}float altitudeHoverFeedForward(){return .49f;}
'''
        delay = function('firmware/util.h', 'class Delay') + ';\n'
        main = '''
int main(){
 for(int i=0;i<40;i++){t+=dt;updateFlightGroundState();}
 assert(flightGroundConfirmed);
 armed=true;thrustTarget=.5f;updateFlightGroundState();
 assert(!flightGroundConfirmed && !assistedTakeoffGroundIdle);
 opticalFlowHeight=.65f;updateFlightGroundState();assert(flightWasAirborne);
 autoFlightPhase=AUTO_LAND_FLARE;thrustTarget=.10f;opticalFlowHeight=.05f;
 for(int i=0;i<20;i++){t+=dt;updateFlightGroundState();}assert(!flightGroundConfirmed);
 fresh=false;t+=dt;updateFlightGroundState();assert(!flightGroundConfirmed);
 fresh=true;for(int i=0;i<40;i++){t+=dt;updateFlightGroundState();}
 assert(flightGroundConfirmed);
}
'''
        run_cpp(prefix + delay + function('firmware/estimate.ino', 'void updateFlightGroundState()') + main)

    def test_bad_imu_sample_does_not_poison_last_good_input(self):
        prefix = '''
struct Vector {float x=0,y=0,z=0;bool valid(){return isfinite(x)&&isfinite(y)&&isfinite(z);}
 Vector operator-(Vector b){return {x-b.x,y-b.y,z-b.z};}
 Vector operator/(Vector b){return {x/b.x,y/b.y,z/b.z};}};
struct Quaternion{static Vector rotateVector(Vector v,Quaternion){return v;}};
struct Filter{Vector update(Vector v){return v;}};
Vector gyro{1,2,3},acc{0,0,9.8f},accRaw,accBias,accScale{1,1,1},gyroBias;
Quaternion imuRotationInverse;Filter accFilter;
bool imuConfigured=true,imuHealthy=true,readOK=true,finiteSample=false;
uint32_t imuReadFailures=0,imuInvalidSamples=0,imuConsecutiveFailures=0;
uint32_t imuMaxConsecutiveFailures=0,imuMaxSampleGapMs=0,imuLastSampleMs=1000;
bool acquireIMUSample(bool){return readOK;}void delay(int){}void calibrateGyroOnce(){}
struct Sensor {
 void getGyro(float& x,float& y,float& z){x=finiteSample?4:NAN;y=5;z=6;}
 void getAccel(float& x,float& y,float& z){x=0;y=0;z=9.8f;}
} imu;
'''
        main = '''
int main(){
 clockMs=1003;readIMU();assert(imuHealthy && gyro.x==1 && imuInvalidSamples==1);
 clockMs=1049;readOK=false;readIMU();assert(imuHealthy && gyro.x==1 && imuReadFailures==1);
 clockMs=1051;readIMU();assert(!imuHealthy && gyro.x==1);
 clockMs=1060;readOK=true;finiteSample=true;readIMU();
 assert(imuHealthy && gyro.x==4 && imuConsecutiveFailures==0 && imuMaxSampleGapMs==60);
 assert(imuMaxConsecutiveFailures==3);
}
'''
        run_cpp(prefix + function('firmware/imu.ino', 'void readIMU()') + main)

    def test_unmeasured_flow_bias_has_bounded_authority_and_no_integral(self):
        prefix = '''
double t=1;float radians(float d){return d*.01745329252f;}
bool armed=true,offboardLocalActive=false,offboardUsePositionXY=false,offboardUseVelocityXY=false;
bool autoTakeoffTargetValid=false,autoLandingRelockPending=false,autoLandingTargetValid=false;
bool tofHealthy=true,flowBiasReady=true,flowCtrlUsingFlow=true,flowPositionGateOpen=true;
bool flowAirborne=true,altitudeHoldEngaged=true,flowBiasFallbackActive=true;
const int AUTO_LAND_DESCEND=2,AUTO_LAND_FLARE=3,AUTO_TAKEOFF=1,POS_HOLD=5,AUTO=3;
int mode=POS_HOLD,autoFlightPhase=0,autoFlightReturnMode=POS_HOLD;
float tiltMax=radians(30),controlYaw=0,controlRoll=0,controlPitch=0;
float opticalFlowHeight=.65f,autoFlightGroundRange=.05f,opticalFlowSampleDt=.02f;
float autoTakeoffTargetX=0,autoTakeoffTargetY=0,autoLandingTargetX=0,autoLandingTargetY=0;
float offboardTargetX=0,offboardTargetY=0,offboardTargetVX=0,offboardTargetVY=0;
const float FLOW_SENSOR_MIN_HEIGHT=.015f;
uint32_t tofTimestamp=1000,opticalFlowSequence=1;
Vec position,velocity{-.3f,.3f,0},attitudeEuler;
bool pilotControlFresh(){return true;}bool autoFlightActive(){return false;}
'''
        main = '''
int main(){
 for(int i=0;i<200;i++){
  t+=.02;opticalFlowSequence++;updatePositionControlSplit(.02f);
  assert(velIntegralX==0 && velIntegralY==0);
  assert(abs(posRollCmd)<=radians(3)+1e-6f && abs(posPitchCmd)<=radians(3)+1e-6f);
 }
 assert(usePosCmd && posHoldGateOpen);
 flowBiasFallbackActive=false;
 for(int i=0;i<20;i++){t+=.02;opticalFlowSequence++;updatePositionControlSplit(.02f);}
 assert(velIntegralX>0 && velIntegralY<0);
}
'''
        run_cpp(prefix + function('firmware/util.h', 'class Delay') + ';\n' +
                (ROOT/'firmware/control_position.ino').read_text() + main)


if __name__ == '__main__':
    unittest.main()
