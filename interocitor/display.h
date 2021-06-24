#include <LiquidCrystal.h>

// 1 will have pulse readout in ms, 0 is hz
#define READOUT_MS 0
#define DEBUG_BEND false

LiquidCrystal vfd(3, 4, 5, 6, 7, 2); // (RS, Enable, D4, D5, D6, D7)

void init_display()
{
  vfd.begin(2, 20);
  // Give time for LCD to init
  delay(10);
  vfd.clear();
}

void update_top_display_line(const char *contents)
{
  vfd.setCursor(0, 0);
  vfd.print("                "); // Blank the line

  vfd.setCursor(0, 0);
  vfd.print(contents);
}

unsigned long last_display_millis = 0;

void update_bottom_display_line()
{
  // Debounce the call rate of this function to avoid flicker
  if ((millis() - last_display_millis) < 250)
    return;

  vfd.setCursor(0, 1);

  vfd.print("W<     ");
  vfd.setCursor(2, 1);
  vfd.print(interrupter_pulsewidth_setpoint);
  vfd.write(0xE4); // <- mu
  vfd.print("s ");

  if ((system_mode == 0) or (system_mode == 4))
  {
    vfd.setCursor(7, 1);
    vfd.print("          ");
    vfd.setCursor(8, 1);

    // NOTE (meawoppl) - This changes the readout between ms and Hz
    if (READOUT_MS)
    {
      vfd.print(pulse_period / 1000);
      vfd.print("ms  ");
    }
    else
    {
      vfd.print(((float)1 / ((float)(pulse_period / (float)1000000))));
      vfd.print("Hz   ");
    }
  }

  if ((system_mode == SM_MIDI_USB) or (system_mode == SM_MIDI_JACK))
  {
    vfd.setCursor(8, 1);
    for (byte i = 0; i < TIMER_COUNT; i++)
    {
      if (active_notes[i].velocity == 0)
      {
        vfd.print("  ");
      }
      else
      {
        vfd.print(note_name[active_notes[i].pitch % 12]);
      }
    };
  }

  last_display_millis = millis();
}
