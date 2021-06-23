enum SYS_MODE{SM_MIDI_USB, SM_MIDI_JACK, SM_FREQ_FIXED, SM_FREQ_PINK};

struct MidiNote {
    uint8_t channel;
    uint8_t pitch;
    uint8_t velocity;
};