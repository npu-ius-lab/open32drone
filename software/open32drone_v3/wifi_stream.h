// Frame-rate filter for the camera stream (from ESP32-CAM example).
//
// Kept in a header (not in wifi.ino) on purpose: the Arduino build system
// auto-generates prototypes for every function defined in a .ino and hoists
// them to the top of the merged translation unit. A prototype like
//   static int ra_filter_run(ra_filter_t *filter, int value);
// would then reference ra_filter_t before its typedef is seen, causing
// "'ra_filter_t' was not declared in this scope". Headers are not scanned
// by that prototype generator, so the typedef stays visible.

#ifndef WIFI_STREAM_H
#define WIFI_STREAM_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	size_t size;
	size_t index;
	size_t count;
	int sum;
	int *values;
} ra_filter_t;

static int ra_filter_run(ra_filter_t *filter, int value) {
	if (!filter->values) {
		filter->values = (int *)malloc(filter->size * sizeof(int));
		if (!filter->values) {
			return value;
		}
		memset(filter->values, 0, filter->size * sizeof(int));
	}

	filter->sum -= filter->values[filter->index];
	filter->values[filter->index] = value;
	filter->sum += filter->values[filter->index];
	filter->index++;
	filter->index = filter->index % filter->size;
	if (filter->count < filter->size) {
		filter->count++;
	}
	return filter->sum / filter->count;
}

#endif // WIFI_STREAM_H
