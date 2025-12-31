#include "PluginProcessor.h"
#include "PluginEditor.h"

extern "C" {
#include "../src/plugin/ft2_plugin_config.h"
#include "../src/plugin/ft2_plugin_replayer.h"
#include "../src/plugin/ft2_plugin_timemap.h"
#include "../src/plugin/ft2_plugin_diskop.h"
#include "../src/plugin/ft2_plugin_loader.h"
}

FT2PluginProcessor::FT2PluginProcessor()
    : AudioProcessor([]()
        {
            BusesProperties props;
            props = props.withOutput("Main", juce::AudioChannelSet::stereo(), true);
            // 15 output buses (30 channels) + Main (2 channels) = 32 channels (Ableton's limit)
            // Tracker channels are routed to outputs via config (default: wrap around)
            for (int i = 1; i <= FT2_NUM_OUTPUTS; ++i)
                props = props.withOutput("Out " + juce::String(i), juce::AudioChannelSet::stereo(), true);
            return props;
        }())
{
    // Initialize MIDI note tracking - all notes start with no channel assigned
    for (int i = 0; i < MAX_MIDI_NOTES; ++i)
        midiNoteToChannel[i] = -1;
    
    // Create instance immediately with a default sample rate
    // It will be updated in prepareToPlay when we know the actual rate
    instance = ft2_instance_create(48000);
    
    // Initialize persistent storage
    initAppProperties();
    
    // Load global config (if exists) and nibbles high scores
    loadGlobalConfig();
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
    return true;
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
    // Main output (bus 0) must be stereo
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // Channel outputs (bus 1-32) can be stereo or disabled
    for (int i = 1; i < layouts.outputBuses.size(); ++i)
    {
        const auto& bus = layouts.outputBuses[i];
        if (!bus.isDisabled() && bus != juce::AudioChannelSet::stereo())
            return false;
    }

    return true;
}

void FT2PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    for (int ch = 0; ch < numChannels; ++ch)
        buffer.clear(ch, 0, numSamples);

    const juce::ScopedLock lock(processLock);

    if (instance == nullptr)
        return;

    /* Process MIDI input messages */
    if (instance->config.midiEnabled)
    {
        for (const auto metadata : midiMessages)
            processMidiInput(metadata.getMessage());
    }

    /* DAW BPM Sync (independent of transport sync) */
    if (instance->config.syncBpmFromDAW)
    {
        if (auto* playHead = getPlayHead())
        {
            if (auto posInfo = playHead->getPosition())
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
        }
    }

    /* DAW Transport Sync (start/stop) */
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

    /* Check if we have more than stereo output (indicates multi-out is active) */
    const int totalChannels = buffer.getNumChannels();
    const bool hasMultiOut = (totalChannels > 2);

    /* Main output pointers (bus 0 = channels 0-1) */
    float* mainL = buffer.getWritePointer(0);
    float* mainR = totalChannels > 1 ? buffer.getWritePointer(1) : nullptr;

    if (hasMultiOut)
    {
        /* Ensure multi-out buffers are allocated */
        if (!instance->audio.multiOutEnabled || 
            instance->audio.multiOutBufferSize < static_cast<uint32_t>(numSamples))
        {
            ft2_instance_set_multiout(instance, true, static_cast<uint32_t>(numSamples));
        }

        /* Render with per-channel outputs */
        if (instance->replayer.songPlaying)
        {
            ft2_instance_render_multiout(instance, mainL, mainR, static_cast<uint32_t>(numSamples));

            /* Copy 16 output buffers to enabled DAW output buses
             * (Tracker channels are already routed to these 16 buffers via config) */
            int bufferChannelOffset = 2; // Start after Main (channels 0-1)
            for (int out = 0; out < 16; ++out)
            {
                const int busIdx = out + 1; // Bus 0=Main, Bus 1=Out1, Bus 2=Out2, etc.
                auto* bus = getBus(false, busIdx);
                
                if (bus == nullptr || !bus->isEnabled())
                    continue; // Skip disabled buses
                
                /* This bus is enabled - copy its data to the buffer */
                if (bufferChannelOffset + 1 < totalChannels)
                {
                    float* outL = buffer.getWritePointer(bufferChannelOffset);
                    float* outR = buffer.getWritePointer(bufferChannelOffset + 1);

                    if (outL != nullptr && instance->audio.fChannelBufferL[out] != nullptr)
                        std::memcpy(outL, instance->audio.fChannelBufferL[out], 
                                    static_cast<size_t>(numSamples) * sizeof(float));
                    if (outR != nullptr && instance->audio.fChannelBufferR[out] != nullptr)
                        std::memcpy(outR, instance->audio.fChannelBufferR[out], 
                                    static_cast<size_t>(numSamples) * sizeof(float));
                }
                
                bufferChannelOffset += 2; // Move to next stereo pair
            }
        }
        else
        {
            /* Not playing - mix keyjazz voices to main only and silence multi-outs */
            ft2_mix_voices_only(instance, mainL, mainR, static_cast<uint32_t>(numSamples));

            /* Clear all multi-out channels (bus channels 2+) to silence */
            for (int ch = 2; ch < totalChannels; ++ch)
            {
                float* chPtr = buffer.getWritePointer(ch);
                if (chPtr != nullptr)
                    std::memset(chPtr, 0, static_cast<size_t>(numSamples) * sizeof(float));
            }
        }
    }
    else
    {
        /* Standard stereo render (more efficient when multi-out not used) */
        if (instance->replayer.songPlaying)
            ft2_instance_render(instance, mainL, mainR, static_cast<uint32_t>(numSamples));
        else
            ft2_mix_voices_only(instance, mainL, mainR, static_cast<uint32_t>(numSamples));
    }

    /* Process MIDI output queue - convert FT2 events to JUCE MidiBuffer */
    ft2_midi_event_t midiEvent;
    while (ft2_midi_queue_pop(instance, &midiEvent))
    {
        int samplePos = juce::jlimit(0, numSamples - 1, midiEvent.samplePos);
        
        switch (midiEvent.type)
        {
            case FT2_MIDI_NOTE_ON:
                midiMessages.addEvent(
                    juce::MidiMessage::noteOn(midiEvent.channel + 1, midiEvent.note, midiEvent.velocity),
                    samplePos);
                break;
                
            case FT2_MIDI_NOTE_OFF:
                midiMessages.addEvent(
                    juce::MidiMessage::noteOff(midiEvent.channel + 1, midiEvent.note),
                    samplePos);
                break;
                
            case FT2_MIDI_PROGRAM_CHANGE:
                midiMessages.addEvent(
                    juce::MidiMessage::programChange(midiEvent.channel + 1, midiEvent.program),
                    samplePos);
                break;
        }
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
    
    if (instance == nullptr)
        return;
    
    uint32_t version = 1;
    destData.append(&version, sizeof(version));
    
    // Config
    destData.append(&instance->config, sizeof(ft2_plugin_config_t));
    
    // Editor state
    destData.append(&instance->editor.curInstr, 1);
    destData.append(&instance->editor.curSmp, 1);
    destData.append(&instance->editor.editPattern, 1);
    destData.append(&instance->editor.curOctave, 1);
    int16_t songPos = instance->editor.songPos;
    destData.append(&songPos, sizeof(songPos));
    
    // Module as XM
    uint8_t *moduleData = nullptr;
    uint32_t moduleSize = 0;
    if (ft2_save_module(instance, &moduleData, &moduleSize) && moduleData != nullptr)
    {
        destData.append(&moduleSize, sizeof(moduleSize));
        destData.append(moduleData, moduleSize);
        free(moduleData);
    }
    else
    {
        uint32_t zero = 0;
        destData.append(&zero, sizeof(zero));
    }
}

void FT2PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    const juce::ScopedLock lock(processLock);
    
    if (instance == nullptr || data == nullptr || sizeInBytes < 4)
        return;
    
    const uint8_t* ptr = static_cast<const uint8_t*>(data);
    uint32_t version;
    memcpy(&version, ptr, sizeof(version));
    ptr += sizeof(version);
    int remaining = sizeInBytes - static_cast<int>(sizeof(version));
    
    if (version != 1)
        return;
    
    // Config
    if (remaining < static_cast<int>(sizeof(ft2_plugin_config_t)))
        return;
    memcpy(&instance->config, ptr, sizeof(ft2_plugin_config_t));
    ft2_config_apply(instance, &instance->config);
    ptr += sizeof(ft2_plugin_config_t);
    remaining -= static_cast<int>(sizeof(ft2_plugin_config_t));
    
    // Editor state (6 bytes)
    if (remaining < 6)
        return;
    uint8_t curInstr = *ptr++;
    uint8_t curSmp = *ptr++;
    uint8_t editPattern = *ptr++;
    uint8_t curOctave = *ptr++;
    int16_t songPos;
    memcpy(&songPos, ptr, sizeof(songPos));
    ptr += sizeof(songPos);
    remaining -= 6;
    
    // Module size
    if (remaining < 4)
        return;
    uint32_t moduleSize;
    memcpy(&moduleSize, ptr, sizeof(moduleSize));
    ptr += sizeof(moduleSize);
    remaining -= static_cast<int>(sizeof(moduleSize));
    
    // Module data
    if (moduleSize > 0 && remaining >= static_cast<int>(moduleSize))
    {
        ft2_load_module(instance, ptr, moduleSize);
        
        // Restore editor state
        instance->editor.curInstr = curInstr;
        instance->editor.curSmp = curSmp;
        instance->editor.editPattern = editPattern;
        instance->editor.curOctave = curOctave;
        instance->editor.songPos = songPos;
        
        // Trigger UI refresh
        instance->uiState.needsFullRedraw = true;
        instance->uiState.updatePosSections = true;
        instance->uiState.updatePatternEditor = true;
        instance->uiState.updateInstrSwitcher = true;
        instance->uiState.updateSampleEditor = true;
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

// Global config version - increment when adding new fields that need migration
static constexpr int GLOBAL_CONFIG_VERSION = 1;

void FT2PluginProcessor::saveGlobalConfig()
{
    if (instance == nullptr || appProperties == nullptr)
        return;
    
    auto* props = appProperties->getUserSettings();
    if (props == nullptr)
        return;
    
    const auto& cfg = instance->config;
    
    // Version for future migrations
    props->setValue("config_version", GLOBAL_CONFIG_VERSION);
    
    // Pattern editor settings
    props->setValue("config_ptnStretch", cfg.ptnStretch);
    props->setValue("config_ptnHex", cfg.ptnHex);
    props->setValue("config_ptnInstrZero", cfg.ptnInstrZero);
    props->setValue("config_ptnFrmWrk", cfg.ptnFrmWrk);
    props->setValue("config_ptnLineLight", cfg.ptnLineLight);
    props->setValue("config_ptnShowVolColumn", cfg.ptnShowVolColumn);
    props->setValue("config_ptnChnNumbers", cfg.ptnChnNumbers);
    props->setValue("config_ptnAcc", cfg.ptnAcc);
    props->setValue("config_ptnFont", cfg.ptnFont);
    props->setValue("config_ptnMaxChannels", cfg.ptnMaxChannels);
    props->setValue("config_ptnLineLightStep", cfg.ptnLineLightStep);
    
    // Recording/Editing settings
    props->setValue("config_multiRec", cfg.multiRec);
    props->setValue("config_multiKeyJazz", cfg.multiKeyJazz);
    props->setValue("config_multiEdit", cfg.multiEdit);
    props->setValue("config_recRelease", cfg.recRelease);
    props->setValue("config_recQuant", cfg.recQuant);
    props->setValue("config_recQuantRes", cfg.recQuantRes);
    props->setValue("config_recTrueInsert", cfg.recTrueInsert);
    
    // Audio/Mixer settings
    props->setValue("config_interpolation", cfg.interpolation);
    props->setValue("config_boostLevel", cfg.boostLevel);
    props->setValue("config_masterVol", cfg.masterVol);
    props->setValue("config_volumeRamp", cfg.volumeRamp);
    
    // Visual settings
    props->setValue("config_linedScopes", cfg.linedScopes);
    
    // Sample editor settings
    props->setValue("config_smpEdNote", cfg.smpEdNote);
    
    // Miscellaneous settings
    props->setValue("config_smpCutToBuffer", cfg.smpCutToBuffer);
    props->setValue("config_ptnCutToBuffer", cfg.ptnCutToBuffer);
    props->setValue("config_killNotesOnStopPlay", cfg.killNotesOnStopPlay);
    
    // DAW sync settings
    props->setValue("config_syncBpmFromDAW", cfg.syncBpmFromDAW);
    props->setValue("config_syncTransportFromDAW", cfg.syncTransportFromDAW);
    props->setValue("config_syncPositionFromDAW", cfg.syncPositionFromDAW);
    props->setValue("config_allowFxxSpeedChanges", cfg.allowFxxSpeedChanges);
    
    // MIDI input settings
    props->setValue("config_midiEnabled", cfg.midiEnabled);
    props->setValue("config_midiAllChannels", cfg.midiAllChannels);
    props->setValue("config_midiChannel", cfg.midiChannel);
    props->setValue("config_midiTranspose", cfg.midiTranspose);
    props->setValue("config_midiVelocitySens", cfg.midiVelocitySens);
    props->setValue("config_midiRecordVelocity", cfg.midiRecordVelocity);
    props->setValue("config_midiTriggerPatterns", cfg.midiTriggerPatterns);
    
    // Miscellaneous
    props->setValue("config_autoUpdateCheck", cfg.autoUpdateCheck);
    
    // Palette
    props->setValue("config_palettePreset", cfg.palettePreset);
    
    // Logo/Badge settings
    props->setValue("config_id_FastLogo", cfg.id_FastLogo);
    props->setValue("config_id_TritonProd", cfg.id_TritonProd);
    
    // Channel output routing (32 values as comma-separated string)
    juce::String routingStr;
    for (int i = 0; i < 32; ++i)
    {
        if (i > 0) routingStr += ",";
        routingStr += juce::String(cfg.channelRouting[i]);
    }
    props->setValue("config_channelRouting", routingStr);
    
    // Channel to main flags (32 values as comma-separated string)
    juce::String toMainStr;
    for (int i = 0; i < 32; ++i)
    {
        if (i > 0) toMainStr += ",";
        toMainStr += cfg.channelToMain[i] ? "1" : "0";
    }
    props->setValue("config_channelToMain", toMainStr);
    
    // Update checker - last notified version
    props->setValue("lastNotifiedVersion", lastNotifiedVersion);
    
    props->saveIfNeeded();
}

void FT2PluginProcessor::loadGlobalConfig()
{
    if (instance == nullptr || appProperties == nullptr)
        return;
    
    auto* props = appProperties->getUserSettings();
    if (props == nullptr)
        return;
    
    // Check if we have saved config data (look for any config key)
    if (!props->containsKey("config_ptnHex") && !props->containsKey("config_version"))
        return; // No saved config, use current defaults
    
    // Read version (0 if not present = pre-versioning config)
    const int version = props->getIntValue("config_version", 0);
    
    // Future migration hooks:
    // if (version < 2) { /* migrate from v1 to v2 */ }
    // if (version < 3) { /* migrate from v2 to v3 */ }
    (void)version; // Suppress unused warning until we need migrations
    
    auto& cfg = instance->config;
    
    // Pattern editor settings
    cfg.ptnStretch = props->getBoolValue("config_ptnStretch", cfg.ptnStretch);
    cfg.ptnHex = props->getBoolValue("config_ptnHex", cfg.ptnHex);
    cfg.ptnInstrZero = props->getBoolValue("config_ptnInstrZero", cfg.ptnInstrZero);
    cfg.ptnFrmWrk = props->getBoolValue("config_ptnFrmWrk", cfg.ptnFrmWrk);
    cfg.ptnLineLight = props->getBoolValue("config_ptnLineLight", cfg.ptnLineLight);
    cfg.ptnShowVolColumn = props->getBoolValue("config_ptnShowVolColumn", cfg.ptnShowVolColumn);
    cfg.ptnChnNumbers = props->getBoolValue("config_ptnChnNumbers", cfg.ptnChnNumbers);
    cfg.ptnAcc = props->getBoolValue("config_ptnAcc", cfg.ptnAcc);
    cfg.ptnFont = static_cast<uint8_t>(props->getIntValue("config_ptnFont", cfg.ptnFont));
    cfg.ptnMaxChannels = static_cast<uint8_t>(props->getIntValue("config_ptnMaxChannels", cfg.ptnMaxChannels));
    cfg.ptnLineLightStep = static_cast<uint8_t>(props->getIntValue("config_ptnLineLightStep", cfg.ptnLineLightStep));
    
    // Recording/Editing settings
    cfg.multiRec = props->getBoolValue("config_multiRec", cfg.multiRec);
    cfg.multiKeyJazz = props->getBoolValue("config_multiKeyJazz", cfg.multiKeyJazz);
    cfg.multiEdit = props->getBoolValue("config_multiEdit", cfg.multiEdit);
    cfg.recRelease = props->getBoolValue("config_recRelease", cfg.recRelease);
    cfg.recQuant = props->getBoolValue("config_recQuant", cfg.recQuant);
    cfg.recQuantRes = static_cast<uint8_t>(props->getIntValue("config_recQuantRes", cfg.recQuantRes));
    cfg.recTrueInsert = props->getBoolValue("config_recTrueInsert", cfg.recTrueInsert);
    
    // Audio/Mixer settings
    cfg.interpolation = static_cast<uint8_t>(props->getIntValue("config_interpolation", cfg.interpolation));
    cfg.boostLevel = static_cast<uint8_t>(props->getIntValue("config_boostLevel", cfg.boostLevel));
    cfg.masterVol = static_cast<uint16_t>(props->getIntValue("config_masterVol", cfg.masterVol));
    cfg.volumeRamp = props->getBoolValue("config_volumeRamp", cfg.volumeRamp);
    
    // Visual settings
    cfg.linedScopes = props->getBoolValue("config_linedScopes", cfg.linedScopes);
    
    // Sample editor settings
    cfg.smpEdNote = static_cast<uint8_t>(props->getIntValue("config_smpEdNote", cfg.smpEdNote));
    
    // Miscellaneous settings
    cfg.smpCutToBuffer = props->getBoolValue("config_smpCutToBuffer", cfg.smpCutToBuffer);
    cfg.ptnCutToBuffer = props->getBoolValue("config_ptnCutToBuffer", cfg.ptnCutToBuffer);
    cfg.killNotesOnStopPlay = props->getBoolValue("config_killNotesOnStopPlay", cfg.killNotesOnStopPlay);
    
    // DAW sync settings
    cfg.syncBpmFromDAW = props->getBoolValue("config_syncBpmFromDAW", cfg.syncBpmFromDAW);
    cfg.syncTransportFromDAW = props->getBoolValue("config_syncTransportFromDAW", cfg.syncTransportFromDAW);
    cfg.syncPositionFromDAW = props->getBoolValue("config_syncPositionFromDAW", cfg.syncPositionFromDAW);
    cfg.allowFxxSpeedChanges = props->getBoolValue("config_allowFxxSpeedChanges", cfg.allowFxxSpeedChanges);
    
    // MIDI input settings
    cfg.midiEnabled = props->getBoolValue("config_midiEnabled", cfg.midiEnabled);
    cfg.midiAllChannels = props->getBoolValue("config_midiAllChannels", cfg.midiAllChannels);
    cfg.midiChannel = static_cast<uint8_t>(props->getIntValue("config_midiChannel", cfg.midiChannel));
    cfg.midiTranspose = static_cast<int8_t>(props->getIntValue("config_midiTranspose", cfg.midiTranspose));
    cfg.midiVelocitySens = static_cast<uint8_t>(props->getIntValue("config_midiVelocitySens", cfg.midiVelocitySens));
    cfg.midiRecordVelocity = props->getBoolValue("config_midiRecordVelocity", cfg.midiRecordVelocity);
    cfg.midiTriggerPatterns = props->getBoolValue("config_midiTriggerPatterns", cfg.midiTriggerPatterns);
    
    // Miscellaneous
    cfg.autoUpdateCheck = props->getBoolValue("config_autoUpdateCheck", cfg.autoUpdateCheck);
    
    // Palette
    cfg.palettePreset = static_cast<uint8_t>(props->getIntValue("config_palettePreset", cfg.palettePreset));
    
    // Logo/Badge settings
    cfg.id_FastLogo = props->getBoolValue("config_id_FastLogo", cfg.id_FastLogo);
    cfg.id_TritonProd = props->getBoolValue("config_id_TritonProd", cfg.id_TritonProd);
    
    // Channel output routing
    juce::String routingStr = props->getValue("config_channelRouting", "");
    if (routingStr.isNotEmpty())
    {
        juce::StringArray tokens;
        tokens.addTokens(routingStr, ",", "");
        for (int i = 0; i < 32 && i < tokens.size(); ++i)
            cfg.channelRouting[i] = static_cast<uint8_t>(tokens[i].getIntValue() % FT2_NUM_OUTPUTS);
    }
    
    // Channel to main flags
    juce::String toMainStr = props->getValue("config_channelToMain", "");
    if (toMainStr.isNotEmpty())
    {
        juce::StringArray tokens;
        tokens.addTokens(toMainStr, ",", "");
        for (int i = 0; i < 32 && i < tokens.size(); ++i)
            cfg.channelToMain[i] = (tokens[i].getIntValue() != 0);
    }
    
    // Apply the loaded config
    ft2_config_apply(instance, &cfg);
    
    // Update checker - last notified version
    lastNotifiedVersion = props->getValue("lastNotifiedVersion", "");
}

void FT2PluginProcessor::setLastNotifiedVersion(const juce::String& version)
{
    lastNotifiedVersion = version;
    
    // Save immediately so it persists
    if (appProperties != nullptr)
    {
        auto* props = appProperties->getUserSettings();
        if (props != nullptr)
        {
            props->setValue("lastNotifiedVersion", version);
            props->saveIfNeeded();
        }
    }
}

void FT2PluginProcessor::resetConfig()
{
    if (instance == nullptr)
        return;
    
    ft2_config_init(&instance->config);
    ft2_config_apply(instance, &instance->config);
    instance->uiState.needsFullRedraw = true;
}

void FT2PluginProcessor::pollConfigRequests()
{
    if (instance == nullptr)
        return;
    
    if (instance->uiState.requestResetConfig)
    {
        instance->uiState.requestResetConfig = false;
        resetConfig();
    }
    
    if (instance->uiState.requestLoadGlobalConfig)
    {
        instance->uiState.requestLoadGlobalConfig = false;
        loadGlobalConfig();
        instance->uiState.needsFullRedraw = true;
    }
    
    if (instance->uiState.requestSaveGlobalConfig)
    {
        instance->uiState.requestSaveGlobalConfig = false;
        saveGlobalConfig();
    }
}

int8_t FT2PluginProcessor::allocateMidiChannel()
{
    if (instance == nullptr)
        return -1;
    
    const int numChannels = instance->replayer.song.numChannels;
    if (numChannels <= 0)
        return 0;
    
    /* Round-robin allocation */
    int8_t channel = nextMidiChannel;
    nextMidiChannel = (nextMidiChannel + 1) % numChannels;
    return channel;
}

void FT2PluginProcessor::releaseMidiChannel(int8_t channel)
{
    /* Nothing special needed for round-robin - just release the note */
    (void)channel;
}

void FT2PluginProcessor::processMidiInput(const juce::MidiMessage& msg)
{
    if (instance == nullptr)
        return;
    
    const auto& cfg = instance->config;
    
    /* Check MIDI channel filter */
    if (!cfg.midiAllChannels)
    {
        int msgChannel = msg.getChannel();  /* 1-16 */
        if (msgChannel != cfg.midiChannel)
            return;
    }
    
    /* Pattern trigger mode: MIDI notes trigger patterns instead of instrument notes */
    if (cfg.midiTriggerPatterns)
    {
        if (msg.isNoteOn())
        {
            int midiNote = msg.getNoteNumber();  /* 0-127 maps to pattern 0-127 */
            
            /* Stop any currently playing pattern first */
            if (instance->replayer.songPlaying)
                ft2_instance_stop(instance);
            
            /* Play pattern corresponding to MIDI note */
            ft2_instance_play_pattern(instance, static_cast<uint8_t>(midiNote), 0);
        }
        else if (msg.isNoteOff())
        {
            /* Stop playback when note is released */
            ft2_instance_stop(instance);
        }
        return;
    }
    
    /* Normal mode: trigger instrument notes */
    if (msg.isNoteOn())
    {
        int midiNote = msg.getNoteNumber();
        int velocity = msg.getVelocity();
        
        /* Convert MIDI note to FT2 note: MIDI 60 = C4, FT2 48 = C-4 */
        /* So: ft2Note = midiNote - 12 (MIDI 60 -> FT2 48) */
        /* But standalone uses -11 offset in midiInKeyAction, so: ft2Note = midiNote - 11 */
        int8_t ft2Note = static_cast<int8_t>(midiNote - 11);
        
        /* Apply transpose */
        ft2Note += cfg.midiTranspose;
        
        /* Clamp to FT2 note range (1-96) */
        if (ft2Note < 1 || ft2Note > 96)
            return;
        
        /* Convert velocity (0-127) to FT2 volume (0-64) with sensitivity */
        int vol = (velocity * 64 * cfg.midiVelocitySens) / (127 * 100);
        if (vol > 64) vol = 64;
        if (velocity > 0 && vol == 0) vol = 1;  /* Match standalone bugfix */
        
        /* Allocate FT2 channel */
        int8_t channel = allocateMidiChannel();
        if (channel < 0)
            return;
        
        /* Track which channel this note is playing on */
        midiNoteToChannel[midiNote] = channel;
        
        /* Trigger note */
        ft2_instance_trigger_note(instance, ft2Note, instance->editor.curInstr, 
                                  static_cast<uint8_t>(channel), static_cast<uint8_t>(vol));
    }
    else if (msg.isNoteOff())
    {
        int midiNote = msg.getNoteNumber();
        
        /* Find which channel this note was playing on */
        int8_t channel = midiNoteToChannel[midiNote];
        if (channel < 0)
            return;
        
        /* Release the note */
        ft2_instance_release_note(instance, static_cast<uint8_t>(channel));
        
        /* Clear tracking */
        midiNoteToChannel[midiNote] = -1;
        releaseMidiChannel(channel);
    }
}

