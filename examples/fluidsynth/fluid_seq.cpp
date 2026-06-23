// Rename the definition of the MIDI sequencer to avoid ABI collision while using multiple similar modules
#define BW_MidiSequencer FluidMidiSequencer
// MIDI sequnecer implementation
#include "midi_sequencer_impl.hpp"

#include "fluid_seq.h"

/***************************************************************************
 *  Callbacks to wrap FluidSynth's real-time API to the BW MIDI Sequencer  *
 ***************************************************************************/

FluidMidiSeq &getModule(void *userdata)
{
    FluidMidiSeq *c = reinterpret_cast<FluidMidiSeq *>(userdata);
    return *c;
}

static void rtNoteOn(void *userdata, uint8_t channel, uint8_t note, uint8_t velocity)
{
    FluidMidiSeq &m = getModule(userdata);
    fluid_synth_noteon(m.getSynth(), channel, note, velocity);
}

static void rtNoteOff(void *userdata, uint8_t channel, uint8_t note)
{
    FluidMidiSeq &m = getModule(userdata);
    fluid_synth_noteoff(m.getSynth(), channel, note);
}

static void rtNoteAfterTouch(void *userdata, uint8_t channel, uint8_t note, uint8_t atVal)
{
#if FLUIDSYNTH_VERSION_MAJOR >= 2
    FluidMidiSeq &m = getModule(userdata);
    fluid_synth_key_pressure(m.getSynth(), channel, note, atVal);
#else
    (void)userdata; (void)channel; (void)note; (void)atVal;
#endif
}

static void rtChannelAfterTouch(void *userdata, uint8_t channel, uint8_t atVal)
{
    FluidMidiSeq &m = getModule(userdata);
    fluid_synth_channel_pressure(m.getSynth(), channel, atVal);
}

static void rtControllerChange(void *userdata, uint8_t channel, uint8_t type, uint8_t value)
{
    FluidMidiSeq &m = getModule(userdata);
    fluid_synth_cc(m.getSynth(), channel, type, value);
}

static void rtPatchChange(void *userdata, uint8_t channel, uint8_t patch)
{
    FluidMidiSeq &m = getModule(userdata);
    fluid_synth_program_change(m.getSynth(), channel, patch);
}

static void rtPitchBend(void *userdata, uint8_t channel, uint8_t msb, uint8_t lsb)
{
    FluidMidiSeq &m = getModule(userdata);
    fluid_synth_pitch_bend(m.getSynth(), channel, (msb << 7) | lsb);
}

static void rtSysEx(void *userdata, const uint8_t *msg, size_t size)
{
    FluidMidiSeq &m = getModule(userdata);
    fluid_synth_sysex(m.getSynth(), reinterpret_cast<const char*>(msg), static_cast<int>(size), nullptr, nullptr, nullptr, 0);
}


/* NonStandard calls */
static void rtDeviceSwitch(void *userdata, size_t track, const char *data, size_t length)
{
    (void)userdata; (void)track; (void)data; (void)length;
    // FluidSynth doesn't implements this
}

static size_t rtCurrentDevice(void *userdata, size_t track)
{
    (void)userdata; (void)track;
    // FluidSynth doesn't implements this
    return 0;
}
/* NonStandard calls End */

static void playSynth(void *userdata, uint8_t *stream, size_t length)
{
    FluidMidiSeq &m = getModule(userdata);
    fluid_synth_write_s16(m.getSynth(), static_cast<int>(length / 4),
                          stream, 0, 2,
                          stream + 2, 0, 2);
}


void FluidMidiSeq::initSequencerInterface()
{
    m_sequencer = new MidiSequencer;
    BW_MidiRtInterface *seq = new BW_MidiRtInterface;
    if(m_sequencerInterface)
        delete m_sequencerInterface;
    m_sequencerInterface = seq;
    std::memset(seq, 0, sizeof(BW_MidiRtInterface));

    /* Real-time MIDI API */
    seq->rtUserData = this;
    seq->rt_noteOn  = rtNoteOn;
    seq->rt_noteOff = rtNoteOff;
    seq->rt_noteAfterTouch = rtNoteAfterTouch;
    seq->rt_channelAfterTouch = rtChannelAfterTouch;
    seq->rt_controllerChange = rtControllerChange;
    seq->rt_patchChange = rtPatchChange;
    seq->rt_pitchBend = rtPitchBend;
    seq->rt_systemExclusive = rtSysEx;

    seq->onPcmRender = playSynth;
    seq->onPcmRender_userData = this;

    seq->pcmSampleRate = m_rate;
    seq->pcmFrameSize = 2 /*channels*/ * 2 /*size of one sample*/;

    /* Non-standard features */
    seq->rt_deviceSwitch = rtDeviceSwitch;
    seq->rt_currentDevice = rtCurrentDevice;

    m_sequencer->setInterface(seq);
    // While playing HMI or EMIDI files, set the device filter
    m_sequencer->setDeviceMask(BW_MidiSequencer::Device_GeneralMidi|BW_MidiSequencer::Device_SoundMasterII|BW_MidiSequencer::Device_GravisUltrasound);
}

void FluidMidiSeq::initFluid()
{
    closeFluid();
    settings = new_fluid_settings();
    synth = new_fluid_synth(settings);
    soundfont = fluid_synth_sfload(synth, soundFontPatch, 1);
}

void FluidMidiSeq::closeFluid()
{
    if(settings && synth)
    {
        delete_fluid_synth(synth);
        delete_fluid_settings(settings);
        settings = nullptr;
        synth = nullptr;
        soundfont = -1;
    }
}

FluidMidiSeq::FluidMidiSeq(const char *bankPath, uint32_t rate)
{
    soundFontPatch = bankPath ? bankPath : "/usr/share/sounds/sf2/FluidR3_GM.sf2";
    settings = nullptr;
    synth = nullptr;
    soundfont = -1;

    m_sequencer = nullptr;
    m_sequencerInterface = nullptr;
    m_rate = rate;

    initFluid();
    initSequencerInterface();
}

FluidMidiSeq::~FluidMidiSeq()
{
    if(m_sequencer)
        delete m_sequencer;
    if(m_sequencerInterface)
        delete m_sequencerInterface;
    closeFluid();
}

fluid_synth_t *FluidMidiSeq::getSynth()
{
    return synth;
}

void FluidMidiSeq::setLoop(bool enable)
{
    m_sequencer->setLoopEnabled(enable);
}

bool FluidMidiSeq::Open(char *filename)
{
    return m_sequencer->loadMIDI(filename);
}

bool FluidMidiSeq::Load(const void *buf, size_t size)
{
    return m_sequencer->loadMIDI(buf, size);
}

double FluidMidiSeq::Tick(double s, double granularity)
{
    MidiSequencer &seqr = *m_sequencer;
    return seqr.Tick(s, granularity);
}

int FluidMidiSeq::playBuffer(uint8_t *stream, int len)
{
    return m_sequencer->playStream(stream, static_cast<size_t>(len));
}
