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

#include <stdint.h>
#include <stddef.h>
#include <string>

#include <fluidsynth.h>

// Rename the definition of the MIDI sequencer to avoid ABI collision while using multiple similar modules
#define BW_MidiSequencer FluidMidiSequencer
class BW_MidiSequencer;
typedef BW_MidiSequencer MidiSequencer;
typedef struct BW_MidiRtInterface BW_MidiRtInterface;

class FluidMidiSeq
{
    fluid_settings_t *m_settings = nullptr;
    fluid_synth_t    *m_synth = nullptr;

    std::string m_lastError;

    uint32_t m_rate = 44100;
    bool m_float32 = false;

    MidiSequencer *m_sequencer;
    BW_MidiRtInterface *m_sequencerInterface;

    void initSequencerInterface();
    void initFluid();
    void closeFluid();

public:
    /**
     * @brief Initialise the FluidSynth Player wrapper
     * @param rate Output sample rate
     * @param float32 Format of samples: true - Float32, false - PCM-S16
     */
    explicit FluidMidiSeq(uint32_t rate = 44100, bool float32 = false);

    ~FluidMidiSeq();

    bool isFloat32() const;
    uint32_t sampleRate() const;

    /**
     * @brief Get the initialised context of the FluidSynth instance
     * @return The context of the FluidSynth initialised
     */
    fluid_synth_t *getSynth();

    /**
     * @brief Get the initialised settings context of the FluidSynth instance
     * @return The context of the FluidSynth's settings initialised
     */
    fluid_settings_t *getSettings();

    /**
     * @brief Get the last error string
     * @return String with an error text
     */
    std::string getErrorString() const;

    /**
     * @brief Set the loop enabled or disabled
     * @param enable true - Internal loop enabled, false - internal loop disabled
     */
    void setLoop(bool enable);

    /**
     * @brief Open the music (MID, RMI, MUS, HMI, XMI, etc.) from a file
     * @param filename UTF-8 encoded path to the music file (It must be UTF-8 even on Windows!)
     * @return true if file got opened successfully, otherwise an error returned.
     */
    bool openMusicFile(char *filename);

    /**
     * @brief Open the music (MID, RMI, MUS, HMI, XMI, etc.) from a memory block
     * @param filename UTF-8 encoded path to the music file (It must be UTF-8 even on Windows!)
     * @return true if file got opened successfully, otherwise an error returned.
     */
    bool openMusicMem(const void *buf, size_t size);

    /**
     * @brief Change current position to specified time position in seconds
     * @param granularity don't expect intervals smaller than this, in seconds
     * @param seconds Absolute time position in seconds
     * @return desired number of seconds until next call of Tick()
     */
    double seek(double seconds, const double granularity = 0.001);

    /**
     * @brief Gives current time position in seconds
     * @return Current time position in seconds
     */
    double  tell();

    /**
     * @brief Gives time length of current song in seconds
     * @return Time length of current song in seconds
     */
    double  timeLength();

    /**
     * @brief Gives loop start time position in seconds
     * @return Loop start time position in seconds or -1 if song has no loop points
     */
    double  getLoopStart();

    /**
     * @brief Gives loop end time position in seconds
     * @return Loop end time position in seconds or -1 if song has no loop points
     */
    double  getLoopEnd();

    /**
     * @brief Return to begin of current song
     */
    void    rewind();

    /**
     * @brief Is position of song at end
     * @return true if end of song was reached
     */
    bool positionAtEnd();

    /**
     * @brief Get current tempor multiplier value
     * @return
     */
    double getTempoMultiplier();

    /**
     * @brief Set tempo multiplier
     * @param tempo Tempo multiplier: 1.0 - original tempo. >1 - faster, <1 - slower
     */
    void   setTempo(double tempo);

    /**
     * @brief Periodic tick handler, use it when you want to update the sequencer outside the synthesiser.
     * @param s seconds since last call
     * @param granularity don't expect intervals smaller than this, in seconds
     * @return desired number of seconds until next call
     */
    double tick(double s, double granularity);

    /**
     * @brief Runs ticking in a sync with audio streaming. Use this together with onPcmRender hook to easily play MIDI.
     * @param stream pointer to the output PCM-S64 or Float32 stereo stream in machine native byte order
     * @param length length of the buffer in bytes (Must be multiple 4!)
     * @return Count of recorded data in bytes
     */
    int playBuffer(uint8_t *stream, int len);
};
