"""Regressions for the follow-up audit; no aircraft or ROS installation needed."""
import asyncio
from pathlib import Path
from types import SimpleNamespace as NS
import unittest
from test_safety_regressions import function, run_cpp
from test_ros_flight_flow import methods

ROOT = Path(__file__).resolve().parents[1]


class AuditRegressions(unittest.TestCase):
    def test_disarm_stops_hardware_before_watchdog_and_logging(self):
        run_cpp(r'''
#include <cstring>
bool watchdog=true,armed=true,rcFailsafeActive=true,offboardFailsafeActive=true;
bool mavlinkArmSession=true,assistedTakeoffGroundIdle=false,flightWasAirborne=true;
bool autoLandingRelockPending=true,autoTakeoffTargetValid=true,autoLandingTargetValid=true;
bool motorsInitialized=true;using ledc_channel_t=int;const int LEDC_LOW_SPEED_MODE=0;
float rcFailsafeThrust=.4f,thrustTarget=.4f,motors[4]={.4f,.4f,.4f,.4f};
float hardwareDuty[4]={.4f,.4f,.4f,.4f};
const int AUTO_FLIGHT_IDLE=0,AUTO_SOURCE_NONE=0,ACTUATOR_NONE=0;
int autoFlightPhase=2,autoFlightSource=1,actuatorOwner=1;
struct Target {void invalidate(){}} attitudeTarget,ratesTarget,torqueTarget;
void ledc_stop(int,int channel,int duty){assert(watchdog);assert(duty==0);hardwareDuty[channel-1]=0;}
void disarmControlLoopWatchdog(){for(float x:hardwareDuty)assert(x==0);watchdog=false;}
void resetRCEmergencyGesture(){} void resetFailsafeAttitude(){}
void resetTipOverGuard(){} void resetAutomaticLandingFlare(){} void releaseOffboardControl(){}
void observePrint(const char*,...){for(float x:hardwareDuty)assert(x==0);assert(!armed);}
#define print observePrint
''' + function('firmware/motors.ino', 'void stopMotors()') +
                function('firmware/safety.ino', 'void forceDisarm(const char *reason)') + r'''
int main(){forceDisarm("audit");for(float x:motors)assert(x==0);assert(!watchdog);}
''')

    def test_blind_zone_needs_recent_near_ground_history_or_disarmed_rest(self):
        run_cpp(r'''
float dt=.01f,thrustTarget=0,autoFlightGroundRange=.05f,opticalFlowHeight=.05f,controlThrottle=0;
double t=1;const float ONE_G=9.80665f,FLOW_SENSOR_MIN_HEIGHT=.015f;
float radians(float d){return d*.01745329252f;}
struct V{float n;bool valid(){return true;}float norm(){return n;}};
V acc{ONE_G},rates{0};Vec velocity;
bool armed=false,assistedTakeoffGroundIdle=true,flightWasAirborne=false;
bool rcFailsafeActive=false,blindDescentActive=false,flightGroundConfirmed=false,fresh=true;
bool packetFresh=true,tofRangeInBlindZone=false,imuHealthy=true;
uint32_t imuLastSampleMs=1000;
const int AUTO_LAND_DESCEND=2,AUTO_LAND_FLARE=3,STAB=2;int autoFlightPhase=0,mode=3;
bool tofRangeFresh(){return fresh;}bool tofPacketFresh(){return packetFresh;}
bool motorsActive(){return armed;}float altitudeHoverFeedForward(){return .49f;}
''' + function('firmware/util.h', 'class Delay') + ';\n' +
                function('firmware/estimate.ino', 'void updateFlightGroundState()') + r'''
void ticks(int n){while(n--){t+=dt;updateFlightGroundState();}}
int main(){
 ticks(40);assert(flightGroundConfirmed);
 armed=true;thrustTarget=.5f;opticalFlowHeight=.65f;ticks(1);assert(!flightGroundConfirmed);
 // High-altitude invalid zeros, including a stationary held aircraft, are not contact.
 fresh=false;tofRangeInBlindZone=true;autoFlightPhase=AUTO_LAND_FLARE;thrustTarget=.1f;
 ticks(200);assert(!flightGroundConfirmed);
 // A real near-ground sample followed by fresh blind-zone packets permits contact.
 fresh=true;opticalFlowHeight=.025f;ticks(1);fresh=false;blindDescentActive=true;
 ticks(20);assert(!flightGroundConfirmed);ticks(20);assert(flightGroundConfirmed);
 // No fresh packets, or a stale IMU, must invalidate that decision.
 packetFresh=false;ticks(1);assert(!flightGroundConfirmed);
 packetFresh=true;imuHealthy=false;ticks(40);assert(!flightGroundConfirmed);
 // Let near-ground history expire: stale history cannot authorize contact in flight.
 imuHealthy=true;ticks(200);assert(!flightGroundConfirmed);
 armed=false;thrustTarget=0;assistedTakeoffGroundIdle=true;flightWasAirborne=false;
 packetFresh=false;ticks(40);assert(!flightGroundConfirmed);
 packetFresh=true;rates.n=1;ticks(40);assert(!flightGroundConfirmed);
 rates.n=0;ticks(40);assert(flightGroundConfirmed);
 // New takeoff removes the ground state, even if low range remains.
 armed=true;thrustTarget=.5f;autoFlightPhase=1;blindDescentActive=false;ticks(1);
 assert(!flightGroundConfirmed);
}
''')

    def test_ros_wait_is_bounded_and_removes_pending_request(self):
        async def scenario():
            loop = asyncio.get_running_loop()
            cls = methods('flight_manager_node.py', 'FlightManager', {'_call_service'},
                          {'Future': lambda **kw: loop.create_future(),
                           'Lock': __import__('threading').Lock, 'Clock': lambda **kw: None,
                           'ClockType': NS(STEADY_TIME=1)})
            node = cls()
            node.executor = None
            node.client_callback_group = object()
            node.create_timer = lambda duration, callback, **kw: loop.call_later(duration, callback)
            node.destroy_timer = lambda timer: timer.cancel()
            pending = loop.create_future()
            removed = []
            client = NS(call_async=lambda req: pending, remove_pending_request=removed.append,
                        srv_name='unresponsive_service')
            with self.assertRaises(TimeoutError):
                await node._call_service(client, object(), timeout=0.01)
            self.assertTrue(pending.cancelled())
            self.assertEqual(removed, [pending])
            ready = loop.create_future()
            ready.set_result(NS(success=True))
            client.call_async = lambda req: ready
            self.assertTrue((await node._call_service(client, object(), timeout=.1)).success)
            # Cancellation also releases the timer/client request, and a late
            # completion must not revive the abandoned operation.
            pending = loop.create_future()
            client.call_async = lambda req: pending
            operation = asyncio.create_task(node._call_service(client, object(), timeout=1))
            await asyncio.sleep(0)
            operation.cancel()
            with self.assertRaises(asyncio.CancelledError):
                await operation
            self.assertTrue(pending.cancelled())
            self.assertEqual(removed[-1], pending)
        asyncio.run(scenario())

    def test_ros_emergency_has_its_own_callback_group(self):
        import ast
        tree = ast.parse((ROOT / 'ros2/open32drone_driver/flight_manager_node.py').read_text())
        groups = {}
        for call in ast.walk(tree):
            if isinstance(call, ast.Call) and isinstance(call.func, ast.Attribute) and call.func.attr == 'create_service':
                name = call.args[1].value
                group = next(k.value for k in call.keywords if k.arg == 'callback_group')
                groups[name] = ast.unparse(group)
        self.assertNotEqual(groups['flight/emergency_stop'], groups['flight/takeoff'])
        self.assertEqual(groups['flight/emergency_stop'], groups['emergency_stop'])
