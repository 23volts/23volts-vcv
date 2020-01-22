#pragma once

#include "rack.hpp"

struct PhaseClockFollower {
	float MAX_DELTA = 1.f; // Define max resolution of phase clock following

	Input* input = NULL;

	float previousValue = 0.f;
	bool following = false;
	bool synced = false;
	bool trigger = false;

	long duration = 0; // Extrapolated duration of a cycle in samples
	float bpm = 0.f;

	void process(/*const ProcessArgs& args*/) {
		// Reinit trigger
		trigger = false;

		if(! input || ! input->isConnected()) {
			following = false;
			synced = false;
			return;
		}

		float currentValue = input->getVoltage();

		if(following) {
			float delta = fabsf(currentValue - previousValue);

	      	float absoluteDelta = fabsf(delta);
			if (absoluteDelta > 9.5f && absoluteDelta <= 10.0f) {
				trigger = true;
				// Compensate phase flip
				if (delta < 0.0f) {
					delta = 10.0f + delta;
				} else {
					delta = delta - 10.0f;
				}
			}

			if(delta > 0.0f && delta < MAX_DELTA) {
				// Limit to 
				duration = floor(10.f / delta);
				//bpm = delta / args.sampleTime * 6.0f;
				synced = true;
			}
		}
		else {
			synced = false;
		}

		previousValue = currentValue;
		following = true;
	}

	/**
	 * Get the phase duration, in samples. Returns -1 if not defined
	 * 
	 * @return [description]
	 */
	long getDuration() {
		if(! following || ! synced) return -1;

		return duration;
	}

	bool hasTriggered() {
		return trigger;
	}

	bool isSynced() {
		return synced;
	}
};
