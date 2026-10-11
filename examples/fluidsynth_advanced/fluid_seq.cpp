/*
 * FluidMidiSeq - An example implementation of the MIDI player over FluidSynth
 * using the BW_Midi_Sequencer
 *
 * Copyright (c) 2015-2026 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */


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

    if(m.isFloat32())
        fluid_synth_write_float(m.getSynth(), static_cast<int>(length / 8),
                                stream, 0, 2,
                                stream, 1, 2);
    else
        fluid_synth_write_s16(m.getSynth(), static_cast<int>(length / 4),
                              stream, 0, 2,
                              stream, 1, 2);
}


void FluidMidiSeq::initSequencerInterface()
{
    double samplerate;
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
    seq->pcmFrameSize = 2 /*channels*/ * (m_float32 ? 4 : 2) /*size of one sample*/;
    fluid_settings_setnum(m_settings, "synth.sample-rate", (double)seq->pcmSampleRate);
    fluid_settings_getnum(m_settings, "synth.sample-rate", &samplerate);
    m_rate = seq->pcmSampleRate = (int)samplerate; // De-facto

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
    m_settings = new_fluid_settings();
    m_synth = new_fluid_synth(m_settings);
}

void FluidMidiSeq::closeFluid()
{
    if(m_settings && m_synth)
    {
        delete_fluid_synth(m_synth);
        delete_fluid_settings(m_settings);
        m_settings = nullptr;
        m_synth = nullptr;
    }
}

FluidMidiSeq::FluidMidiSeq(uint32_t rate, bool float32)
{
    m_settings = nullptr;
    m_synth = nullptr;

    m_sequencer = nullptr;
    m_sequencerInterface = nullptr;
    m_rate = rate;
    m_float32 = float32;

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

bool FluidMidiSeq::isFloat32() const
{
    return m_float32;
}

uint32_t FluidMidiSeq::sampleRate() const
{
    return m_rate;
}

fluid_synth_t *FluidMidiSeq::getSynth()
{
    return m_synth;
}

fluid_settings_t *FluidMidiSeq::getSettings()
{
    return m_settings;
}

std::string FluidMidiSeq::getErrorString() const
{
    return m_lastError;
}

void FluidMidiSeq::setLoop(bool enable)
{
    m_sequencer->setLoopEnabled(enable);
}

bool FluidMidiSeq::openMusicFile(char *filename)
{
    m_lastError.clear();
    bool ret = m_sequencer->loadMIDI(filename);

    if(!ret)
        m_lastError = m_sequencer->getErrorString();

    return ret;
}

bool FluidMidiSeq::openMusicMem(const void *buf, size_t size)
{
    m_lastError.clear();
    bool ret = m_sequencer->loadMIDI(buf, size);

    if(!ret)
        m_lastError = m_sequencer->getErrorString();

    return ret;
}

double FluidMidiSeq::seek(double seconds, const double granularity)
{
    return m_sequencer->seek(seconds, granularity);
}

double FluidMidiSeq::tell()
{
    return m_sequencer->tell();
}

double FluidMidiSeq::timeLength()
{
    return m_sequencer->timeLength();
}

double FluidMidiSeq::getLoopStart()
{
    return m_sequencer->getLoopStart();
}

double FluidMidiSeq::getLoopEnd()
{
    return m_sequencer->getLoopEnd();
}

void FluidMidiSeq::rewind()
{
    return m_sequencer->rewind();
}

bool FluidMidiSeq::positionAtEnd()
{
    return m_sequencer->positionAtEnd();
}

double FluidMidiSeq::getTempoMultiplier()
{
    return m_sequencer->getTempoMultiplier();
}

void FluidMidiSeq::setTempo(double tempo)
{
    return m_sequencer->setTempo(tempo);
}

double FluidMidiSeq::tick(double s, double granularity)
{
    MidiSequencer &seqr = *m_sequencer;
    return seqr.Tick(s, granularity);
}

int FluidMidiSeq::playBuffer(uint8_t *stream, int len)
{
    return m_sequencer->playStream(stream, static_cast<size_t>(len));
}
