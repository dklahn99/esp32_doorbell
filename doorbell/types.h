#ifndef TYPES_H
#define TYPES_H

const int VIN_PIN = 32;
const int AUDIO_ENABLE_PIN = 33;

enum State {
  STATE_NOT_RINGING,
  STATE_RINGING,
  STATE_RINGING_DEBOUNCE,
};

enum Action {
  ACTION_NONE,
  ACTION_RING,
};

#endif
