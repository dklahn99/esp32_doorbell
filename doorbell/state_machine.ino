#include "types.h"

const int NUM_V_READINGS = 20;

int average(int* values, int size) {
  int sum = 0;
  for(int i = 0; i < size; i++) {
    sum += values[i];
  }
  return sum / size;
}

int get_v_avg() {
	static int readings[NUM_V_READINGS];
	static int next_to_write = 0;
	
	int v_raw = analogRead(VIN_PIN);
	readings[next_to_write] = v_raw;
	next_to_write = (next_to_write + 1) % NUM_V_READINGS;
	
	return average(readings, NUM_V_READINGS);
}

Action update_state_machine() {
	
	static State current_state = STATE_NOT_RINGING;
	static int timer = 0;

	const float RING_THRESHOLD = 1800;
	const int RING_DEBOUNCE = 100; // Milliseconds that we must spend with v < RING_THRESHOLD before switching to NOT_RINGING state
	const int POST_RING_DEBOUNCE = 500; // Milliseconds to wait after we enter the NOT_RINGING state before we can enter the RINGING state again

	Action action = ACTION_NONE;

//  static int max_val = 0;
	int v = get_v_avg();

//  if (v > max_val) {
//    max_val = v;
//    Serial.printf("New max: %d, timeSince: %d\n", max_val, timeSince(timer));
//  }
  
	switch (current_state) {
	case STATE_NOT_RINGING:
		if (v > RING_THRESHOLD && timeSince(timer) > POST_RING_DEBOUNCE) {
			current_state = STATE_RINGING;
			timer = millis();
			action = ACTION_RING;
			Serial.println("New State: STATE_RINGING");
		}
		break;
	  
	case STATE_RINGING:
		if (v < RING_THRESHOLD) {
			current_state = STATE_RINGING_DEBOUNCE;
			timer = millis();
			Serial.println("New State: STATE_RINGING_DEBOUCNE");
		}
		break;

	case STATE_RINGING_DEBOUNCE:
		if (v > RING_THRESHOLD) {
			current_state = STATE_RINGING;
			Serial.println("New State: STATE_RINGING");
		} else if (timeSince(timer) > RING_DEBOUNCE) {
			current_state = STATE_NOT_RINGING;
			timer = millis();
			Serial.println("New State: STATE_NOT_RINGING");
		}
		break;
	}

  return action;
}

int timeSince(int millisTime) {
  return millis() - millisTime;
}
