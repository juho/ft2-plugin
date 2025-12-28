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

private:
    ft2_instance_t* instance = nullptr;
    double currentSampleRate = 48000.0;
    
    bool wasDAWPlaying = false;  // Track DAW play state for edge detection
    double lastPpqPosition = 0.0;  // Track PPQ for seek detection
    
    mutable juce::CriticalSection processLock;
    
    std::unique_ptr<juce::ApplicationProperties> appProperties;
    void initAppProperties();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FT2PluginProcessor)
};

