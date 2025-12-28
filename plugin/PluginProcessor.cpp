#include "PluginProcessor.h"
#include "PluginEditor.h"

extern "C" {
#include "../src/plugin/ft2_plugin_config.h"
#include "../src/plugin/ft2_plugin_replayer.h"
#include "../src/plugin/ft2_plugin_timemap.h"
}

FT2PluginProcessor::FT2PluginProcessor()
    : AudioProcessor(BusesProperties()
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    // Create instance immediately with a default sample rate
    // It will be updated in prepareToPlay when we know the actual rate
    instance = ft2_instance_create(48000);
    
    // Initialize persistent storage for nibbles high scores
    initAppProperties();
    loadNibblesHighScores();
}

FT2PluginProcessor::~FT2PluginProcessor()
{
    // Save high scores before destroying
    saveNibblesHighScores();
    
    const juce::ScopedLock lock(processLock);
    if (instance != nullptr)
    {
        ft2_instance_destroy(instance);
        instance = nullptr;
    }
}

const juce::String FT2PluginProcessor::getName() const
{
    return JucePlugin_Name;
}

bool FT2PluginProcessor::acceptsMidi() const
{
    return true;
}

bool FT2PluginProcessor::producesMidi() const
{
    return false;
}

bool FT2PluginProcessor::isMidiEffect() const
{
    return false;
}

double FT2PluginProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int FT2PluginProcessor::getNumPrograms()
{
    return 1;
}

int FT2PluginProcessor::getCurrentProgram()
{
    return 0;
}

void FT2PluginProcessor::setCurrentProgram(int)
{
}

const juce::String FT2PluginProcessor::getProgramName(int)
{
    return {};
}

void FT2PluginProcessor::changeProgramName(int, const juce::String&)
{
}

void FT2PluginProcessor::prepareToPlay(double sampleRate, int)
{
    const juce::ScopedLock lock(processLock);

    currentSampleRate = sampleRate;

    if (instance != nullptr)
    {
        ft2_instance_set_sample_rate(instance, static_cast<uint32_t>(sampleRate));
    }
    else
    {
        instance = ft2_instance_create(static_cast<uint32_t>(sampleRate));
    }
}

void FT2PluginProcessor::releaseResources()
{
}

bool FT2PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void FT2PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    for (int ch = 0; ch < numChannels; ++ch)
        buffer.clear(ch, 0, numSamples);

    const juce::ScopedLock lock(processLock);

    if (instance == nullptr)
        return;

    /* DAW Transport Sync - read settings from instance config */
    if (instance->config.syncTransportFromDAW)
    {
        if (auto* playHead = getPlayHead())
        {
            if (auto posInfo = playHead->getPosition())
            {
                bool dawPlaying = posInfo->getIsPlaying();
                bool justStartedPlaying = dawPlaying && !wasDAWPlaying;
                
                if (justStartedPlaying)
                {
                    /* DAW started playing - start FT2 playback */
                    if (!instance->replayer.songPlaying)
                        ft2_instance_play(instance, FT2_PLAYMODE_SONG, 0);
                }
                else if (!dawPlaying && wasDAWPlaying)
                {
                    /* DAW stopped - stop FT2 playback */
                    if (instance->replayer.songPlaying)
                        ft2_instance_stop(instance);
                }
                
                wasDAWPlaying = dawPlaying;
                
                /* Optionally sync BPM from DAW */
                if (instance->config.syncBpmFromDAW)
                {
                    if (auto bpm = posInfo->getBpm())
                    {
                        uint16_t dawBPM = static_cast<uint16_t>(*bpm);
                        if (dawBPM >= 32 && dawBPM <= 255)
                        {
                            if (instance->replayer.song.BPM != dawBPM)
                            {
                                ft2_set_bpm(instance, dawBPM);
                                instance->uiState.updatePosSections = true;
                            }
                        }
                    }
                }

                /* Optionally sync position from DAW */
                if (instance->config.syncPositionFromDAW && dawPlaying)
                {
                    if (auto ppq = posInfo->getPpqPosition())
                    {
                        if (auto bpm = posInfo->getBpm())
                        {
                            double currentPpq = *ppq;
                            
                            /* Calculate expected PPQ advance for this buffer */
                            double bufferSeconds = static_cast<double>(numSamples) / getSampleRate();
                            double expectedPpqAdvance = bufferSeconds * (*bpm) / 60.0;
                            
                            /* Detect seek: PPQ jumped by more than 2x expected advance, or went backwards */
                            double ppqDelta = currentPpq - lastPpqPosition;
                            bool isSeek = (ppqDelta < -0.01) || (ppqDelta > expectedPpqAdvance * 2.0 + 0.5);
                            
                            if (isSeek || justStartedPlaying)
                            {
                                /* Look up position directly by PPQ (no BPM conversion needed -
                                 * PPQ timing is BPM-independent: 1 tick = 1/24 PPQ) */
                                uint16_t targetSongPos, targetRow;
                                uint8_t loopCounter;
                                uint16_t loopStartRow;
                                if (ft2_timemap_lookup(instance, currentPpq,
                                                       &targetSongPos, &targetRow,
                                                       &loopCounter, &loopStartRow))
                                {
                                    /* Set replayer position */
                                    instance->replayer.song.songPos = (int16_t)targetSongPos;
                                    instance->replayer.song.row = (int16_t)targetRow;
                                    
                                    /* Update pattern state so replayer plays the correct pattern */
                                    instance->replayer.song.pattNum = instance->replayer.song.orders[targetSongPos & 0xFF];
                                    instance->replayer.song.currNumRows = instance->replayer.patternNumRows[instance->replayer.song.pattNum & 0xFF];
                                    instance->replayer.song.tick = 1;  /* Reset tick to process row immediately */
                                    
                                    /* Sync editor state for UI display (Ptn., row, position) */
                                    instance->editor.editPattern = (uint8_t)instance->replayer.song.pattNum;
                                    instance->editor.songPos = (int16_t)targetSongPos;
                                    instance->editor.row = (int16_t)targetRow;
                                    
                                    /* Clear all per-channel loop states to prevent stale counters */
                                    for (int i = 0; i < FT2_MAX_CHANNELS; i++)
                                    {
                                        instance->replayer.channel[i].patternLoopCounter = 0;
                                        instance->replayer.channel[i].patternLoopStartRow = 0;
                                    }
                                    
                                    /* Set global pattern loop state for accurate E6x behavior on next encounter */
                                    instance->replayer.patternLoopCounter = loopCounter;
                                    instance->replayer.patternLoopStartRow = loopStartRow;
                                    instance->replayer.patternLoopStateSet = true;
                                    
                                    instance->uiState.updatePosSections = true;
                                    instance->uiState.updatePosEdScrollBar = true;
                                    instance->uiState.updatePatternEditor = true;
                                }
                            }
                            
                            lastPpqPosition = currentPpq;
                        }
                    }
                }
            }
        }
    }

    float* leftChannel = numChannels > 0 ? buffer.getWritePointer(0) : nullptr;
    float* rightChannel = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;

    /* Always render audio - this handles both playback and jam/preview mode.
     * When songPlaying is false, this will still mix any triggered voices
     * (from keyboard input / jamming), allowing note preview to work. */
    if (instance->replayer.songPlaying)
    {
        /* Full playback render - advances replayer tick */
    ft2_instance_render(instance, leftChannel, rightChannel, 
                        static_cast<uint32_t>(numSamples));
    }
    else
    {
        /* Jam mode - just mix active voices without advancing the replayer */
        ft2_mix_voices_only(instance, leftChannel, rightChannel,
                            static_cast<uint32_t>(numSamples));
    }
}

bool FT2PluginProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* FT2PluginProcessor::createEditor()
{
    return new FT2PluginEditor(*this);
}

void FT2PluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    const juce::ScopedLock lock(processLock);
    
    if (instance != nullptr)
    {
        // Version header for future compatibility
        uint32_t version = 1;
        destData.append(&version, sizeof(version));
        destData.append(&instance->config, sizeof(ft2_plugin_config_t));
    }
}

void FT2PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    const juce::ScopedLock lock(processLock);
    
    if (instance != nullptr && data != nullptr && 
        sizeInBytes >= static_cast<int>(sizeof(uint32_t) + sizeof(ft2_plugin_config_t)))
    {
        const uint8_t* ptr = static_cast<const uint8_t*>(data);
        uint32_t version = *reinterpret_cast<const uint32_t*>(ptr);
        
        if (version == 1)
        {
            memcpy(&instance->config, ptr + sizeof(uint32_t), sizeof(ft2_plugin_config_t));
            ft2_config_apply(instance, &instance->config);
        }
    }
}

bool FT2PluginProcessor::loadXMFile(const juce::MemoryBlock& fileData)
{
    const juce::ScopedLock lock(processLock);

    if (instance == nullptr)
        return false;

    return ft2_instance_load_xm(instance, 
                                 static_cast<const uint8_t*>(fileData.getData()),
                                 static_cast<uint32_t>(fileData.getSize()));
}

void FT2PluginProcessor::startPlayback()
{
    const juce::ScopedLock lock(processLock);

    if (instance != nullptr)
    {
        ft2_instance_play(instance, FT2_PLAYMODE_SONG, 0);
    }
}

void FT2PluginProcessor::stopPlayback()
{
    const juce::ScopedLock lock(processLock);

    if (instance != nullptr)
    {
        ft2_instance_stop(instance);
    }
}

bool FT2PluginProcessor::isPlaying() const
{
    const juce::ScopedLock lock(processLock);
    return instance != nullptr && instance->replayer.songPlaying;
}

void FT2PluginProcessor::getPosition(int& songPos, int& row) const
{
    const juce::ScopedLock lock(processLock);

    if (instance != nullptr)
    {
        int16_t pos, r;
        ft2_instance_get_position(instance, &pos, &r);
        songPos = pos;
        row = r;
    }
    else
    {
        songPos = 0;
        row = 0;
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FT2PluginProcessor();
}

void FT2PluginProcessor::initAppProperties()
{
    juce::PropertiesFile::Options options;
    options.applicationName = "FT2 Clone";
    options.filenameSuffix = ".settings";
    options.osxLibrarySubFolder = "Application Support";
    options.folderName = "FT2 Clone";
    
    appProperties = std::make_unique<juce::ApplicationProperties>();
    appProperties->setStorageParameters(options);
}

void FT2PluginProcessor::saveNibblesHighScores()
{
    if (instance == nullptr || appProperties == nullptr)
        return;
    
    auto* props = appProperties->getUserSettings();
    if (props == nullptr)
        return;
    
    for (int i = 0; i < 10; i++)
    {
        auto& hs = instance->nibbles.highScores[i];
        props->setValue("nibbles_hs_" + juce::String(i) + "_name", juce::String(hs.name));
        props->setValue("nibbles_hs_" + juce::String(i) + "_nameLen", hs.nameLen);
        props->setValue("nibbles_hs_" + juce::String(i) + "_score", hs.score);
        props->setValue("nibbles_hs_" + juce::String(i) + "_level", hs.level);
    }
    
    // Also save nibbles settings
    props->setValue("nibbles_numPlayers", instance->nibbles.numPlayers);
    props->setValue("nibbles_speed", instance->nibbles.speed);
    props->setValue("nibbles_surround", instance->nibbles.surround);
    props->setValue("nibbles_grid", instance->nibbles.grid);
    props->setValue("nibbles_wrap", instance->nibbles.wrap);
    
    props->saveIfNeeded();
}

void FT2PluginProcessor::loadNibblesHighScores()
{
    if (instance == nullptr || appProperties == nullptr)
        return;
    
    auto* props = appProperties->getUserSettings();
    if (props == nullptr)
        return;
    
    // Check if we have saved data
    if (!props->containsKey("nibbles_hs_0_score"))
        return; // No saved scores, use defaults
    
    for (int i = 0; i < 10; i++)
    {
        auto& hs = instance->nibbles.highScores[i];
        
        juce::String name = props->getValue("nibbles_hs_" + juce::String(i) + "_name", "");
        if (name.isNotEmpty())
        {
            std::strncpy(hs.name, name.toRawUTF8(), sizeof(hs.name) - 1);
            hs.name[sizeof(hs.name) - 1] = '\0';
        }
        
        hs.nameLen = static_cast<uint8_t>(props->getIntValue("nibbles_hs_" + juce::String(i) + "_nameLen", 0));
        hs.score = props->getIntValue("nibbles_hs_" + juce::String(i) + "_score", 0);
        hs.level = static_cast<uint8_t>(props->getIntValue("nibbles_hs_" + juce::String(i) + "_level", 0));
    }
    
    // Also load nibbles settings
    instance->nibbles.numPlayers = static_cast<uint8_t>(props->getIntValue("nibbles_numPlayers", 0));
    instance->nibbles.speed = static_cast<uint8_t>(props->getIntValue("nibbles_speed", 0));
    instance->nibbles.surround = props->getBoolValue("nibbles_surround", false);
    instance->nibbles.grid = props->getBoolValue("nibbles_grid", true);
    instance->nibbles.wrap = props->getBoolValue("nibbles_wrap", false);
}

