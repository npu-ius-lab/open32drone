// Copyright (c) 2023 Open32Drone Project
// 两状态卡尔曼滤波器 (2-State Kalman Filter) 用于姿态解算
// 同时估计角度(Angle)和陀螺仪零偏(Gyro Bias)

#ifndef KALMAN_ANGLE_H
#define KALMAN_ANGLE_H

class KalmanAngle {
public:
    float Q_angle;   // 角度过程噪声协方差 (相信陀螺仪积分的程度)
    float Q_bias;    // 零偏过程噪声协方差 (相信零偏变化的程度)
    float R_measure; // 测量噪声协方差 (相信加速度计的程度，大=不信，小=信)

    float angle; // 计算出的角度
    float bias;  // 计算出的陀螺仪零偏
    float rate;  // 无偏角速度

    float P[2][2]; // 误差协方差矩阵

    KalmanAngle() {
        Q_angle = 0.001f;
        Q_bias = 0.003f;
        R_measure = 0.03f;

        angle = 0.0f;
        bias = 0.0f;

        P[0][0] = 0.0f; P[0][1] = 0.0f;
        P[1][0] = 0.0f; P[1][1] = 0.0f;
    }

    // 设置初始角度 (用于初始化)
    void setAngle(float newAngle) {
        angle = newAngle;
    }

    // 设置滤波器参数
    void setParameters(float q_angle, float q_bias, float r_measure) {
        Q_angle = q_angle;
        Q_bias = q_bias;
        R_measure = r_measure;
    }

    // dt: 循环时间 (秒)
    // newAngle: 加速度计计算出的当前角度 (Measurement)
    // newRate: 陀螺仪读数 (Input)
    float getAngle(float newAngle, float newRate, float dt) {
        // --------------------------------------------------------
        // 1. 预测步 (Predict)
        // --------------------------------------------------------
        rate = newRate - bias;
        angle += rate * dt;

        // 更新估计误差协方差矩阵 P
        P[0][0] += dt * (dt * P[1][1] - P[0][1] - P[1][0] + Q_angle);
        P[0][1] -= dt * P[1][1];
        P[1][0] -= dt * P[1][1];
        P[1][1] += Q_bias * dt;

        // --------------------------------------------------------
        // 2. 更新步 (Update)
        // --------------------------------------------------------
        // 计算卡尔曼增益 K
        float S = P[0][0] + R_measure; // 估计误差 + 测量噪声
        float K[2]; // Kalman gain
        K[0] = P[0][0] / S;
        K[1] = P[1][0] / S;

        // 计算残差 (Measurement innovation)
        float y = newAngle - angle;

        // 更新状态 (角度和零偏)
        angle += K[0] * y;
        bias += K[1] * y;

        // 更新后验估计误差协方差 P
        float P00_temp = P[0][0];
        float P01_temp = P[0][1];

        P[0][0] -= K[0] * P00_temp;
        P[0][1] -= K[0] * P01_temp;
        P[1][0] -= K[1] * P00_temp;
        P[1][1] -= K[1] * P01_temp;

        return angle;
    }

    // 获取当前估计的陀螺仪零偏
    float getBias() {
        return bias;
    }

    // 获取无偏角速度
    float getRate() {
        return rate;
    }
};

#endif // KALMAN_ANGLE_H
