// Independent high-priority timer watches completion, not entry into loop().
#include <esp_timer.h>
#include <esp_system.h>
#include <driver/ledc.h>

static esp_timer_handle_t controlWatchdogTimer = nullptr;
static uint32_t controlWatchdogLastMs = 0;
static bool controlWatchdogArmed = false;
bool controlWatchdogFault = false;
bool controlWatchdogReady = false;
const uint32_t CONTROL_WATCHDOG_TIMEOUT_MS = 100;

void controlWatchdogTick(void *) {
	if (!__atomic_load_n(&controlWatchdogArmed, __ATOMIC_ACQUIRE)) return;
	uint32_t age = millis() - __atomic_load_n(&controlWatchdogLastMs, __ATOMIC_ACQUIRE);
	if (age <= CONTROL_WATCHDOG_TIMEOUT_MS) return;
	__atomic_store_n(&controlWatchdogFault, true, __ATOMIC_RELEASE);
	// Stop the hardware generator before resetting; don't wait for the stuck loop.
	for (int channel = 1; channel <= 4; ++channel) {
		ledc_stop(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel, 0);
	}
	esp_restart();
}

void setupControlLoopWatchdog() {
	esp_timer_create_args_t args = {};
	args.callback = controlWatchdogTick;
	args.name = "flight-loop";
	controlWatchdogReady = esp_timer_create(&args, &controlWatchdogTimer) == ESP_OK &&
		esp_timer_start_periodic(controlWatchdogTimer, 10000) == ESP_OK;
}

void armControlLoopWatchdog() {
	__atomic_store_n(&controlWatchdogLastMs, millis(), __ATOMIC_RELEASE);
	__atomic_store_n(&controlWatchdogArmed, true, __ATOMIC_RELEASE);
}

void disarmControlLoopWatchdog() {
	__atomic_store_n(&controlWatchdogArmed, false, __ATOMIC_RELEASE);
}

void completeControlLoopWatchdog() {
	__atomic_store_n(&controlWatchdogLastMs, millis(), __ATOMIC_RELEASE);
}
