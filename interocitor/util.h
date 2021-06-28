enum SYS_MODE
{
    SM_MIDI_USB,
    SM_MIDI_JACK,
    SM_FREQ_FIXED,
    SM_FREQ_PINK
};

struct MidiNote
{
    uint8_t channel;
    uint8_t pitch;
    uint8_t velocity;
    uint32_t phase_us;
    uint32_t period_us;
    uint32_t start_ms;
};

enum MUSIC_STATE {
    SM_NEXT,
    SM_WAIT,
    SM_PULSE,
};