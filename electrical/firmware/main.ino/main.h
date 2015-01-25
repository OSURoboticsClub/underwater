// Define states
enum STATE {
  idle,
  talk,
  run,
  error
} current_state, commanded_state;

int watchdog_counter;
