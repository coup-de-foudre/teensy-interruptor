#include <SerLCD.h>

// 1 will have pulse readout in ms, 0 is hz
#define READOUT_MS 0
#define DEBUG_BEND false

SerLCD lcd; // Initialize the library with default I2C address 0x72

boolean _lcd_init = false;
void init_serlcd(){
  if(_lcd_init){
    return;
  }
  
  Serial1.setTX(1);
  Serial1.begin(9600);
  lcd.begin(Serial1); //Set up the LCD for Serial communication at 9600bps
  lcd.disableSystemMessages();

  lcd.setBacklight(255, 255, 255); //Set backlight to bright white
  lcd.setContrast(2); //Set contrast. Lower to 0 for higher contrast.

  lcd.clear();
  delay(10);

  _lcd_init = true;
}


void update_top_display_line(const char *contents)
{
  lcd.setCursor(0, 0);
  lcd.print("                "); // Blank the line
  delay(10);
  lcd.setCursor(0, 0);
  lcd.print(contents);
}

unsigned long last_display_millis = 0;

void update_bottom_display_line()
{
  
  // Debounce the call rate of this function to avoid flicker
  if ((millis() - last_display_millis) < 250)
    return;

  lcd.setCursor(0, 1);
  delay(50);

  lcd.print("W<     ");
  lcd.setCursor(2, 1);
  lcd.print(interrupter_pulsewidth_setpoint);
  lcd.write(0xE4); // <- mu
  lcd.print("s ");


  if ((system_mode == SM_FREQ_FIXED) or (system_mode == SM_FREQ_PINK))
  {
    lcd.setCursor(7, 1);
    lcd.print("          ");
    lcd.setCursor(8, 1);

    // NOTE (meawoppl) - This changes the readout between ms and Hz
    if (READOUT_MS)
    {
      lcd.print(pulse_period / 1000);
      lcd.print("ms  ");
    }
    else
    {
      lcd.print(((float)1 / ((float)(pulse_period / (float)1000000))));
      lcd.print("Hz   ");
    }
  }

  if ((system_mode == SM_MIDI_USB) or (system_mode == SM_MIDI_JACK))
  {
    lcd.setCursor(8, 1);
    for (byte i = 0; i < 4; i++)
    {
      if (active_notes[i].velocity == 0)
      {
        lcd.print("  ");
      }
      else
      {
        lcd.print(note_name[active_notes[i].pitch % 12]);
      }
    };
  }

  last_display_millis = millis();
}
