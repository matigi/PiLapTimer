#include "imu_qmi8658.h"

#include "QMI8658.h"

bool imu_qmi8658_init() {
  return QMI8658_init() == 1;
}

bool imu_qmi8658_read_accel(float &ax, float &ay, float &az) {
  float acc[3] = {0.0f, 0.0f, 0.0f};
  QMI8658_read_acc_xyz(acc);
  ax = acc[0];
  ay = acc[1];
  az = acc[2];
  return true;
}

bool imu_qmi8658_read_gyro(float &gx, float &gy, float &gz) {
  float gyro[3] = {0.0f, 0.0f, 0.0f};
  QMI8658_read_gyro_xyz(gyro);
  gx = gyro[0];
  gy = gyro[1];
  gz = gyro[2];
  return true;
}
