#include <MIDI.h>
#include <LiquidCrystal.h>

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
         | A1

Pulse Duty Cycle Pot
GND --/\/\/\/\/-- 3V3
         | A0


INPUTS ----------------------- ||
USB -> MicroUSB port
Optoisolated Midi RX -> D0 (See http://bit.ly/2a6BQgA )

*/

// Allocate all the teensy timers, put them in an array
IntervalTimer timer_0, timer_1, timer_2, timer_3;

#define midi_mode_switch 10
#define pulse_mode_switch 9
#define estop_switch 11

#define channel_1_out 22
#define channel_2_out 22

#define pulsewidth_pot A1
#define duty_cycle_pot A0

// 88 Key keyboard CONSTANTS ------------------
#define NOTE_MIN 21
#define NOTE_MAX 108

#define VERSION "2.1.0"

// Values for bipolar/theophany
// #define COILNAME "Cfg: Theophany"
// #define PULSEWIDTH_MIN 5
// #define PULSEWIDTH_MAX 100

// Values for Orage and 2014 coil
#define COILNAME "Cfg: Orage"
#define PULSEWIDTH_MIN 35
#define PULSEWIDTH_MAX 250

#include "util.h"
#include "midi_constants.h"

int clamp_pulse_width(float nominal_width)
{
  return (int)constrain(nominal_width, PULSEWIDTH_MIN, PULSEWIDTH_MAX);
}

volatile uint32_t pulse_period = 10000; // NOTE(meawoppl) - units of microseconds
volatile uint32_t old_pulse_period = 0;
volatile uint16_t interrupter_pulsewidth_setpoint = 100;
volatile SYS_MODE system_mode;

// In the below set of callbacks, we map the triple (c, p, v)
// to an individual timer keyed on p.  This way multiple notes
// with the same pitch won't take multiple timer slots
#define MY_CHANNEL 1
#define NOTE_ARRAY_SIZE 10

// Note scheduling array,
volatile MidiNote active_notes[NOTE_ARRAY_SIZE];

void set_note(int i, byte channel, byte pitch, byte velocity)
{
  active_notes[i].channel = channel;
  active_notes[i].pitch = pitch;
  active_notes[i].velocity = velocity;
  active_notes[i].period_us = midi_period_us[pitch];
  active_notes[i].phase_us = micros() % active_notes[i].period_us;
  active_notes[i].start_ms = millis();
}

void clear_note(int i)
{
  active_notes[i].velocity = 0;
  active_notes[i].pitch = 255;
  active_notes[i].channel = 0;
  active_notes[i].period_us = midi_period_us[0];
  active_notes[i].phase_us = 0;
  active_notes[i].start_ms = 0;
}

int find_first_empty_note_index()
{
  for (int i = 0; i < NOTE_ARRAY_SIZE
; i++)
  {
    if (active_notes[i].pitch == 255)
    {
      return i;
    }
  }
  return -1;
}

float bent_value_cents = 0;

#include "display.h"
#include "entropy.h"

const uint8_t ADC_RESOLUTION = 7;
const uint16_t ANALOG_SCALE_MAX = pow(2, ADC_RESOLUTION) - 2;
const uint8_t ANALOG_SCALE_MIN = 0;

const float MODIFIER_MAX = 1;
const float MODIFIER_MIN = midi_freq[NOTE_MIN] / midi_freq[NOTE_MAX];

float mapf(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// Enable the below to get a jankey vis of what midi notes
// are coming into the system
void debug_thing(int thing)
{
  vfd.setCursor(0, 0);
  vfd.print("        ");
  vfd.setCursor(0, 0);
  vfd.print("V: ");
  vfd.print(thing);
}

void setup()
{
  /* Pullups, impedances etc */
  pinMode(channel_1_out, OUTPUT);
  pinMode(channel_2_out, OUTPUT);

  pinMode(midi_mode_switch, INPUT);
  pinMode(pulse_mode_switch, INPUT);
  pinMode(estop_switch, INPUT_PULLUP);

  pinMode(pulsewidth_pot, INPUT);
  pinMode(duty_cycle_pot, INPUT);

  analogReadResolution(ADC_RESOLUTION);
  analogReadAveraging(128);

  /* Init  the midi watchers/callbacks */
  usbMIDI.begin();
  usbMIDI.setHandleNoteOn(HandleNoteOn);
  usbMIDI.setHandleNoteOff(HandleNoteOff);
  // usbMIDI.setHandleError(HandleError);
  // usbMIDI.setHandlePitchBend(HandlePitchBend);

  MIDI.begin(MIDI_CHANNEL_OMNI);
  MIDI.setHandleNoteOn(HandleNoteOn);
  MIDI.setHandleNoteOff(HandleNoteOff);
  MIDI.setHandlePitchBend(HandlePitchBend);
  MIDI.setHandleError(HandleError);

  init_display();

  /* Welcome Message */
  update_top_display_line("Coup De Foudre");
  delay(1000);

  update_top_display_line(COILNAME);
  delay(1000);

  update_top_display_line(VERSION);
  delay(1000);
};

void HandleError(int8_t err){
  clear_notes();
}

void init_mode()
{
  clear_notes_and_timers();
  vfd.clear();
  vfd.setCursor(0, 0);
  switch (system_mode)
  {
  case SM_MIDI_USB:
    vfd.print("MIDI Mode (USB)");
    break;
  case SM_MIDI_JACK:
    vfd.print("MIDI Mode (PORT)");
    break;
  case SM_FREQ_FIXED:
    vfd.print("Fixed Freq.");
    break;
  case SM_FREQ_PINK:
    vfd.print("Pink Noise");
    break;
  }

  delay(100);
}

void act_on_estop()
{
  if (digitalReadFast(estop_switch) == 0)
  {
    // Clear all notes and pending timers
    clear_notes_and_timers();

    // Force the outputs to low
    digitalWriteFast(channel_1_out, LOW);
    digitalWriteFast(channel_2_out, LOW);

    // Display what has happened
    vfd.setCursor(0, 0);
    vfd.print("      ESTOP     ");

    // Require HW reboot to exit
    // This HCF loop also ensures that no new midi events are processed
    while (true)
    {
    };
  }
}

bool update_pulse_duty_cycle()
{
  if ((old_pulse_period + 50) < pulse_period ||
      (old_pulse_period - 50) > pulse_period)
  {
    old_pulse_period = pulse_period;
    return true;
  }
  return false;
}

void preloop()
{
  act_on_estop();
  read_controls();
  update_bottom_display_line();
}

void midi_usb_loop()
{
  start_music_sm();
  while (system_mode == SM_MIDI_USB)
  {
    preloop();
    usbMIDI.read();
  };
}

void midi_jack_loop()
{
  start_music_sm();
  while (system_mode == SM_MIDI_JACK)
  {
    preloop();
    MIDI.read();
  };
}

void fixed_freq_loop()
{
  while (system_mode == SM_FREQ_FIXED)
  {
    preloop();

    // NOTE (meawoppl) - The knob readout is noisy, so only ack large changes
    if (update_pulse_duty_cycle())
    {
      timer_0.end();
      delay(10);
      timer_0.begin(pulse_static, pulse_period);
    };
  };
}

void pink_noise_loop()
{
  int delay_time;
  while (system_mode == SM_FREQ_PINK)
  {
    preloop();

    update_pulse_duty_cycle();

    digitalWriteFast(channel_1_out, HIGH);
    delay_safe_micros(interrupter_pulsewidth_setpoint);
    digitalWriteFast(channel_1_out, LOW);

    // Ensure that we ring-down for at least 2x that time
    // Linear approximation to Poisson arrival time
    delay_time = map(random_unit8(), 0, 256, interrupter_pulsewidth_setpoint * 2, pulse_period * 2);
    delayMicroseconds(delay_time);
  };
}

void loop()
{
  read_controls();
  init_mode();

  switch (system_mode)
  {
  case SM_MIDI_USB:
    midi_usb_loop();
    break;
  case SM_MIDI_JACK:
    midi_jack_loop();
    break;
  case SM_FREQ_FIXED:
    fixed_freq_loop();
    break;
  case SM_FREQ_PINK:
    pink_noise_loop();
    break;
  };

  clear_notes_and_timers();
};

// Midi.h outputs values between -8192, 8192 which we map here to
// to cents: https://en.wikipedia.org/wiki/Cent_(music)
// Assumption is that input range is +- 2 semitones
void HandlePitchBend(byte channel, int bend)
{
  float bent_value_cents = mapf(bend, -8192, 8192, -200, 200);
  bend_all_notes(bent_value_cents);
}

void read_controls()
{
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
  uint8_t midi_vs_pulse = digitalReadFast(midi_mode_switch);
  uint8_t sub_setting = digitalReadFast(pulse_mode_switch);

  if ((midi_vs_pulse == 1))
  {
    if (sub_setting == 0)
    {
      system_mode = SM_MIDI_USB;
    }
    else
    {
      system_mode = SM_MIDI_JACK;
    }
  }
  else
  {
    if (sub_setting == 0)
    {
      system_mode = SM_FREQ_FIXED;
    }
    else
    {
      system_mode = SM_FREQ_PINK;
    }
  }
};

byte clamp_pitch(byte pitch)
{
  return constrain(pitch, NOTE_MIN, NOTE_MAX);
}

// Callback from MIDI.h
void HandleNoteOn(byte channel, byte pitch, byte velocity)
{
  if ((channel != 1) && (channel != 2))
    return;

  pitch = clamp_pitch(pitch);

  if (velocity == 0)
  {
    stop_note(pitch, channel);
  }
  else
  {
    start_note(pitch, velocity, channel);
  };
};

// Callback from MIDI.h
void HandleNoteOff(byte channel, byte pitch, byte velocity)
{
  if ((channel != 1) && (channel != 2))
    return;
  pitch = clamp_pitch(pitch);

  stop_note(pitch, channel);
};

void start_note(byte pitch, byte velocity, byte channel)
{
  // if we find a timer free, start a note with pitch/velocity specified
  int idx = find_first_empty_note_index();
  if (idx == -1)
  {
    return;
  }

  set_note(idx, channel, pitch, velocity);
};

void stop_note(byte pitch, byte channel)
{
  // Stop _all notes with the specified pitch
  for (byte i = 0; i < NOTE_ARRAY_SIZE
; i++)
  {
    if (active_notes[i].pitch != pitch)
      continue;

    active_notes[i].pitch = 255;
    active_notes[i].velocity = 0;
    active_notes[i].channel = 0;
    active_notes[i].phase_us = 0;
  };
};

void bend_all_notes(float cents)
{
  for (int i = 0; i < NOTE_ARRAY_SIZE
; i++)
  {
    byte pitch = active_notes[i].pitch;
    float f = midi_freq[pitch] * pow(2, cents / 1200.0);
    float p = 1.0 / f;
    active_notes[i].period_us = round(p * 1000000);
  }
};

void clear_notes()
{
  for (byte i = 0; i < NOTE_ARRAY_SIZE; i++)
  {
    clear_note(i);
  };
}

void clear_notes_and_timers()
{
  clear_notes();
  timer_0.end();
  timer_1.end();
  timer_2.end();
  timer_3.end();
};

inline uint32_t velocity_to_pulse_length(uint8_t velocity, uint32_t start_ms)
{
  velocity += random_unit8() / 128;

  uint32_t millis_since_start = millis() - start_ms;

  // if(millis_since_start > 500){
  //   velocity /= 2;
  // }
  
  return map(velocity, 1, 127, PULSEWIDTH_MIN, interrupter_pulsewidth_setpoint);
}

void delay_safe_micros(uint32_t micros)
{
  delayMicroseconds(clamp_pulse_width(micros));
}

// For the pulsed (tick) mode
const int32_t MAX_WAIT_US = 1000;
const float DUTY_CYCLE = 0.25;
const float PULSE_MULT = (1.0 / DUTY_CYCLE) - 1.0;

volatile MUSIC_STATE music_state = SM_NEXT;
volatile int pulse_length_us = 100;
volatile unsigned long next_pulse_start_micros = 0;

void start_music_sm()
{
  music_state = SM_NEXT;
  timer_0.begin(music_loop, 10);
  // timer_0.priority(0);
}

void stop_music_sm()
{
  music_state = SM_NEXT;
  timer_0.end();
}

// NOTE(meawoppl)
// This is a tiny state machine. It has only two states:
// SM_WAIT: Wait for a note to be less than MAX_WAIT_US
// SM_PULSE: Play a pulse previously scheduled.
// This uses two state variables:
//  - music_state - The branch of the state machien
//  - pulse_length_us - the length of the pulse to be played.
void music_loop()
{
  unsigned long start_micros = micros();

  switch (music_state)
  {
  case SM_NEXT:
  {
    uint32_t wait_us = 10000;
    for (int i = 0; i < NOTE_ARRAY_SIZE
  ; i++)
    {
      if (active_notes[i].pitch == 255)
      {
        continue;
      }
      unsigned long phase_us = active_notes[i].phase_us;
      unsigned long period_us = active_notes[i].period_us;

      unsigned long this_wait_us = period_us - ((start_micros + phase_us) % period_us);

      if (this_wait_us < wait_us)
      {
        wait_us = this_wait_us;
        next_pulse_start_micros = start_micros + this_wait_us;
        pulse_length_us = velocity_to_pulse_length(active_notes[i].velocity, active_notes[i].start_ms);
        music_state = SM_WAIT;
      }
    }
  }
  break;
  case SM_WAIT:
    music_state = micros() > next_pulse_start_micros ? SM_PULSE : SM_WAIT;
    break;
  case SM_PULSE:
    // Actual pulse is below
    digitalWriteFast(channel_1_out, HIGH);
    delay_safe_micros(pulse_length_us);
    digitalWriteFast(channel_1_out, LOW);
    // Compute the time needed to keep duty cycle below:
    delayMicroseconds(PULSE_MULT * pulse_length_us);
    music_state = SM_NEXT;
    break;
  }
}

// For fixed freq osc
void pulse_static()
{
  digitalWriteFast(channel_1_out, HIGH);
  delay_safe_micros(interrupter_pulsewidth_setpoint);
  digitalWriteFast(channel_1_out, LOW);
};

// Note timer subroutine
void pulse_0()
{
  digitalWriteFast(channel_1_out, HIGH);
  delay_safe_micros(velocity_to_pulse_length(active_notes[0].velocity, active_notes[0].start_ms));
  digitalWriteFast(channel_1_out, LOW);
};

void pulse_1()
{
  digitalWriteFast(channel_1_out, HIGH);
  delay_safe_micros(velocity_to_pulse_length(active_notes[1].velocity, active_notes[1].start_ms));
  digitalWriteFast(channel_1_out, LOW);
};

void pulse_2()
{
  digitalWriteFast(channel_1_out, HIGH);
  delay_safe_micros(velocity_to_pulse_length(active_notes[2].velocity, active_notes[2].start_ms));
  digitalWriteFast(channel_1_out, LOW);
};

void pulse_3()
{
  digitalWriteFast(channel_1_out, HIGH);
  delay_safe_micros(velocity_to_pulse_length(active_notes[3].velocity, active_notes[3].start_ms));
  digitalWriteFast(channel_1_out, LOW);
};
