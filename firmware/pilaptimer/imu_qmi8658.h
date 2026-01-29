#ifndef IMU_QMI8658_H
#define IMU_QMI8658_H

#include <stdbool.h>

// Initializes the QMI8658 IMU. Returns true when the device is ready.
bool imu_qmi8658_init();

// Reads accelerometer values in m/s^2.
// Returns true when the read succeeds.
bool imu_qmi8658_read_accel(float &ax, float &ay, float &az);

// Reads gyroscope values in dps.
// Returns true when the read succeeds.
bool imu_qmi8658_read_gyro(float &gx, float &gy, float &gz);

#endif
