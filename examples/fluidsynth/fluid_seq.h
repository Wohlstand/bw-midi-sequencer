#include <stdint.h>
#include <stddef.h>

#include <fluidsynth.h>

// Rename the definition of the MIDI sequencer to avoid ABI collision while using multiple similar modules
#define BW_MidiSequencer FluidMidiSequencer
class BW_MidiSequencer;
typedef BW_MidiSequencer MidiSequencer;
typedef struct BW_MidiRtInterface BW_MidiRtInterface;

class FluidMidiSeq
{
    fluid_settings_t *settings;
    fluid_synth_t    *synth;
    int               soundfont;
    const char *soundFontPatch;

    uint32_t m_rate;

    MidiSequencer *m_sequencer;
    BW_MidiRtInterface *m_sequencerInterface;
    void initSequencerInterface();
    void initFluid();
    void closeFluid();

public:
    explicit FluidMidiSeq(const char *bankPath, uint32_t rate = 44100);
    ~FluidMidiSeq();

    fluid_synth_t *getSynth();

    void setLoop(bool enable);

    bool Open(char *filename);

    bool Load(const void *buf, size_t size);

    double Tick(double s, double granularity);

    int playBuffer(uint8_t *stream, int len);    
};
