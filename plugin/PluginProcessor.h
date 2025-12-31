#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

extern "C" {
#include "ft2_instance.h"
}

/**
 * @class FT2PluginProcessor
 * @brief JUCE AudioProcessor wrapper for the FT2 replayer.
 *
 * This class wraps the C-based FT2 replayer instance into a JUCE
 * AudioProcessor, enabling it to be used as a VST3, AU, or LV2 plugin.
 */
class FT2PluginProcessor : public juce::AudioProcessor
{
public:
    FT2PluginProcessor();
    ~FT2PluginProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    /**
     * @brief Loads an XM file into the plugin.
     * @param fileData The raw XM file data.
     * @return true on success, false on failure.
     */
    bool loadXMFile(const juce::MemoryBlock& fileData);

    /**
     * @brief Starts playback.
     */
    void startPlayback();

    /**
     * @brief Stops playback.
     */
    void stopPlayback();

    /**
     * @brief Returns whether the plugin is currently playing.
     */
    bool isPlaying() const;

    /**
     * @brief Gets the current playback position.
     * @param songPos Output for song position.
     * @param row Output for current row.
     */
    void getPosition(int& songPos, int& row) const;

    /**
     * @brief Gets the FT2 instance for UI access.
     * @return Pointer to the FT2 instance.
     */
    ft2_instance_t* getInstance() { return instance; }
    const ft2_instance_t* getInstance() const { return instance; }

    /**
     * @brief Enable/disable DAW transport sync.
     * @param enabled true to sync with DAW, false for standalone control
     * @note Now reads/writes from instance->config.syncTransportFromDAW
     */
    void setSyncToDAWTransport(bool enabled) { if (instance) instance->config.syncTransportFromDAW = enabled; }
    bool isSyncToDAWTransport() const { return instance ? instance->config.syncTransportFromDAW : true; }

    /**
     * @brief Enable/disable BPM sync from DAW.
     * @param enabled true to sync BPM from DAW
     * @note Now reads/writes from instance->config.syncBpmFromDAW
     */
    void setSyncBPMFromDAW(bool enabled) { if (instance) instance->config.syncBpmFromDAW = enabled; }
    bool isSyncBPMFromDAW() const { return instance ? instance->config.syncBpmFromDAW : true; }

    /** Save nibbles high scores to persistent storage. */
    void saveNibblesHighScores();

    /** Load nibbles high scores from persistent storage. */
    void loadNibblesHighScores();

    /** Save global config to persistent storage. */
    void saveGlobalConfig();

    /** Load global config from persistent storage. */
    void loadGlobalConfig();

    /** Reset config to factory defaults. */
    void resetConfig();

    /** Poll and handle config request flags from C code. */
    void pollConfigRequests();

    /** Get the last version the user was notified about. */
    juce::String getLastNotifiedVersion() const { return lastNotifiedVersion; }

    /** Set and save the last notified version (called after showing update dialog). */
    void setLastNotifiedVersion(const juce::String& version);

    /** Check if automatic update checking is enabled. */
    bool isAutoUpdateCheckEnabled() const { return instance ? instance->config.autoUpdateCheck : true; }

private:
    ft2_instance_t* instance = nullptr;
    double currentSampleRate = 48000.0;
    
    bool wasDAWPlaying = false;  // Track DAW play state for edge detection
    double lastPpqPosition = 0.0;  // Track PPQ for seek detection
    
    // MIDI input state: track which FT2 channel is playing which MIDI note
    static constexpr int MAX_MIDI_NOTES = 128;
    int8_t midiNoteToChannel[MAX_MIDI_NOTES];  // MIDI note -> FT2 channel (-1 = not playing, initialized in ctor)
    int8_t nextMidiChannel = 0;  // Round-robin channel assignment
    
    void processMidiInput(const juce::MidiMessage& msg);
    int8_t allocateMidiChannel();
    void releaseMidiChannel(int8_t channel);
    
    mutable juce::CriticalSection processLock;
    
    std::unique_ptr<juce::ApplicationProperties> appProperties;
    void initAppProperties();

    juce::String lastNotifiedVersion;  // Version user was last notified about (for update checker)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FT2PluginProcessor)
};

