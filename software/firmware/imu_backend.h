#pragma once

#include <FlixPeriph.h>

// Select exactly one tested driver implementation at build time.  The flight
// controller below this file only uses the common FlixPeriph IMU interface, so
// adding a sensor does not spread register-specific code through estimation or
// control.
#define OPEN32DRONE_IMU_MPU9250 1
#define OPEN32DRONE_IMU_ICM20948 2
#define OPEN32DRONE_IMU_MPU6050 3

#ifndef OPEN32DRONE_IMU_BACKEND
#define OPEN32DRONE_IMU_BACKEND OPEN32DRONE_IMU_MPU9250
#endif

#if OPEN32DRONE_IMU_BACKEND == OPEN32DRONE_IMU_MPU9250
using Open32DroneImu = MPU9250; // also accepts MPU6500/MPU9255 identities
#define OPEN32DRONE_IMU_BACKEND_NAME "MPU6500/MPU9250"
#elif OPEN32DRONE_IMU_BACKEND == OPEN32DRONE_IMU_ICM20948
using Open32DroneImu = ICM20948;
#define OPEN32DRONE_IMU_BACKEND_NAME "ICM20948"
#elif OPEN32DRONE_IMU_BACKEND == OPEN32DRONE_IMU_MPU6050
using Open32DroneImu = MPU6050;
#define OPEN32DRONE_IMU_BACKEND_NAME "MPU6050"
#else
#error "Unsupported OPEN32DRONE_IMU_BACKEND"
#endif
