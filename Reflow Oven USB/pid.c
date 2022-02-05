#include <stdio.h>

#include "PID.h"

void PIDController_Init(PIDController *pid) {
	// Clear controller variables
	pid->integrator = 0.0f;
	pid->prevError  = 0.0f;

	pid->differentiator  = 0.0f;
	pid->prevMeasurement = 0.0f;

	pid->out = 0;
}

uint16_t PIDController_Update(PIDController *pid, uint16_t setpoint, uint16_t measurement) {
	// Error signal
  float error = ((float)setpoint*4.0) - (float)measurement;


	// Proportional
  pid->proportional = pid->Kp * error;

	// Integral
  pid->integrator = pid->integrator + 0.5f * pid->Ki * pid->T * (error + pid->prevError);

	// Anti-wind-up via integrator clamping
  if (pid->integrator > pid->limMaxInt)
	{
    pid->integrator = pid->limMaxInt;
  } 
	else if (pid->integrator < pid->limMinInt) 
	{
    pid->integrator = pid->limMinInt;
	}

	// Derivative (band-limited differentiator)
  pid->differentiator = -(2.0f * pid->Kd * (measurement - pid->prevMeasurement)	// Note: derivative on measurement, therefore minus sign in front of equation!
                        + (2.0f * pid->tau - pid->T) * pid->differentiator)
                        / (2.0f * pid->tau + pid->T);

	// Compute output and apply limits
	float result = pid->proportional + pid->integrator + pid->differentiator;

  if (result > pid->limMax) 
	{
    pid->out = (uint16_t)pid->limMax;
  } 
	else if (result < pid->limMin) 
	{
    pid->out = (uint16_t)pid->limMin;
  }
	else
	{
		pid->out = (uint16_t)result;
	}

	// Store error and measurement for later use
  pid->prevError       = error;
  pid->prevMeasurement = measurement;

	// Return controller output
  return pid->out;
}