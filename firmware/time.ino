// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix

// Time related functions

#include <esp_timer.h>

uint64_t monotonicTimeUs = 0; // monotonic time since boot, us
double t = NAN; // current monotonic time, s
float dt; // time delta with the previous step, s
float loopRate; // Hz
float loopDtAverageMs;
float loopDtMaxMs;
float loopDtP95Ms;
float loopDtP99Ms;
constexpr uint32_t CONTROL_LOOP_TARGET_HZ = 300;
constexpr uint32_t CONTROL_LOOP_PERIOD_US = 1000000U / CONTROL_LOOP_TARGET_HZ;
uint32_t controlLoopTickCount;
uint32_t loopDeadlineMissCount;
uint32_t loopDeadlineMaxLatenessUs;
uint32_t loopOver5msCount;
uint32_t loopOver10msCount;
uint32_t pendingDeadlineMissCount;
uint32_t pendingDeadlineMaxLatenessUs;

struct PerformanceAccumulator {
	uint64_t totalUs;
	uint32_t maxUs;
	uint32_t samples;
};

PerformanceAccumulator performanceStages[PERF_STAGE_COUNT];
PerformanceAccumulator performanceTotal;
bool performanceSampleActive = false;
uint64_t performanceCycleStartUs = 0;
uint64_t performanceStageStartUs = 0;
uint32_t performanceCycleCounter = 0;

const char *performanceStageName(PerformanceStage stage) {
	switch (stage) {
		case PERF_IMU: return "imu acquire";
		case PERF_INPUTS: return "rc/flow/time";
		case PERF_ESTIMATE: return "estimators";
		case PERF_CONTROL: return "control/motors";
		case PERF_CLI: return "serial cli";
		case PERF_MAVLINK: return "mavlink/ota";
		case PERF_HOUSEKEEPING: return "power/log/nvs";
		default: return "unknown";
	}
}

// Start complete control iterations on a deterministic 300 Hz schedule. A late
// iteration resets the phase to "now" instead of running catch-up bursts.
void waitForControlLoopTick() {
	static uint64_t previousStartUs = 0;
	uint64_t nowUs = (uint64_t)esp_timer_get_time();
	if (previousStartUs != 0) {
		uint64_t deadlineUs = previousStartUs + CONTROL_LOOP_PERIOD_US;
		if (nowUs <= deadlineUs) {
			if (nowUs < deadlineUs) {
				delayMicroseconds((uint32_t)(deadlineUs - nowUs));
			}
			previousStartUs = deadlineUs;
		} else {
			uint32_t latenessUs = (uint32_t)min(
				nowUs - deadlineUs, (uint64_t)UINT32_MAX);
			pendingDeadlineMissCount++;
			pendingDeadlineMaxLatenessUs = max(
				pendingDeadlineMaxLatenessUs, latenessUs);
			previousStartUs = nowUs;
		}
	} else {
		previousStartUs = nowUs;
	}
	controlLoopTickCount++;
}

bool controlLoopEvery(uint32_t divisor) {
	return divisor != 0 && controlLoopTickCount % divisor == 0;
}

void beginPerformanceCycle() {
	performanceCycleCounter++;
	performanceSampleActive = (performanceCycleCounter & 0x0fU) == 0;
	if (!performanceSampleActive) return;
	performanceCycleStartUs = (uint64_t)esp_timer_get_time();
	performanceStageStartUs = performanceCycleStartUs;
}

void markPerformanceStage(PerformanceStage stage) {
	if (!performanceSampleActive || stage < 0 || stage >= PERF_STAGE_COUNT) return;
	uint64_t nowUs = (uint64_t)esp_timer_get_time();
	uint32_t elapsedUs = (uint32_t)(nowUs - performanceStageStartUs);
	PerformanceAccumulator &stats = performanceStages[stage];
	stats.totalUs += elapsedUs;
	stats.maxUs = max(stats.maxUs, elapsedUs);
	stats.samples++;
	performanceStageStartUs = nowUs;
}

void endPerformanceCycle() {
	if (!performanceSampleActive) return;
	uint64_t nowUs = (uint64_t)esp_timer_get_time();
	uint32_t elapsedUs = (uint32_t)(nowUs - performanceCycleStartUs);
	performanceTotal.totalUs += elapsedUs;
	performanceTotal.maxUs = max(performanceTotal.maxUs, elapsedUs);
	performanceTotal.samples++;
	performanceSampleActive = false;
}

void resetPerformanceInfo() {
	memset(performanceStages, 0, sizeof(performanceStages));
	performanceTotal = {};
	performanceSampleActive = false;
	performanceCycleStartUs = 0;
	performanceStageStartUs = 0;
	performanceCycleCounter = 0;
	pendingDeadlineMissCount = 0;
	pendingDeadlineMaxLatenessUs = 0;
	loopDeadlineMissCount = 0;
	loopDeadlineMaxLatenessUs = 0;
}

void printPerformanceInfo() {
	print("Loop: %.0fHz avg %.3fms p95 %.3fms p99 %.3fms max %.3fms\n",
		loopRate, loopDtAverageMs, loopDtP95Ms, loopDtP99Ms, loopDtMaxMs);
	print("Schedule: target %uHz misses %u max late %uus; >5ms %u >10ms %u (last 1s)\n",
		CONTROL_LOOP_TARGET_HZ, loopDeadlineMissCount, loopDeadlineMaxLatenessUs,
		loopOver5msCount, loopOver10msCount);
	print("Sampled stage cost (1/16 loops, %u samples):\n", performanceTotal.samples);
	for (int i = 0; i < PERF_STAGE_COUNT; i++) {
		const PerformanceAccumulator &stats = performanceStages[i];
		float meanUs = stats.samples ? (float)stats.totalUs / stats.samples : 0.0f;
		print("  %-16s mean %7.1fus max %7uus samples %u\n",
			performanceStageName((PerformanceStage)i), meanUs, stats.maxUs, stats.samples);
	}
	float totalMeanUs = performanceTotal.samples ?
		(float)performanceTotal.totalUs / performanceTotal.samples : 0.0f;
	print("  %-16s mean %7.1fus max %7uus samples %u\n",
		"sampled total", totalMeanUs, performanceTotal.maxUs, performanceTotal.samples);
}

float percentileFromHistogram(const uint32_t *histogram, uint32_t samples, float percentile) {
	if (samples == 0) return 0.0f;
	uint32_t rank = max((uint32_t)1, (uint32_t)ceilf(samples * percentile));
	uint32_t cumulative = 0;
	for (int i = 0; i < 64; i++) {
		cumulative += histogram[i];
		if (cumulative >= rank) return (i + 1) * 0.25f;
	}
	return 16.0f;
}

void step() {
	static uint64_t previousTimeUs = 0;
	monotonicTimeUs = (uint64_t)esp_timer_get_time();
	t = monotonicTimeUs / 1000000.0;
	if (previousTimeUs != 0 && monotonicTimeUs > previousTimeUs) {
		dt = (monotonicTimeUs - previousTimeUs) / 1000000.0f;
	} else {
		dt = 0.0f; // first step or an unexpected timer reset
	}
	previousTimeUs = monotonicTimeUs;

	computeLoopRate();
}

void computeLoopRate() {
	static double windowStart = 0.0;
	static uint32_t rate = 0;
	static uint32_t dtSamples = 0;
	static float dtSum = 0.0f;
	static float dtMax = 0.0f;
	static uint32_t over5ms = 0;
	static uint32_t over10ms = 0;
	static uint32_t histogram[64] = {};
	rate++;
	if (dt > 0.0f) {
		dtSamples++;
		dtSum += dt;
		dtMax = max(dtMax, dt);
		float dtMs = dt * 1000.0f;
		// Each bucket stores (previous boundary, current boundary], so a loop
		// exactly on a 1.00 ms boundary reports 1.00 ms rather than 1.25 ms.
		int bucket = constrain((int)ceilf(dtMs / 0.25f) - 1, 0, 63);
		histogram[bucket]++;
		if (dt > 0.005f) over5ms++; // control loop slower than 200 Hz
		if (dt > 0.010f) over10ms++;
	}
	if (t - windowStart >= 1) { // 1 second window
		loopRate = rate;
		loopDtAverageMs = dtSamples > 0 ? dtSum * 1000.0f / dtSamples : 0.0f;
		loopDtMaxMs = dtMax * 1000.0f;
		loopDtP95Ms = percentileFromHistogram(histogram, dtSamples, 0.95f);
		loopDtP99Ms = percentileFromHistogram(histogram, dtSamples, 0.99f);
		loopDeadlineMissCount = pendingDeadlineMissCount;
		loopDeadlineMaxLatenessUs = pendingDeadlineMaxLatenessUs;
		loopOver5msCount = over5ms;
		loopOver10msCount = over10ms;
		pendingDeadlineMissCount = 0;
		pendingDeadlineMaxLatenessUs = 0;
		windowStart = t;
		rate = 0;
		dtSamples = 0;
		dtSum = 0.0f;
		dtMax = 0.0f;
		over5ms = 0;
		over10ms = 0;
		memset(histogram, 0, sizeof(histogram));
	}
}
