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
