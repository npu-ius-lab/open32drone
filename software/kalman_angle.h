// Copyright (c) 2023 Open32Drone Project

#pragma once

class KalmanAngle {
public:
	float Q_angle;
	float Q_bias;
	float R_measure;
	float angle;
	float bias;
	float rate;
	float P[2][2];

	KalmanAngle() {
		Q_angle = 0.001f;
		Q_bias = 0.003f;
		R_measure = 0.03f;
		angle = 0.0f;
		bias = 0.0f;
		P[0][0] = 0.0f;
		P[0][1] = 0.0f;
		P[1][0] = 0.0f;
		P[1][1] = 0.0f;
	}

	void setAngle(float newAngle) {
		angle = newAngle;
	}

	void setParameters(float qAngle, float qBias, float rMeasure) {
		Q_angle = qAngle;
		Q_bias = qBias;
		R_measure = rMeasure;
	}

	float getAngle(float newAngle, float newRate, float dt) {
		rate = newRate - bias;
		angle += rate * dt;

		P[0][0] += dt * (dt * P[1][1] - P[0][1] - P[1][0] + Q_angle);
		P[0][1] -= dt * P[1][1];
		P[1][0] -= dt * P[1][1];
		P[1][1] += Q_bias * dt;

		float s = P[0][0] + R_measure;
		float k0 = P[0][0] / s;
		float k1 = P[1][0] / s;
		float y = newAngle - angle;

		angle += k0 * y;
		bias += k1 * y;

		float p00 = P[0][0];
		float p01 = P[0][1];
		P[0][0] -= k0 * p00;
		P[0][1] -= k0 * p01;
		P[1][0] -= k1 * p00;
		P[1][1] -= k1 * p01;

		return angle;
	}

	float getBias() {
		return bias;
	}

	float getRate() {
		return rate;
	}
};
