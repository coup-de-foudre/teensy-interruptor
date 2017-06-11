#include <LiquidCrystal.h>

LiquidCrystal vfd(3, 4, 5, 6, 7, 2);                // (RS, Enable, D4, D5, D6, D7)

void init_display(){
  vfd.begin(2, 20);
  // Give time for LCD to init
  delay(10); 
  vfd.clear();
}


void update_top_display_line(char * contents) {
  vfd.setCursor(0,0);
  vfd.print(contents);
}


unsigned long last_display_millis = 0;

void update_bottom_display_line() {
  // Debounce the call rate of this function to avoid flicker
  if ((millis() - last_display_millis) < 250)
    return;

  vfd.setCursor(0, 1);
  vfd.print("W:      ");
  vfd.setCursor(2, 1);
  vfd.print(interrupter_pulsewidth_setpoint);
  vfd.write(0xE4);
  vfd.print("s  ");

  if (system_mode == 0) {
      vfd.setCursor(8, 1);
      vfd.print("T:      ");
      vfd.setCursor(10, 1);

      // NOTE (meawoppl) - This changes the readout between ms and Hz
      if (READOUT_MS) {
        vfd.print(pulse_duty_cycle_setpoint / 1000);
        vfd.print("ms  ");
      } else {
        vfd.print(( (float) 1 / ((float)(pulse_duty_cycle_setpoint / (float) 1000000))  ));
        vfd.print("Hz   ");
      }
  }

  last_display_millis = millis();
}
