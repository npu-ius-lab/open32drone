// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Work with the IMU sensor

#include <Wire.h>
#include "imu_backend.h"
#include "vector.h"
#include "lpf.h"
#include "util.h"

Open32DroneImu imu(Wire);
#define I2C_SDA 2
#define I2C_SCL 43

Vector imuRotation(PI, 0, PI / 2); // IMU orientation as Euler angles
Quaternion imuRotationInverse;

Vector gyro; // gyroscope output, rad/s
Vector gyroBias;

Vector acc; // filtered accelerometer output, m/s/s
Vector accRaw; // calibrated body-frame sample before software filtering
Vector accBias;
Vector accScale(1, 1, 1);
LowPassFilter<Vector> gyroBiasFilter(0.001); // global for IMU_GYRO_BIAS_A param
LowPassFilter<Vector> accFilter(0.02f); // rejects motor/prop vibration before attitude/height estimation
bool imuHealthy = false;
bool imuConfigured = false;
uint32_t imuLastSampleMs = 0;
float imuConfiguredRateHz = 1000.0f;
uint32_t imuMinimumSamplePeriodUs = 1000;
uint32_t imuLastDriverReadUs = 0;
bool gyroCalibrated = false;
extern bool armed;

const uint32_t GYRO_CAL_MIN_SAMPLES = 500;
const uint32_t GYRO_CAL_MIN_TIME_MS = 2000;
const float GYRO_CAL_MAX_STDDEV = radians(0.8f);
const float GYRO_CAL_MAX_ACC_STDDEV = 0.20f;
const uint32_t GYRO_CAL_TRANSIENT_WARMUP_SAMPLES = 8;
const float GYRO_CAL_TRANSIENT_LIMIT = GYRO_CAL_MAX_STDDEV * 6.0f;
// All supported drivers are configured for +-2000 deg/s above. This is only a
// sensor-health guard; DC bias is not used to decide whether the airframe moved.
const float GYRO_CONFIGURED_RANGE_DPS = 2000.0f;
const float GYRO_CAL_MAX_BIAS_FRACTION = 0.05f;
const float GYRO_CAL_MAX_SENSOR_BIAS = radians(GYRO_CONFIGURED_RANGE_DPS * GYRO_CAL_MAX_BIAS_FRACTION);

enum GyroCalibrationState { GYRO_CAL_WAITING, GYRO_CAL_COLLECTING, GYRO_CAL_COMPLETE };
GyroCalibrationState gyroCalibrationState = GYRO_CAL_WAITING;
uint32_t gyroCalibrationSamples = 0;
uint32_t gyroCalibrationStartMs = 0;
Vector gyroCalibrationMean;
Vector gyroCalibrationM2;
Vector gyroCalibrationStdDev;
Vector gyroCalibrationAccMean;
Vector gyroCalibrationAccM2;
Vector gyroCalibrationAccStdDev;
const char *gyroCalibrationReason = "WAITING";
const char *gyroCalibrationLastFailure = "NONE";
const int ACCEL_CAL_POSE_COUNT = 6;
const int ACCEL_CAL_SAMPLES_PER_POSE = 1000;
const float ACCEL_CAL_MAX_STDDEV = 0.20f;
const float ACCEL_CAL_MIN_AXIS_EXTREME = ONE_G * 0.70f;
const float ACCEL_CAL_MIN_SCALE = 0.80f;
const float ACCEL_CAL_MAX_SCALE = 1.20f;
const float ACCEL_CAL_MAX_BIAS = 2.50f;
Vector accelCalibrationPoseMean[ACCEL_CAL_POSE_COUNT];
Vector accelCalibrationPoseStdDev[ACCEL_CAL_POSE_COUNT];
const char *accelCalibrationFailure = "none";
bool accelCalibrationActive = false;

void updateIMURotation();

void setupIMU() {
  print("Setup IMU\n");
  // Initialize I2C with custom pins
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);  // Set I2C clock speed to 400kHz
	updateIMURotation();
  bool initialized = false;
  for (int i = 0; i < 5; i++) {
    if (imu.begin()) { initialized = true; break; }
    delay(100);
  }
	if (!initialized) {
		print("IMU initialization failed\n");
		return;
	}
	imuConfigured = configureIMU();
	imuHealthy = imuConfigured;
	resetGyroCalibration();
	if (imuConfigured) print("Gyro calibration: keep aircraft still for at least 2 seconds\n");
}

void updateIMURotation() {
	// Mounting angles are persistent configuration, not per-sample state. Cache
	// the inverse once so the 1 kHz sensor path does not repeat six trigonometric
	// functions and two quaternion inversions for every measurement.
	Quaternion rotation = Quaternion::fromEuler(imuRotation);
	imuRotationInverse = rotation.inversed();
}

bool configureIMU() {
  bool ok = true;
  ok &= imu.setAccelRange(imu.ACCEL_RANGE_4G);
  ok &= imu.setGyroRange(imu.GYRO_RANGE_2000DPS);
  ok &= imu.setDLPF(imu.DLPF_50HZ_APPROX);
  ok &= imu.setRate(imu.RATE_1KHZ_APPROX);
	if (ok) {
		imuConfiguredRateHz = imu.getRate();
		if (!isfinite(imuConfiguredRateHz) || imuConfiguredRateHz < 1.0f) {
			imuConfiguredRateHz = 1000.0f;
		}
		imuMinimumSamplePeriodUs =
			(uint32_t)roundf(1000000.0f / imuConfiguredRateHz);
		if (imuMinimumSamplePeriodUs == 0) imuMinimumSamplePeriodUs = 1;
		imuLastDriverReadUs = 0;
	}
  accFilter.reset();
	if (!ok) print("IMU configuration failed\n");
	return ok;
}

// The 300 Hz control scheduler already spaces normal reads farther apart than
// the configured ~1 kHz sensor period. Ground calibration calls this helper in
// blocking mode so even backends without a hardware DRDY pin cannot duplicate
// samples in a tight loop. Register access and data-ready semantics remain the
// responsibility of the selected FlixPeriph backend.
bool acquireIMUSample(bool waitForFresh) {
	if (waitForFresh && imuLastDriverReadUs != 0) {
		uint32_t elapsedUs = micros() - imuLastDriverReadUs;
		if (elapsedUs < imuMinimumSamplePeriodUs) {
			delayMicroseconds(imuMinimumSamplePeriodUs - elapsedUs);
		}
	}

	uint32_t startedUs = micros();
	do {
		if (imu.read() && imu.status() == 0) {
			imuLastDriverReadUs = micros();
			return true;
		}
		if (!waitForFresh) return false;
		delayMicroseconds(100);
	} while (micros() - startedUs < 20000U);
	return false;
}

void readIMU() {
	if (!imuConfigured) {
		imuHealthy = false;
		delay(1);
		return;
	}
	if (!acquireIMUSample(false)) {
		imuHealthy = imuLastSampleMs != 0 && millis() - imuLastSampleMs <= 50;
		return;
	}
  imu.getGyro(gyro.x, gyro.y, gyro.z);
  imu.getAccel(acc.x, acc.y, acc.z);
	if (!gyro.valid() || !acc.valid()) {
		imuHealthy = false;
		return;
	}
	imuLastSampleMs = millis();
	imuHealthy = true;
  calibrateGyroOnce();
  // apply scale and bias
  acc = (acc - accBias) / accScale;
  gyro = gyro - gyroBias;
  // rotate to body frame
  acc = Quaternion::rotateVector(acc, imuRotationInverse);
  gyro = Quaternion::rotateVector(gyro, imuRotationInverse);
  accRaw = acc;
  acc = accFilter.update(accRaw);
}

void resetGyroCalibrationWindow(const char *failure = nullptr) {
	gyroCalibrationSamples = 0;
	gyroCalibrationStartMs = 0;
	gyroCalibrationMean = Vector();
	gyroCalibrationM2 = Vector();
	gyroCalibrationAccMean = Vector();
	gyroCalibrationAccM2 = Vector();
	gyroCalibrationState = GYRO_CAL_WAITING;
	if (failure) {
		gyroCalibrationReason = failure;
		gyroCalibrationLastFailure = failure;
	}
}

void resetGyroCalibration() {
	gyroCalibrated = false;
	gyroBias = Vector();
	gyroCalibrationStdDev = Vector();
	gyroCalibrationAccStdDev = Vector();
	gyroCalibrationReason = "WAITING";
	gyroCalibrationLastFailure = "NONE";
	gyroBiasFilter.reset();
	resetGyroCalibrationWindow();
}

void updateCalibrationStats(const Vector &sample, Vector &mean, Vector &m2, uint32_t count) {
	Vector delta = sample - mean;
	mean += delta / (float)count;
	Vector delta2 = sample - mean;
	m2 += delta * delta2;
}

Vector calibrationStdDev(const Vector &m2, uint32_t count) {
	if (count < 2) return Vector();
	float divisor = count - 1;
	return Vector(
		sqrtf(max(m2.x / divisor, 0.0f)),
		sqrtf(max(m2.y / divisor, 0.0f)),
		sqrtf(max(m2.z / divisor, 0.0f)));
}

const char *getGyroCalibrationStateName() {
	switch (gyroCalibrationState) {
		case GYRO_CAL_COLLECTING: return "COLLECTING";
		case GYRO_CAL_COMPLETE: return "COMPLETE";
		default: return "WAITING";
	}
}

void calibrateGyroOnce() {
	static Delay biasAdaptDelay(5.0f);
	float accNorm = acc.norm(); // raw sensor sample; independent of six-face accel calibration

	if (gyroCalibrated) {
		if (armed) return; // freeze bias throughout flight
		bool stableForAdaptation = !motorsActive() &&
			accNorm > ONE_G * 0.75f && accNorm < ONE_G * 1.25f &&
			(gyro - gyroBias).norm() < radians(1.0f);
		if (biasAdaptDelay.update(stableForAdaptation)) gyroBias = gyroBiasFilter.update(gyro);
		return;
	}

	if (armed) return;
	if (motorsActive()) {
		resetGyroCalibrationWindow("MOTORS_ACTIVE");
		return;
	}
	if (!isfinite(accNorm) || accNorm < ONE_G * 0.70f || accNorm > ONE_G * 1.30f) {
		resetGyroCalibrationWindow("ACC_NORM");
		return;
	}
	// Reject changes relative to this window, not the absolute reading. A stable
	// DC offset is the bias being measured and varies across IMU models/units.
	if (gyroCalibrationSamples >= GYRO_CAL_TRANSIENT_WARMUP_SAMPLES &&
		(gyro - gyroCalibrationMean).norm() > GYRO_CAL_TRANSIENT_LIMIT) {
		resetGyroCalibrationWindow("MOVING");
		return;
	}

	if (gyroCalibrationSamples == 0) {
		gyroCalibrationStartMs = millis();
		gyroCalibrationState = GYRO_CAL_COLLECTING;
		gyroCalibrationReason = "COLLECTING";
	}
	gyroCalibrationSamples++;
	updateCalibrationStats(gyro, gyroCalibrationMean, gyroCalibrationM2, gyroCalibrationSamples);
	updateCalibrationStats(acc, gyroCalibrationAccMean, gyroCalibrationAccM2, gyroCalibrationSamples);

	if (gyroCalibrationSamples < GYRO_CAL_MIN_SAMPLES ||
		millis() - gyroCalibrationStartMs < GYRO_CAL_MIN_TIME_MS) return;

	gyroCalibrationStdDev = calibrationStdDev(gyroCalibrationM2, gyroCalibrationSamples);
	gyroCalibrationAccStdDev = calibrationStdDev(gyroCalibrationAccM2, gyroCalibrationSamples);
	float gyroNoise = max(max(gyroCalibrationStdDev.x, gyroCalibrationStdDev.y), gyroCalibrationStdDev.z);
	float accNoise = max(max(gyroCalibrationAccStdDev.x, gyroCalibrationAccStdDev.y), gyroCalibrationAccStdDev.z);
	float meanAccNorm = gyroCalibrationAccMean.norm();
	float maxSensorBias = max(max(fabsf(gyroCalibrationMean.x), fabsf(gyroCalibrationMean.y)),
		fabsf(gyroCalibrationMean.z));
	if (maxSensorBias > GYRO_CAL_MAX_SENSOR_BIAS) {
		resetGyroCalibrationWindow("SENSOR_BIAS");
		return;
	}
	if (gyroNoise > GYRO_CAL_MAX_STDDEV) {
		resetGyroCalibrationWindow("GYRO_NOISE");
		return;
	}
	if (accNoise > GYRO_CAL_MAX_ACC_STDDEV ||
		meanAccNorm < ONE_G * 0.75f || meanAccNorm > ONE_G * 1.25f) {
		resetGyroCalibrationWindow("ACC_NOISE");
		return;
	}

	gyroBias = gyroCalibrationMean;
	gyroBiasFilter.reset();
	gyroBiasFilter.update(gyroBias); // initialize optional disarmed temperature tracking
	gyroCalibrated = true;
	gyroCalibrationState = GYRO_CAL_COMPLETE;
	gyroCalibrationReason = "COMPLETE";
	print("Gyro calibration complete: %u samples, bias %.5f %.5f %.5f\n",
		gyroCalibrationSamples, gyroBias.x, gyroBias.y, gyroBias.z);
}

void printGyroCalibrationStatus() {
	uint32_t elapsed = gyroCalibrationStartMs == 0 ? 0 : millis() - gyroCalibrationStartMs;
	print("gyro calibrated: %d\n", gyroCalibrated);
	print("gyro calibration: %s samples %u/%u elapsed %ums reason %s last failure %s\n",
		getGyroCalibrationStateName(), gyroCalibrationSamples, GYRO_CAL_MIN_SAMPLES,
		elapsed, gyroCalibrationReason, gyroCalibrationLastFailure);
	print("gyro calibration stddev: %.6f %.6f %.6f rad/s\n",
		gyroCalibrationStdDev.x, gyroCalibrationStdDev.y, gyroCalibrationStdDev.z);
	print("acc calibration stddev: %.4f %.4f %.4f m/s2\n",
		gyroCalibrationAccStdDev.x, gyroCalibrationAccStdDev.y, gyroCalibrationAccStdDev.z);
}

bool collectAccelCalibrationPose(Vector &mean, Vector &stddev) {
	mean = Vector();
	Vector m2;
	int samples = 0;
	uint32_t started = millis();
	while (samples < ACCEL_CAL_SAMPLES_PER_POSE && millis() - started < 3000) {
		if (!acquireIMUSample(true)) continue;
		Vector sample;
		imu.getAccel(sample.x, sample.y, sample.z);
		if (!sample.valid()) continue;
		samples++;
		Vector delta = sample - mean;
		mean += delta / samples;
		Vector delta2 = sample - mean;
		m2 += delta * delta2;
	}
	if (samples < ACCEL_CAL_SAMPLES_PER_POSE) {
		accelCalibrationFailure = "insufficient fresh IMU samples";
		return false;
	}
	float divisor = max(samples - 1, 1);
	stddev = Vector(
		sqrtf(max(m2.x / divisor, 0.0f)),
		sqrtf(max(m2.y / divisor, 0.0f)),
		sqrtf(max(m2.z / divisor, 0.0f)));
	if (!mean.valid() || !stddev.valid()) {
		accelCalibrationFailure = "non-finite calibration sample";
		return false;
	}
	if (stddev.x > ACCEL_CAL_MAX_STDDEV || stddev.y > ACCEL_CAL_MAX_STDDEV ||
		stddev.z > ACCEL_CAL_MAX_STDDEV) {
		accelCalibrationFailure = "aircraft moved during calibration";
		return false;
	}
	float gravity = mean.norm();
	if (!isfinite(gravity) || gravity < ONE_G * 0.75f || gravity > ONE_G * 1.25f) {
		accelCalibrationFailure = "measured gravity is out of range";
		return false;
	}
	return true;
}

bool validateAccelCalibration(Vector &candidateBias, Vector &candidateScale) {
	Vector maximum(-INFINITY, -INFINITY, -INFINITY);
	Vector minimum(INFINITY, INFINITY, INFINITY);
	for (int i = 0; i < ACCEL_CAL_POSE_COUNT; i++) {
		const Vector &sample = accelCalibrationPoseMean[i];
		for (int j = 0; j < i; j++) {
			float similarity = Vector::dot(sample, accelCalibrationPoseMean[j]) /
				(sample.norm() * accelCalibrationPoseMean[j].norm());
			if (!isfinite(similarity) || similarity > 0.85f) {
				accelCalibrationFailure = "duplicate or incomplete face coverage";
				return false;
			}
		}
		maximum.x = max(maximum.x, sample.x);
		maximum.y = max(maximum.y, sample.y);
		maximum.z = max(maximum.z, sample.z);
		minimum.x = min(minimum.x, sample.x);
		minimum.y = min(minimum.y, sample.y);
		minimum.z = min(minimum.z, sample.z);
	}
	if (maximum.x < ACCEL_CAL_MIN_AXIS_EXTREME || maximum.y < ACCEL_CAL_MIN_AXIS_EXTREME ||
		maximum.z < ACCEL_CAL_MIN_AXIS_EXTREME || minimum.x > -ACCEL_CAL_MIN_AXIS_EXTREME ||
		minimum.y > -ACCEL_CAL_MIN_AXIS_EXTREME || minimum.z > -ACCEL_CAL_MIN_AXIS_EXTREME) {
		accelCalibrationFailure = "six faces do not cover every axis";
		return false;
	}
	candidateScale = (maximum - minimum) / (2.0f * ONE_G);
	candidateBias = (maximum + minimum) / 2.0f;
	if (!candidateScale.valid() || !candidateBias.valid() ||
		candidateScale.x < ACCEL_CAL_MIN_SCALE || candidateScale.x > ACCEL_CAL_MAX_SCALE ||
		candidateScale.y < ACCEL_CAL_MIN_SCALE || candidateScale.y > ACCEL_CAL_MAX_SCALE ||
		candidateScale.z < ACCEL_CAL_MIN_SCALE || candidateScale.z > ACCEL_CAL_MAX_SCALE ||
		abs(candidateBias.x) > ACCEL_CAL_MAX_BIAS || abs(candidateBias.y) > ACCEL_CAL_MAX_BIAS ||
		abs(candidateBias.z) > ACCEL_CAL_MAX_BIAS) {
		accelCalibrationFailure = "computed bias or scale is implausible";
		return false;
	}
	return true;
}

void calibrateAccel() {
	if (armed) { print("Accelerometer calibration rejected while armed\n"); return; }
	if (accelCalibrationActive) {
		print("Accelerometer calibration already active\n");
		return;
	}
	accelCalibrationActive = true;
	print("Calibrating accelerometer: all six distinct faces must remain still\n");
	Vector previousBias = accBias;
	Vector previousScale = accScale;
	accelCalibrationFailure = "none";
	bool success = imu.setAccelRange(imu.ACCEL_RANGE_2G);
	if (!success) accelCalibrationFailure = "unable to select accelerometer calibration range";

	const char *prompts[ACCEL_CAL_POSE_COUNT] = {
		"1/6 Place level [8 sec]\n",
		"2/6 Place nose up [8 sec]\n",
		"3/6 Place nose down [8 sec]\n",
		"4/6 Place on right side [8 sec]\n",
		"5/6 Place on left side [8 sec]\n",
		"6/6 Place upside down [8 sec]\n",
	};
	for (int i = 0; success && i < ACCEL_CAL_POSE_COUNT; i++) {
		print("%s", prompts[i]);
		pause(8);
		success = collectAccelCalibrationPose(
			accelCalibrationPoseMean[i], accelCalibrationPoseStdDev[i]);
		if (success) {
			print("face %d mean %.3f %.3f %.3f stddev %.3f %.3f %.3f\n", i + 1,
				accelCalibrationPoseMean[i].x, accelCalibrationPoseMean[i].y,
				accelCalibrationPoseMean[i].z, accelCalibrationPoseStdDev[i].x,
				accelCalibrationPoseStdDev[i].y, accelCalibrationPoseStdDev[i].z);
		}
	}

	Vector candidateBias;
	Vector candidateScale;
	if (success) success = validateAccelCalibration(candidateBias, candidateScale);
	if (success) {
		accBias = candidateBias;
		accScale = candidateScale;
		printIMUCalibration();
		print("Accelerometer calibration accepted\n");
	} else {
		accBias = previousBias;
		accScale = previousScale;
		print("Accelerometer calibration rejected: %s; previous calibration restored\n",
			accelCalibrationFailure);
	}
	accelCalibrationActive = false;
	imuConfigured = configureIMU();
	imuHealthy = imuConfigured;
	resetGyroCalibration(); // sensor configuration changed; acquire a fresh gyro bias
}

void printIMUCalibration() {
  print("gyro bias: %f %f %f\n", gyroBias.x, gyroBias.y, gyroBias.z);
  print("accel bias: %f %f %f\n", accBias.x, accBias.y, accBias.z);
  print("accel scale: %f %f %f\n", accScale.x, accScale.y, accScale.z);
}

void printIMUInfo() {
  imu.status() ? print("status: ERROR %d\n", imu.status()) : print("status: OK\n");
  print("model: %s\n", imu.getModel());
  print("backend: %s\n", OPEN32DRONE_IMU_BACKEND_NAME);
  print("who am I: 0x%02X\n", imu.whoAmI());
  print("sensor rate: %.0f Hz\n", imuConfiguredRateHz);
  print("rate: %.0f\n", loopRate);
  print("gyro: %f %f %f\n", rates.x, rates.y, rates.z);
  print("acc filtered: %f %f %f (norm %.2f)\n", acc.x, acc.y, acc.z, acc.norm());
  print("acc body raw: %f %f %f (norm %.2f)\n", accRaw.x, accRaw.y, accRaw.z, accRaw.norm());
  acquireIMUSample(true);
  Vector rawGyro, rawAcc;
  imu.getGyro(rawGyro.x, rawGyro.y, rawGyro.z);
  imu.getAccel(rawAcc.x, rawAcc.y, rawAcc.z);
  print("raw gyro: %f %f %f\n", rawGyro.x, rawGyro.y, rawGyro.z);
  print("raw acc: %f %f %f\n", rawAcc.x, rawAcc.y, rawAcc.z);
	printGyroCalibrationStatus();
}
