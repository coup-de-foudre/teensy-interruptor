#include <MIDI.h>

#include <LiquidCrystal.h>

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


IntervalTimer timer_0, timer_1, timer_2, timer_3;
LiquidCrystal vfd(3, 4, 5, 6, 7, 8);                // (RS, Enable, D4, D5, D6, D7)

#define midi_mode_switch    10
#define pulse_mode_switch   9
#define estop_switch        11
#define channel_1_out       21
#define pulsewidth_pot A0
#define duty_cycle_pot A1

// 88 Key keyboard CONSTANTS ------------------
#define NOTE_MIN 21
#define NOTE_MAX 108
#define PULSEWIDTH_MIN 2
#define PULSEWIDTH_MAX 300

#include "midi_constants.h"

volatile uint32_t pulse_duty_cycle_setpoint          = 10000;
volatile uint32_t old_pulse_duty_cycle_setpoint      = 0;
volatile uint16_t interrupter_pulsewidth_setpoint    = 100;
volatile uint8_t  system_mode                        = 0;     // System Mode, 2 = USB, 1 = MIDI-RX, 0 = Clock

const uint8_t  ADC_RESOLUTION = 7;
const uint16_t ANALOG_SCALE_MAX = pow(2, ADC_RESOLUTION) - 2;
const uint8_t  ANALOG_SCALE_MIN = 0;

const float  MODIFIER_MAX = 1;
const float  MODIFIER_MIN = midi_freq[NOTE_MIN] / midi_freq[NOTE_MAX];

float pulse_0_modifier = 1;
float pulse_1_modifier = 1;
float pulse_2_modifier = 1;
float pulse_3_modifier = 1;

float mapf(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
  }

uint8_t note_scheduler[4] = {0, 0, 0, 0};   // Note scheduling array

/*
    Setup / Init. This runs only once
*/

void setup() {
  /* Pullups, impedances etc */

  pinMode(channel_1_out,         OUTPUT);
  pinMode(midi_mode_switch,      INPUT);
  pinMode(pulse_mode_switch,     INPUT);
  pinMode(estop_switch,          INPUT_PULLUP);

  pinMode(pulsewidth_pot, INPUT);
  pinMode(duty_cycle_pot, INPUT);

  analogReadResolution(ADC_RESOLUTION);
  analogReadAveraging(128);


  /* Init Classes */

  usbMIDI.begin();
  usbMIDI.setHandleNoteOn(HandleNoteOn);
  usbMIDI.setHandleNoteOff(HandleNoteOff);

  MIDI.begin(MIDI_CHANNEL_OMNI);
  MIDI.setHandleNoteOn(HandleNoteOn);
  MIDI.setHandleNoteOff(HandleNoteOff);

  vfd.begin(2,20);
  delay(2); // Give time for LCD to init

  /* Welcome Message */

  vfd.clear();
  vfd.setCursor(0,0);
  vfd.print("Turboencabulator");

  delay(1500); // Do nothing

  vfd.clear();
};

/*

  Main loop. Loopity loopity loop

*/


void loop() {
  common();


  /*
      == USB Midi Mode ==
    This mode will make the system behave as a USB midi device
  */

  if(system_mode == 2){
      killAllNotes();

      vfd.clear();
      vfd.setCursor(0,0);
      vfd.print("USB-MIDI");
      
      /* end setup */
      
      while(system_mode == 2){
	usbMIDI.read();
	common();
	
	vfd.setCursor(0,1);
	vfd.print("W:      ");
	vfd.setCursor(2,1);
	vfd.print(interrupter_pulsewidth_setpoint);
	vfd.write(0xE4);
	vfd.print("s  ");
	
	if(digitalReadFast(estop_switch) == 0) killAllNotes();
      };
  };
  

  /*
    == MIDI Input Mode ==

  This mode will make the system behave as an actual MIDI reciever (for guitars and shit)
  */

  if(system_mode == 1){     // MIDI-RX mode
    killAllNotes();

    vfd.clear();
    vfd.setCursor(0,0);
    vfd.print("MIDI-RX");
    
    /* end setup */

    while(system_mode == 1){
      MIDI.read();
      common();
      
      vfd.setCursor(0,1);
      vfd.print("W:      ");
      vfd.setCursor(2,1);
      vfd.print(interrupter_pulsewidth_setpoint);
      vfd.write(0xE4);
      vfd.print("s  ");
      
      if(digitalReadFast(estop_switch) == 0) killAllNotes();
    };
  };
  

  if(system_mode == 0){     // Pulse clock mode
    killAllNotes();
    
    vfd.clear();
    vfd.setCursor(0,0);
    vfd.print("PULSE");
    delay(100);

    /* end setup */
    
    while(system_mode == 0){
      common();
      
      vfd.setCursor(0,1);
      vfd.print("W:      ");
      vfd.setCursor(2,1);
      vfd.print(interrupter_pulsewidth_setpoint);
      vfd.write(0xE4);
      vfd.print("s  ");
      
      vfd.setCursor(8,1);
      vfd.print("T:      ");
      vfd.setCursor(10,1);
      
      vfd.print(pulse_duty_cycle_setpoint/1000);
      vfd.print("ms  ");
      
      
      //vfd.print(( (float) 1 / ((float)(pulse_duty_cycle_setpoint / (float)1000000))  ));
      //vfd.print("Hz   ");
      
      pulse_duty_cycle_setpoint = map(analogRead(duty_cycle_pot), 128, 0, 10000, 100000);
      
      if((old_pulse_duty_cycle_setpoint + 2000) < pulse_duty_cycle_setpoint || (old_pulse_duty_cycle_setpoint - 2000) > pulse_duty_cycle_setpoint ){
	old_pulse_duty_cycle_setpoint = pulse_duty_cycle_setpoint;
	timer_0.end();
	delay(10);
	timer_0.begin(pulse_0, pulse_duty_cycle_setpoint);
      };
    };
    
    timer_0.end();
      };
};



void common(){
  interrupter_pulsewidth_setpoint = map(analogRead(pulsewidth_pot), ANALOG_SCALE_MAX, ANALOG_SCALE_MIN, PULSEWIDTH_MAX, PULSEWIDTH_MIN);
  interrupter_pulsewidth_setpoint = constrain(interrupter_pulsewidth_setpoint, 0, PULSEWIDTH_MAX);

  uint8_t midi_mode_state  = digitalReadFast(midi_mode_switch);
  uint8_t pulse_mode_state = digitalReadFast(pulse_mode_switch);
  
  if(midi_mode_state == 1){
      system_mode = 2;
  }
  
  else if(pulse_mode_state == 1){
      system_mode = 0;
  }
  
  else{  // Otherwise... MIDI-RX mode
    system_mode = 1;
  };
};


void highGUI(){
  int current_gui_channel = 0;  // Tesla coil channel
  int current_gui_mode = 0;  // GUI mode;
  
  vfd.setCursor(10, 0);
  vfd.print("Coil ");
  vfd.print(current_gui_channel);
}



/*

  MIDI Functions for handling such bullcrap

*/

void HandleNoteOn(byte channel, byte pitch, byte velocity) {   // Callback
  if (velocity == 0) {
    ceaseNote(channel);
  } else {
    playNote(pitch, channel);
  };
};


void HandleNoteOff(byte channel, byte pitch, byte velocity) {  // Callback
  ceaseNote(channel);
};


void playNote(byte pitch, byte note_channel){

  pitch = constrain(pitch, NOTE_MIN, NOTE_MAX);
  
  for(byte i = 0; i < 4; i++){
    if(note_scheduler[i] != 0) continue;
          //otherwise...
    
    note_scheduler[i] = note_channel;
    setTimer(pitch, i);
    i = 100; // mission accomplished, now gtfo
  };
};


void ceaseNote(byte note_channel){
  for(byte i = 0; i < 4; i++){
    if(note_scheduler[i] != note_channel) continue;
    //otherwise...
    
    note_scheduler[i] = 0;
    killTimer(i);
    i = 100; // mission accomplished, now gtfo
  };
};



/*

  Timer Functions

*/

void setTimer(byte pitch, byte timer){
  int pp;

  switch(timer){
    case 0:
      pulse_0_modifier = mapf(pitch, NOTE_MIN, NOTE_MAX, MODIFIER_MAX, MODIFIER_MIN);
      timer_0.begin(pulse_0, midi_period_us[pitch]);
      break;
    case 1:
      pulse_1_modifier = mapf(pitch, NOTE_MIN, NOTE_MAX, MODIFIER_MAX, MODIFIER_MIN);
      timer_1.begin(pulse_1, midi_period_us[pitch]);
      break;
    case 2:
      pulse_2_modifier = mapf(pitch, NOTE_MIN, NOTE_MAX, MODIFIER_MAX, MODIFIER_MIN);
      timer_2.begin(pulse_2, midi_period_us[pitch]);
      break;
    case 3:
      pulse_3_modifier = mapf(pitch, NOTE_MIN, NOTE_MAX, MODIFIER_MAX, MODIFIER_MIN);
      timer_3.begin(pulse_3, midi_period_us[pitch]);
      break;
  };
};


void killTimer(byte timer){
  switch(timer){
  case 0: timer_0.end(); break;
  case 1: timer_1.end(); break;
  case 2: timer_2.end(); break;
  case 3: timer_3.end(); break;
  };
};


void killAllNotes(){
  for(byte i = 0; i < 4; i++){
    note_scheduler[i] = 0;
    killTimer(i);
  };
};

void pulse_0(){
  digitalWriteFast(channel_1_out, HIGH);
  int delay_us = (int) ((float)interrupter_pulsewidth_setpoint * pulse_0_modifier);
  delay_us     = constrain(delay_us, PULSEWIDTH_MIN, PULSEWIDTH_MAX);
  delayMicroseconds( delay_us );
  digitalWriteFast(channel_1_out, LOW);
};

void pulse_1(){
  digitalWriteFast(channel_1_out, HIGH);
  int delay_us = (int) ((float)interrupter_pulsewidth_setpoint * pulse_1_modifier);
  delay_us     = constrain(delay_us, PULSEWIDTH_MIN, PULSEWIDTH_MAX);
  delayMicroseconds( delay_us );
  digitalWriteFast(channel_1_out, LOW);
};

void pulse_2(){
  digitalWriteFast(channel_1_out, HIGH);
  int delay_us = (int) ((float)interrupter_pulsewidth_setpoint * pulse_2_modifier);
  delay_us     = constrain(delay_us, PULSEWIDTH_MIN, PULSEWIDTH_MAX);
  delayMicroseconds( delay_us );
  digitalWriteFast(channel_1_out, LOW);
};

void pulse_3(){
  digitalWriteFast(channel_1_out, HIGH);
  int delay_us = (int) ((float)interrupter_pulsewidth_setpoint * pulse_3_modifier);
  delay_us     = constrain(delay_us, PULSEWIDTH_MIN, PULSEWIDTH_MAX);
  delayMicroseconds( delay_us );
  digitalWriteFast(channel_1_out, LOW);
};

