#include <MIDI.h>
MIDI_CREATE_DEFAULT_INSTANCE();

/* Coup De Foudre - Adapted heavily from       */
/* 4 Note Interrupter http://adammunich.com    */
/* Run sketch at 96MHz on a teensy 3.1 or 3.2  */

/* PIN CONNECTIONS ---------- ||
LCD RS -> D2
LCD EN -> D4
LCD D0-3 -> GND
LCD D4 -> D5
LCD D6 -> D6
LCD D7 -> D7
LCD D8 -> D8


Mode Switches --------------- ||
MIDI (high) / USB-MIDI (low) -> D10
Pulse Mode (high) - > D9


Output ---------------------- ||
Pulses -> D21
( use a transistor and 120 ohm series res off of      )
( 5V bus to drive hfbr-1414tz or similar transmitter. )
( Buy extras it sucks when they break. )


10k Potentiometers ---------- ||
Pulse Width / Power Pot
GND --\/\/\/\/\-- 3v3
         | A0

Pulse Duty Cycle Pot
GND --/\/\/\/\/-- 3V3
         | A1


INPUTS ----------------------- ||
USB -> MicroUSB port
Optoisolated Midi RX -> D0 (See http://bit.ly/2a6BQgA )

*/


// Allocate all the teensy timers, put them in an array
IntervalTimer timer_0, timer_1, timer_2, timer_3;

float pulse_mod_array[4] = {1, 1, 1, 1};
float pulse_0_modifier = 1;
float pulse_1_modifier = 1;
float pulse_2_modifier = 1;
float pulse_3_modifier = 1;

#define midi_mode_switch    10
#define pulse_mode_switch   9
#define estop_switch        11

#define channel_1_out       22
#define channel_2_out       22 

#define pulsewidth_pot A1
#define duty_cycle_pot A0

// 88 Key keyboard CONSTANTS ------------------
#define NOTE_MIN 21
#define NOTE_MAX 108

#define VERSION "1.2.3"

// Values for bipolar/theophany
#define COILNAME "Cfg: Theophany"
#define PULSEWIDTH_MIN 5
#define PULSEWIDTH_MAX 100

// Values for Orage and 2014 coil 
// #define COILNAME "Cfg: Orage"
// #define PULSEWIDTH_MIN 35
// #define PULSEWIDTH_MAX 250

int clamp_pulse_width(float nominal_width) {
  return (int) constrain(nominal_width, PULSEWIDTH_MIN, PULSEWIDTH_MAX);
}

volatile uint32_t pulse_period          = 10000; // NOTE(meawoppl) - units of microseconds
volatile uint32_t old_pulse_period      = 0;
volatile uint16_t interrupter_pulsewidth_setpoint    = 100;
volatile uint8_t  system_mode                        = 0;     // System Mode, 2 = USB, 1 = MIDI-RX, 0 = Clock

// In the below set of callbacks, we map the triple (c, p, v)
// to an individual timer keyed on p.  This way multiple notes
// with the same pitch won't take multiple timer slots
#define MY_CHANNEL 1
#define TIMER_COUNT 4

// Note scheduling array,
// index is 1:1 with timer
// contents store the velocity
// Storage of 255 indicates an EMPTY SLOT
uint8_t note_channel[TIMER_COUNT] = {0, 0, 0, 0};
uint8_t note_pitch[TIMER_COUNT] = {255, 255, 255, 255};
uint8_t note_velocity[TIMER_COUNT] = {0, 0, 0, 0};

#include "midi_constants.h"
#include "display.h"
#include "entropy.h"

const uint8_t  ADC_RESOLUTION = 7;
const uint16_t ANALOG_SCALE_MAX = pow(2, ADC_RESOLUTION) - 2;
const uint8_t  ANALOG_SCALE_MIN = 0;

const float  MODIFIER_MAX = 1;
const float  MODIFIER_MIN = midi_freq[NOTE_MIN] / midi_freq[NOTE_MAX];

float mapf(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// Enable the below to get a jankey vis of what midi notes
// are coming into the system
// #define DEBUGMIDI
void debug_thing(int thing) {
  vfd.setCursor(0, 0);
  vfd.print("        ");
  vfd.setCursor(0, 0);
  vfd.print("V: ");
  vfd.print(thing);
}

void setup() {
  /* Pullups, impedances etc */
  pinMode(channel_1_out,     OUTPUT);
  pinMode(channel_2_out,     OUTPUT);
 
  pinMode(midi_mode_switch,  INPUT);
  pinMode(pulse_mode_switch, INPUT);
  pinMode(estop_switch,      INPUT_PULLUP);

  pinMode(pulsewidth_pot,    INPUT);
  pinMode(duty_cycle_pot,    INPUT);

  analogReadResolution(ADC_RESOLUTION);
  analogReadAveraging(128);

  /* Init  the midi watchers/callbacks */
  usbMIDI.begin();
  usbMIDI.setHandleNoteOn(HandleNoteOn);
  usbMIDI.setHandleNoteOff(HandleNoteOff);

  MIDI.begin(MIDI_CHANNEL_OMNI);
  MIDI.setHandleNoteOn(HandleNoteOn);
  MIDI.setHandleNoteOff(HandleNoteOff);

  init_display();

  /* Welcome Message */
  update_top_display_line("Coup De Foudre");
  delay(1000);

  update_top_display_line(COILNAME);
  delay(1000);

  update_top_display_line(VERSION);
  delay(1000);
};


void init_mode(char* mode_name) {
  kill_all_notes();
  vfd.clear();
  vfd.setCursor(0, 0);
  vfd.print(mode_name);
  delay(100);
}

void act_on_estop() {
  if(digitalReadFast(estop_switch) == 0) {
    kill_all_notes();

    digitalWriteFast(channel_1_out, LOW);
    digitalWriteFast(channel_2_out, LOW);
    vfd.setCursor(0, 0);
    vfd.print("      ESTOP     ");
    // Require HW reboot to exit
    while(true) {};

  }
}

void midi_runloop() {
  read_controls();
  update_bottom_display_line();
  act_on_estop();
}

bool update_pulse_duty_cycle() {
  if( (old_pulse_period + 50) < pulse_period || 
      (old_pulse_period - 50) > pulse_period ) {
    old_pulse_period = pulse_period;
    return true;
  }
  return false;
}

void loop() {
  read_controls();

  // USB -> MIDI mode
  if (system_mode == 2) {
    init_mode("MIDI Mode (USB)");

    while (system_mode == 2) {
      usbMIDI.read();
      midi_runloop();
    };
  };

  // MIDI Input Mode
  if (system_mode == 1) {
    init_mode("MIDI Mode (PORT)");
    
    while (system_mode == 1) {
      MIDI.read();
      midi_runloop();
    };
  };
  
  // Pulse clock mode
  if (system_mode == 0) {
    init_mode("Fixed Freq.");
    
    while (system_mode == 0) {
      read_controls();
      update_bottom_display_line();      

      // NOTE (meawoppl) - The knob readout is noisy, so only ack large changes
      if(update_pulse_duty_cycle()) {
        timer_0.end();
        delay(10);
        timer_0.begin(pulse_static, pulse_period);
      };
    };
    
    timer_0.end();
  };

  if (system_mode == 4) {
    init_mode("Pink Noise");
    int delay_time;
    while (system_mode == 4) {
      read_controls();
      update_bottom_display_line();
      update_pulse_duty_cycle();

      digitalWriteFast(channel_1_out, HIGH);
      delay_safe_micros(interrupter_pulsewidth_setpoint);
      digitalWriteFast(channel_1_out, LOW);

      // Ensure that we ring-down for at least 2x that time
      // Linear approximation to Poisson arrival time
      delay_time = map(random_unit8(), 0, 256, interrupter_pulsewidth_setpoint * 2, pulse_period * 2);
      delayMicroseconds(delay_time);
    };
  };
};

void read_controls() {
  act_on_estop();

  // Read the potentiometers, and set the pulse width setpoint
  // This includes mapping the potentiometer values to the pulsewidth
  // as well as clamping the possible outputs
  int pulsewidth_raw = analogRead(pulsewidth_pot);
  interrupter_pulsewidth_setpoint = constrain(
    map(pulsewidth_raw, ANALOG_SCALE_MAX, ANALOG_SCALE_MIN, PULSEWIDTH_MAX, PULSEWIDTH_MIN),
      0, PULSEWIDTH_MAX);

  // Time between pulses in microseconds
  pulse_period = map(analogRead(duty_cycle_pot), 128, 0, 1000, 100000);

  // Read the input switches, and set the global system_mode var
  uint8_t midi_vs_pulse  = digitalReadFast(midi_mode_switch);
  uint8_t sub_setting = digitalReadFast(pulse_mode_switch);
  
  if ((midi_vs_pulse == 1) and (sub_setting == 0)) {
    // MIDI USB
    system_mode = 2;
  } 
  else if ((midi_vs_pulse == 1) and (sub_setting == 1))
  { 
    // MIDI Jack
    system_mode = 1;
  }
  else if ((midi_vs_pulse == 0) and (sub_setting == 0))
  {
    // Fixed Pulse mode
    system_mode = 0;
  }
  else if ((midi_vs_pulse == 0) and (sub_setting == 1)) 
  {
    // Static noise mode
    system_mode = 4;
  };
};

// Callback from MIDI.h
void HandleNoteOn(byte channel, byte pitch, byte velocity) {
  if ((channel != 1) && (channel != 2))
    return;

  #ifdef DEBUGMIDI
  debug_thing(channel);
  #endif

  if (velocity == 0) {
    stop_note(pitch, channel);
  } else {
    play_note(pitch, velocity, channel);
  };
};

// Callback from MIDI.h
void HandleNoteOff(byte channel, byte pitch, byte velocity) {
  if ((channel != 1) && (channel != 2))
    return;

  stop_note(pitch, channel);
};

void play_note(byte pitch, byte velocity, byte channel) {
  // if we find a timer free, start a note with pitch/velocity specified
  byte start = 0;
  byte end = 4;

  for(byte i = start; i < end; i++) {
    if(note_pitch[i] != 255)
      continue;

    note_pitch[i] = pitch;
    note_velocity[i] = velocity;
    note_channel[i] = channel;
    setup_note(pitch, i);
    break;
  };
};

void stop_note(byte pitch, byte channel) {
  // Stop _all notes with the specified pitch
  byte start = 0;
  byte end = 4;

  for(byte i = start; i < end; i++) {
    if(note_pitch[i] != pitch)
      continue;
    
    stop_timer(i);
    note_velocity[i] = 0;
    note_pitch[i] = 255;
  };
};


void setup_note(byte pitch, byte timer_number) {
  pitch = constrain(pitch, NOTE_MIN, NOTE_MAX);
  pulse_mod_array[timer_number] = mapf(pitch, NOTE_MIN, NOTE_MAX, MODIFIER_MAX, MODIFIER_MIN);
  start_timer(timer_number, midi_period_us[pitch]);
};

void start_timer(byte timer_number, uint32_t period_us) {
  // Start timer 'timer_number' to trigger every 'period_us' microseconds
  switch(timer_number) {
    case 0: timer_0.begin(pulse_0, period_us); break;
    case 1: timer_1.begin(pulse_1, period_us); break;
    case 2: timer_2.begin(pulse_2, period_us); break;
    case 3: timer_3.begin(pulse_3, period_us); break;
  };
}

void stop_timer(byte timer_number) {
  switch(timer_number) {
    case 0: timer_0.end(); break;
    case 1: timer_1.end(); break;
    case 2: timer_2.end(); break;
    case 3: timer_3.end(); break;
  };
};

void kill_all_notes() {
  for(byte i = 0; i < 4; i++) {
    note_pitch[i] = 255;
    note_velocity[i] = 0;
    stop_timer(i);
  };
};

inline uint32_t velocity_to_pulse_length(uint8_t velo) {
  // uint8_t random_component = random_unit8() / 128;
  return map(velo, 1, 127, PULSEWIDTH_MIN, interrupter_pulsewidth_setpoint);
}


void delay_safe_micros(uint32_t micros) {
  delayMicroseconds(clamp_pulse_width(micros));
}

// For the pulsed (tick) mode
void pulse_static() {
  digitalWriteFast(channel_1_out, HIGH);
  delay_safe_micros(interrupter_pulsewidth_setpoint);
  digitalWriteFast(channel_1_out, LOW);
}

// Note timer subroutine
void pulse_0() {
  digitalWriteFast(channel_1_out, HIGH);
  delay_safe_micros(velocity_to_pulse_length(note_velocity[0]));
  digitalWriteFast(channel_1_out, LOW);
};

void pulse_1() {
  digitalWriteFast(channel_1_out, HIGH);
  delay_safe_micros(velocity_to_pulse_length(note_velocity[1]));
  digitalWriteFast(channel_1_out, LOW);
};

void pulse_2() {
  digitalWriteFast(channel_1_out, HIGH);
  delay_safe_micros(velocity_to_pulse_length(note_velocity[2]));
  digitalWriteFast(channel_1_out, LOW);
};

void pulse_3() {
  digitalWriteFast(channel_1_out, HIGH);
  delay_safe_micros(velocity_to_pulse_length(note_velocity[3]));
  digitalWriteFast(channel_1_out, LOW);
};
