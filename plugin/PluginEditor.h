#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_opengl/juce_opengl.h>
#include "PluginProcessor.h"

extern "C" {
#include "ft2_plugin_ui.h"
}

//==============================================================================
/**
 * OpenGL-based FT2 framebuffer renderer using the full UI system
 */
class FT2PluginEditor  : public juce::AudioProcessorEditor,
                          public juce::FileDragAndDropTarget,
                          private juce::OpenGLRenderer,
                          private juce::Timer
{
public:
    explicit FT2PluginEditor (FT2PluginProcessor&);
    ~FT2PluginEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    
    // OpenGL callbacks
    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;
    
    // Timer for refresh
    void timerCallback() override;
    
    // Mouse handling
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    
    // Keyboard handling
    bool keyPressed(const juce::KeyPress& key) override;
    bool keyStateChanged(bool isKeyDown) override;
    
    // Drag and drop handling
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

private:
    FT2PluginProcessor& audioProcessor;
    
    // OpenGL context
    juce::OpenGLContext openGLContext;
    
    // UI state
    ft2_ui_t ui;
    
    // OpenGL texture for framebuffer
    GLuint framebufferTexture = 0;
    bool textureInitialized = false;
    
    // Upscale factor
    int upscaleFactor = 2;
    
    // Convert screen coordinates to FT2 coordinates
    juce::Point<int> screenToFT2(juce::Point<int> screenPos);
    
    // Convert JUCE modifiers to FT2 modifiers
    uint8_t getModifiers(const juce::ModifierKeys& mods);

    // Disk op helpers
    void processDiskOpRequests();
    void readDiskOpDirectory();
    void loadDiskOpFile(const juce::File& file);
    void saveDiskOpFile();
    void deleteDiskOpFile();
    void renameDiskOpFile();
    void makeDiskOpDirectory();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FT2PluginEditor)
};
