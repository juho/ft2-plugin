#include "PluginEditor.h"

#include <cstdio>
#include <cstddef>
#if defined(_WIN32)
#pragma pack(push, 8)
#endif
extern "C" {
#include "ft2_plugin_diskop.h"
#include "ft2_plugin_loader.h"
#include "ft2_plugin_timemap.h"
#include "ft2_plugin_gui.h"
#include "ft2_plugin_sample_ed.h"
#include "ft2_plugin_instr_ed.h"
#include "ft2_plugin_pattern_ed.h"
}
#if defined(_WIN32)
#pragma pack(pop)
#endif

// Use JUCE's OpenGL namespace
using namespace ::juce::gl;

/* JUCE-to-FT2 key code mapping table for special keys.
   ASCII keys (0-127) pass through unchanged; these are the remapped keys
   that need explicit translation for both press and release detection. */
struct KeyMapping { int juceCode; int ft2Code; };
static const KeyMapping kKeyMap[] = {
    /* Navigation keys */
    { juce::KeyPress::leftKey,      FT2_KEY_LEFT },
    { juce::KeyPress::rightKey,     FT2_KEY_RIGHT },
    { juce::KeyPress::upKey,        FT2_KEY_UP },
    { juce::KeyPress::downKey,      FT2_KEY_DOWN },
    { juce::KeyPress::pageUpKey,    FT2_KEY_PAGEUP },
    { juce::KeyPress::pageDownKey,  FT2_KEY_PAGEDOWN },
    { juce::KeyPress::homeKey,      FT2_KEY_HOME },
    { juce::KeyPress::endKey,       FT2_KEY_END },
    { juce::KeyPress::insertKey,    FT2_KEY_INSERT },
    /* F-keys */
    { juce::KeyPress::F1Key,        FT2_KEY_F1 },
    { juce::KeyPress::F2Key,        FT2_KEY_F2 },
    { juce::KeyPress::F3Key,        FT2_KEY_F3 },
    { juce::KeyPress::F4Key,        FT2_KEY_F4 },
    { juce::KeyPress::F5Key,        FT2_KEY_F5 },
    { juce::KeyPress::F6Key,        FT2_KEY_F6 },
    { juce::KeyPress::F7Key,        FT2_KEY_F7 },
    { juce::KeyPress::F8Key,        FT2_KEY_F8 },
    { juce::KeyPress::F9Key,        FT2_KEY_F9 },
    { juce::KeyPress::F10Key,       FT2_KEY_F10 },
    { juce::KeyPress::F11Key,       FT2_KEY_F11 },
    { juce::KeyPress::F12Key,       FT2_KEY_F12 },
    /* Numpad keys */
    { juce::KeyPress::numberPad0,           FT2_KEY_NUMPAD0 },
    { juce::KeyPress::numberPad1,           FT2_KEY_NUMPAD1 },
    { juce::KeyPress::numberPad2,           FT2_KEY_NUMPAD2 },
    { juce::KeyPress::numberPad3,           FT2_KEY_NUMPAD3 },
    { juce::KeyPress::numberPad4,           FT2_KEY_NUMPAD4 },
    { juce::KeyPress::numberPad5,           FT2_KEY_NUMPAD5 },
    { juce::KeyPress::numberPad6,           FT2_KEY_NUMPAD6 },
    { juce::KeyPress::numberPad7,           FT2_KEY_NUMPAD7 },
    { juce::KeyPress::numberPad8,           FT2_KEY_NUMPAD8 },
    { juce::KeyPress::numberPad9,           FT2_KEY_NUMPAD9 },
    { juce::KeyPress::numberPadAdd,         FT2_KEY_NUMPAD_PLUS },
    { juce::KeyPress::numberPadSubtract,    FT2_KEY_NUMPAD_MINUS },
    { juce::KeyPress::numberPadMultiply,    FT2_KEY_NUMPAD_MULTIPLY },
    { juce::KeyPress::numberPadDivide,      FT2_KEY_NUMPAD_DIVIDE },
    { juce::KeyPress::numberPadDecimalPoint, FT2_KEY_NUMPAD_PERIOD },
    { juce::KeyPress::numberPadDelete,      FT2_KEY_NUMLOCK },
    { 0xF739,                               FT2_KEY_NUMLOCK }, /* Mac Clear key */
    { 0, 0 } /* sentinel */
};

/* Lookup FT2 key code from JUCE key code. Returns 0 if not in table. */
static int juceToFt2Key(int juceCode)
{
    for (const KeyMapping* m = kKeyMap; m->juceCode != 0; ++m)
        if (m->juceCode == juceCode)
            return m->ft2Code;
    return 0;
}

//==============================================================================
FT2PluginEditor::FT2PluginEditor (FT2PluginProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Create UI system (allocated in C for correct memory layout)
    ui = ft2_ui_create();
    
    // Link the UI to the instance for multi-instance support
    auto* inst = audioProcessor.getInstance();
    if (inst != nullptr)
    {
        inst->ui = ui;
        
        // Request full redraw on first frame - the render loop will handle
        // showing the correct widgets based on persisted uiState flags.
        // We don't call show* functions here because they draw to framebuffer,
        // which is unsafe during construction on Windows.
        inst->uiState.needsFullRedraw = true;
    }
    
    // Set the window size (2x upscale by default)
    setSize (FT2_SCREEN_W * upscaleFactor, FT2_SCREEN_H * upscaleFactor);
    
    // Make the component opaque - important for OpenGL
    setOpaque(true);
    
    // Enable OpenGL rendering
    openGLContext.setRenderer(this);
    openGLContext.setContinuousRepainting(false);  // Disable continuous rendering - we trigger repaints manually
    openGLContext.attachTo(*this);
    
    // Set resizable with aspect ratio constraint
    setResizable(true, true);
    setResizeLimits(FT2_SCREEN_W, FT2_SCREEN_H, 
                    FT2_SCREEN_W * 4, FT2_SCREEN_H * 4);
    getConstrainer()->setFixedAspectRatio((double)FT2_SCREEN_W / (double)FT2_SCREEN_H);
    
    // Enable keyboard focus
    setWantsKeyboardFocus(true);
    
    // Start timer for UI updates at ~60fps
    startTimerHz(60);
    
    // Start async update check
    checkForUpdates();
}

FT2PluginEditor::~FT2PluginEditor()
{
    // Clear the UI link before destroying
    auto* inst = audioProcessor.getInstance();
    if (inst != nullptr)
        inst->ui = nullptr;
    
    // Must detach before destroying
    openGLContext.detach();
    stopTimer();
    ft2_ui_destroy(ui);
    ui = nullptr;
}

//==============================================================================
void FT2PluginEditor::paint (juce::Graphics& g)
{
    // OpenGL handles all rendering, but we need to draw something
    // in case OpenGL context hasn't been created yet
    if (!textureInitialized)
    {
        g.fillAll(juce::Colours::black);
        g.setColour(juce::Colours::white);
        g.drawText("Initializing OpenGL...", getLocalBounds(), juce::Justification::centred);
    }
    // When OpenGL is active, this method isn't called because we set opaque
}

void FT2PluginEditor::resized()
{
    // Update upscale factor based on window size
    upscaleFactor = juce::jmax(1, getWidth() / FT2_SCREEN_W);
}

//==============================================================================
void FT2PluginEditor::newOpenGLContextCreated()
{
    // Generate texture for framebuffer
    glGenTextures(1, &framebufferTexture);
    glBindTexture(GL_TEXTURE_2D, framebufferTexture);
    
    // Set texture parameters for pixel-perfect rendering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Allocate texture storage
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, FT2_SCREEN_W, FT2_SCREEN_H, 
                 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
    
    textureInitialized = true;
}

void FT2PluginEditor::renderOpenGL()
{
    if (!textureInitialized)
        return;
    
    // Get framebuffer pointer
    const uint32_t* framebuffer = ft2_ui_get_framebuffer(ui);
    if (framebuffer == nullptr)
        return;
    
    // Get the render scale for high-DPI displays
    const float renderScale = (float)openGLContext.getRenderingScale();
    const int width = juce::roundToInt(renderScale * (float)getWidth());
    const int height = juce::roundToInt(renderScale * (float)getHeight());
    
    // Set viewport
    glViewport(0, 0, width, height);
    
    // Clear to black
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Disable blending - our framebuffer uses alpha channel for palette index storage,
    // not actual transparency, so we need to render it opaque
    glDisable(GL_BLEND);
    
    // Update texture with framebuffer contents
    glBindTexture(GL_TEXTURE_2D, framebufferTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, FT2_SCREEN_W, FT2_SCREEN_H,
                    GL_BGRA, GL_UNSIGNED_BYTE, framebuffer);
    
    // Set up orthographic projection (using deprecated fixed-function pipeline for compatibility)
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, 1.0, 1.0, 0.0, -1.0, 1.0);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // Enable texturing
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, framebufferTexture);
    
    // Set texture color to white so we see the actual texture
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    
    // Draw textured quad
    glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f, 0.0f);
        glTexCoord2f(1.0f, 0.0f); glVertex2f(1.0f, 0.0f);
        glTexCoord2f(1.0f, 1.0f); glVertex2f(1.0f, 1.0f);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(0.0f, 1.0f);
    glEnd();
    
    glDisable(GL_TEXTURE_2D);
}

void FT2PluginEditor::openGLContextClosing()
{
    if (textureInitialized)
    {
        glDeleteTextures(1, &framebufferTexture);
        framebufferTexture = 0;
        textureInitialized = false;
    }
}

//==============================================================================
void FT2PluginEditor::processDiskOpRequests()
{
    ft2_instance_t* inst = audioProcessor.getInstance();
    if (inst == nullptr)
        return;

    auto& diskop = inst->diskop;

    // Handle drop load request FIRST (works regardless of disk op screen visibility)
    if (diskop.requestDropLoad)
    {
        diskop.requestDropLoad = false;
        juce::File file(diskop.pendingDropPath);
        if (file.exists())
        {
            juce::MemoryBlock fileData;
            if (file.loadFileAsData(fileData))
            {
                const uint8_t* data = static_cast<const uint8_t*>(fileData.getData());
                uint32_t dataSize = static_cast<uint32_t>(fileData.getSize());
                if (ft2_load_module(inst, data, dataSize))
                {
                    inst->replayer.song.isModified = false;
                    
                    // Exit extended mode first if active (restores widget positions)
                    if (inst->uiState.extendedPatternEditor)
                        exitPatternEditorExtended(inst);
                    
                    // Reset UI state - close all overlays and return to pattern editor
                    hideTopScreen(inst);
                    hideAllTopLeftPanelOverlays(inst);
                    hideSampleEditor(inst);
                    hideInstEditor(inst);
                    inst->uiState.aboutScreenShown = false;
                    inst->uiState.nibblesShown = false;
                    inst->uiState.patternEditorShown = true;
                    inst->uiState.scopesShown = true;
                    inst->uiState.instrSwitcherShown = true;
                    
                    inst->uiState.updatePosSections = true;
                    inst->uiState.updatePatternEditor = true;
                    inst->uiState.updateInstrSwitcher = true;
                    inst->uiState.needsFullRedraw = true;
                    ft2_timemap_invalidate(inst);  // Invalidate - will rebuild with DAW BPM on next lookup
                }
            }
        }
        diskop.pendingDropPath[0] = '\0';
    }

    // Rest of disk op handling requires screen to be shown
    if (!inst->uiState.diskOpShown)
        return;

#ifdef _WIN32
    // Enumerate drives when requested (Windows only)
    if (diskop.requestEnumerateDrives)
    {
        diskop.requestEnumerateDrives = false;
        juce::Array<juce::File> roots;
        juce::File::findFileSystemRoots(roots);
        
        diskop.numDrives = juce::jmin((int)roots.size(), (int)FT2_DISKOP_MAX_DRIVES);
        for (int i = 0; i < diskop.numDrives; i++)
        {
            juce::String driveName = roots[i].getFullPathName();
            strncpy(diskop.driveNames[i], driveName.toRawUTF8(), 3);
            diskop.driveNames[i][3] = '\0';
        }
        inst->uiState.needsFullRedraw = true;
    }

    // Handle drive navigation request (Windows only)
    if (diskop.requestDriveIndex >= 0 && diskop.requestDriveIndex < diskop.numDrives)
    {
        int idx = diskop.requestDriveIndex;
        diskop.requestDriveIndex = -1;
        
        // Navigate to drive root
        strncpy(diskop.currentPath, diskop.driveNames[idx], FT2_PATH_MAX - 1);
        diskop.currentPath[FT2_PATH_MAX - 1] = '\0';
        diskop.requestReadDir = true;
    }
#endif

    // Handle navigation requests
    if (diskop.requestGoHome)
    {
        diskop.requestGoHome = false;
        juce::File homeDir = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
        strncpy(diskop.currentPath, homeDir.getFullPathName().toRawUTF8(), FT2_PATH_MAX - 1);
        diskop.currentPath[FT2_PATH_MAX - 1] = '\0';
        diskop.requestReadDir = true;
    }

    if (diskop.requestGoRoot)
    {
        diskop.requestGoRoot = false;
        // Navigate to root of current path (works on all platforms)
        juce::File current(diskop.currentPath);
        while (!current.isRoot())
            current = current.getParentDirectory();
        strncpy(diskop.currentPath, current.getFullPathName().toRawUTF8(), FT2_PATH_MAX - 1);
        diskop.currentPath[FT2_PATH_MAX - 1] = '\0';
        diskop.requestReadDir = true;
    }

    if (diskop.requestGoParent)
    {
        diskop.requestGoParent = false;
        juce::File current(diskop.currentPath);
        juce::File parent = current.getParentDirectory();
        if (parent.exists())
        {
            strncpy(diskop.currentPath, parent.getFullPathName().toRawUTF8(), FT2_PATH_MAX - 1);
            diskop.currentPath[FT2_PATH_MAX - 1] = '\0';
            diskop.requestReadDir = true;
        }
    }

    if (diskop.requestOpenEntry >= 0)
    {
        int32_t idx = diskop.requestOpenEntry;
        diskop.requestOpenEntry = -1;

        if (diskop.entries != nullptr && idx < diskop.fileCount && diskop.entries[idx].isDir)
        {
            juce::File current(diskop.currentPath);

            // Handle ".." entry - navigate to parent
            if (strcmp(diskop.entries[idx].name, "..") == 0)
            {
                juce::File parent = current.getParentDirectory();
                if (parent.exists())
                {
                    strncpy(diskop.currentPath, parent.getFullPathName().toRawUTF8(), FT2_PATH_MAX - 1);
                    diskop.currentPath[FT2_PATH_MAX - 1] = '\0';
                    diskop.requestReadDir = true;
                }
            }
            else
            {
            juce::File child = current.getChildFile(diskop.entries[idx].name);
            if (child.exists() && child.isDirectory())
            {
                strncpy(diskop.currentPath, child.getFullPathName().toRawUTF8(), FT2_PATH_MAX - 1);
                diskop.currentPath[FT2_PATH_MAX - 1] = '\0';
                diskop.requestReadDir = true;
                }
            }
        }
    }

    // Handle directory read request
    if (diskop.requestReadDir)
    {
        diskop.requestReadDir = false;
        readDiskOpDirectory();
    }

    // Handle file load request
    if (diskop.requestLoadEntry >= 0)
    {
        int32_t idx = diskop.requestLoadEntry;
        diskop.requestLoadEntry = -1;

        if (diskop.entries != nullptr && idx < diskop.fileCount && !diskop.entries[idx].isDir)
        {
            juce::File current(diskop.currentPath);
            juce::File file = current.getChildFile(diskop.entries[idx].name);
            if (file.exists())
            {
                loadDiskOpFile(file);
            }
        }
    }

    // Handle save request
    if (diskop.requestSave)
    {
        diskop.requestSave = false;
        saveDiskOpFile();
    }

    // Handle delete request
    if (diskop.requestDelete)
    {
        diskop.requestDelete = false;
        deleteDiskOpFile();
    }

    // Handle rename request
    if (diskop.requestRename)
    {
        diskop.requestRename = false;
        renameDiskOpFile();
    }

    // Handle make directory request
    if (diskop.requestMakeDir)
    {
        diskop.requestMakeDir = false;
        makeDiskOpDirectory();
    }

    // Handle set path request (with validation)
    if (diskop.requestSetPath)
    {
        diskop.requestSetPath = false;
        
        juce::File newPath(diskop.newPath);
        if (newPath.exists() && newPath.isDirectory())
        {
            // Path is valid - update current path and read directory
            strncpy(diskop.currentPath, diskop.newPath, FT2_PATH_MAX - 1);
            diskop.currentPath[FT2_PATH_MAX - 1] = '\0';
            diskop.requestReadDir = true;
        }
        else
        {
            // Path doesn't exist - set error flag for C side to show dialog
            diskop.pathSetFailed = true;
        }
    }
}

void FT2PluginEditor::deleteDiskOpFile()
{
    ft2_instance_t* instPtr = audioProcessor.getInstance();
    if (instPtr == nullptr)
        return;

    auto& diskop = instPtr->diskop;
    int32_t idx = diskop.selectedEntry + diskop.dirPos;

    if (diskop.entries == nullptr || idx < 0 || idx >= diskop.fileCount)
        return;

    juce::File current(diskop.currentPath);
    juce::File file = current.getChildFile(diskop.entries[idx].name);

    if (file.exists())
    {
        juce::AlertWindow::showOkCancelBox(
            juce::AlertWindow::WarningIcon,
            "Delete",
            "Delete " + file.getFileName() + "?",
            "Delete", "Cancel",
            nullptr,
            juce::ModalCallbackFunction::create([this, file](int result) {
                if (result == 1)
                {
                    if (file.isDirectory())
                        file.deleteRecursively();
                    else
                        file.deleteFile();

                    ft2_instance_t* inst = audioProcessor.getInstance();
                    if (inst)
                    {
                        inst->diskop.requestReadDir = true;
                        inst->diskop.selectedEntry = -1;
                    }
                }
            })
        );
    }
}

void FT2PluginEditor::renameDiskOpFile()
{
    ft2_instance_t* instPtr = audioProcessor.getInstance();
    if (instPtr == nullptr)
        return;

    auto& diskop = instPtr->diskop;
    int32_t idx = diskop.selectedEntry + diskop.dirPos;

    if (diskop.entries == nullptr || idx < 0 || idx >= diskop.fileCount)
        return;

    juce::File current(diskop.currentPath);
    juce::File file = current.getChildFile(diskop.entries[idx].name);

    if (file.exists())
    {
        juce::AlertWindow* aw = new juce::AlertWindow(
            "Rename",
            "Enter new name:",
            juce::AlertWindow::QuestionIcon
        );
        aw->addTextEditor("newName", file.getFileName());
        aw->addButton("OK", 1);
        aw->addButton("Cancel", 0);

        aw->enterModalState(true, juce::ModalCallbackFunction::create(
            [this, file, aw](int result) {
                if (result == 1)
                {
                    juce::String newName = aw->getTextEditor("newName")->getText();
                    if (newName.isNotEmpty() && newName != file.getFileName())
                    {
                        juce::File newFile = file.getParentDirectory().getChildFile(newName);
                        file.moveFileTo(newFile);

                        ft2_instance_t* inst = audioProcessor.getInstance();
                        if (inst)
                            inst->diskop.requestReadDir = true;
                    }
                }
                delete aw;
            }
        ), true);
    }
}

void FT2PluginEditor::makeDiskOpDirectory()
{
    ft2_instance_t* inst = audioProcessor.getInstance();
    if (inst == nullptr)
        return;

    auto& diskop = inst->diskop;

    // Use directory name from FT2 dialog
    juce::String dirName(diskop.newDirName);
    if (dirName.isEmpty())
        return;

    juce::File current(diskop.currentPath);
                        juce::File newDir = current.getChildFile(dirName);

    // Check if directory already exists or if creation fails
    if (newDir.exists())
    {
        // Already exists - set error flag
        diskop.makeDirFailed = true;
    }
    else
    {
        // Try to create the directory
        if (!newDir.createDirectory())
        {
            // Creation failed (access denied, etc.)
            diskop.makeDirFailed = true;
        }
        else
        {
            // Success - refresh directory listing
            diskop.requestReadDir = true;
                    }
                }

    // Clear the name field
    diskop.newDirName[0] = '\0';
}

void FT2PluginEditor::saveDiskOpFile()
{
    ft2_instance_t* inst = audioProcessor.getInstance();
    if (inst == nullptr)
        return;

    auto& diskop = inst->diskop;

    // Get filename from textbox, add extension if needed
    juce::String filename(diskop.filename);
    if (filename.isEmpty())
    {
        filename = "untitled";
    }

    // Add extension based on save format
    juce::String ext;
    switch (diskop.itemType)
    {
        case FT2_DISKOP_ITEM_MODULE:
            switch (diskop.saveFormat[FT2_DISKOP_ITEM_MODULE])
            {
                case FT2_MOD_SAVE_MOD: ext = ".mod"; break;
                case FT2_MOD_SAVE_XM:  ext = ".xm";  break;
                case FT2_MOD_SAVE_WAV: ext = ".wav"; break;
            }
            break;
        case FT2_DISKOP_ITEM_INSTR:
            ext = ".xi";
            break;
        case FT2_DISKOP_ITEM_SAMPLE:
            switch (diskop.saveFormat[FT2_DISKOP_ITEM_SAMPLE])
            {
                case FT2_SMP_SAVE_RAW: ext = ".raw"; break;
                case FT2_SMP_SAVE_IFF: ext = ".iff"; break;
                case FT2_SMP_SAVE_WAV: ext = ".wav"; break;
            }
            break;
        case FT2_DISKOP_ITEM_PATTERN:
            ext = ".xp";
            break;
        case FT2_DISKOP_ITEM_TRACK:
            ext = ".xt";
            break;
    }

    // Add extension if not present
    if (!filename.endsWithIgnoreCase(ext))
        filename += ext;

    juce::File destDir(diskop.currentPath);
    juce::File destFile = destDir.getChildFile(filename);

    // Check for overwrite warning (unless already confirmed)
    if (inst->config.overwriteWarning && destFile.exists() && !diskop.requestSaveConfirmed)
    {
        juce::AlertWindow::showAsync(
            juce::MessageBoxOptions()
                .withIconType(juce::MessageBoxIconType::WarningIcon)
                .withTitle("File Overwrite")
                .withMessage("File \"" + filename + "\" already exists.\nDo you want to overwrite it?")
                .withButton("Yes")
                .withButton("No"),
            [inst](int result) {
                if (result == 1) // Yes
                {
                    inst->diskop.requestSaveConfirmed = true;
                    inst->diskop.requestSave = true;
                }
            });
        return;
    }
    diskop.requestSaveConfirmed = false; // Reset for next save

    uint8_t* data = nullptr;
    uint32_t dataSize = 0;
    bool success = false;

    switch (diskop.itemType)
    {
        case FT2_DISKOP_ITEM_MODULE:
            success = ft2_save_module(inst, &data, &dataSize);
            break;
        case FT2_DISKOP_ITEM_INSTR:
            success = ft2_save_instrument(inst, inst->editor.curInstr, &data, &dataSize);
            break;
        case FT2_DISKOP_ITEM_SAMPLE:
            success = ft2_save_sample(inst, inst->editor.curInstr, inst->editor.curSmp, &data, &dataSize);
            break;
        case FT2_DISKOP_ITEM_PATTERN:
            success = ft2_save_pattern(inst, (int16_t)inst->editor.editPattern, &data, &dataSize);
            break;
        case FT2_DISKOP_ITEM_TRACK:
            success = ft2_save_pattern(inst, (int16_t)inst->editor.editPattern, &data, &dataSize);
            break;
    }

    if (success && data != nullptr && dataSize > 0)
    {
        bool isModuleSave = (diskop.itemType == FT2_DISKOP_ITEM_MODULE);
        
        // Try to write to FT2-style path first
        if (destDir.hasWriteAccess() && destFile.create())
        {
            destFile.replaceWithData(data, dataSize);
            if (isModuleSave)
                inst->replayer.song.isModified = false;
        }
        else
        {
            // Fall back to native save dialog (async)
            auto chooser = std::make_shared<juce::FileChooser>("Save As", destFile, "*" + ext);
            chooser->launchAsync(juce::FileBrowserComponent::saveMode |
                                 juce::FileBrowserComponent::canSelectFiles,
                                 [data, dataSize, &diskop, inst, isModuleSave](const juce::FileChooser& fc) mutable
            {
                auto result = fc.getResult();
                if (result.getFullPathName().isNotEmpty())
                {
                    result.replaceWithData(data, dataSize);
                    if (isModuleSave)
                        inst->replayer.song.isModified = false;
                }
                free(data);
                diskop.requestReadDir = true;
            });
            return; // Data will be freed in async callback
        }

        free(data);
        diskop.requestReadDir = true;
    }
    else if (data != nullptr)
    {
        free(data);
    }
}

void FT2PluginEditor::loadDiskOpFile(const juce::File& file)
{
    ft2_instance_t* inst = audioProcessor.getInstance();
    if (inst == nullptr)
        return;

    juce::MemoryBlock fileData;
    if (!file.loadFileAsData(fileData))
        return;

    const uint8_t* data = static_cast<const uint8_t*>(fileData.getData());
    uint32_t dataSize = static_cast<uint32_t>(fileData.getSize());

    bool success = false;

    switch (inst->diskop.itemType)
    {
        case FT2_DISKOP_ITEM_MODULE:
            success = ft2_load_module(inst, data, dataSize);
            if (success)
            {
                inst->replayer.song.isModified = false;
                
                // Exit extended mode first if active (restores widget positions)
                if (inst->uiState.extendedPatternEditor)
                    exitPatternEditorExtended(inst);
                
                // Reset UI state - close disk op and all overlays, return to pattern editor
                hideDiskOpScreen(inst);
                hideAllTopLeftPanelOverlays(inst);
                hideSampleEditor(inst);
                hideInstEditor(inst);
                inst->uiState.aboutScreenShown = false;
                inst->uiState.nibblesShown = false;
                inst->uiState.configScreenShown = false;
                inst->uiState.helpScreenShown = false;
                inst->uiState.patternEditorShown = true;
                inst->uiState.scopesShown = true;
                inst->uiState.instrSwitcherShown = true;
                
                inst->uiState.updatePosSections = true;
                inst->uiState.updatePatternEditor = true;
                inst->uiState.updateInstrSwitcher = true;
                inst->uiState.needsFullRedraw = true;
                ft2_timemap_invalidate(inst);  // Invalidate - will rebuild with DAW BPM on next lookup
            }
            break;

        case FT2_DISKOP_ITEM_INSTR:
            success = ft2_load_instrument(inst, inst->editor.curInstr, data, dataSize);
            if (success)
            {
                inst->uiState.updateInstEditor = true;
                inst->uiState.needsFullRedraw = true;
            }
            break;

        case FT2_DISKOP_ITEM_SAMPLE:
            success = ft2_load_sample(inst, inst->editor.curInstr, inst->editor.curSmp, data, dataSize);
            if (success)
            {
                ft2_set_sample_name_from_filename(inst, inst->editor.curInstr, inst->editor.curSmp,
                                                   file.getFileName().toRawUTF8());
                inst->uiState.updateSampleEditor = true;
                inst->uiState.needsFullRedraw = true;
            }
            break;

        case FT2_DISKOP_ITEM_PATTERN:
            success = ft2_load_pattern(inst, (int16_t)inst->editor.editPattern, data, dataSize);
            if (success)
            {
                inst->uiState.updatePatternEditor = true;
                inst->uiState.needsFullRedraw = true;
            }
            break;

        case FT2_DISKOP_ITEM_TRACK:
            /* Track loading - same as pattern but different data format */
            success = ft2_load_pattern(inst, (int16_t)inst->editor.editPattern, data, dataSize);
            if (success)
            {
                inst->uiState.updatePatternEditor = true;
                inst->uiState.needsFullRedraw = true;
            }
            break;
    }
}

void FT2PluginEditor::readDiskOpDirectory()
{
    ft2_instance_t* inst = audioProcessor.getInstance();
    if (inst == nullptr)
        return;

    auto& diskop = inst->diskop;
    juce::File currentDir(diskop.currentPath);

    // Free existing entries
    if (diskop.entries != nullptr)
    {
        free(diskop.entries);
        diskop.entries = nullptr;
    }
    diskop.fileCount = 0;
    diskop.dirPos = 0;
    diskop.selectedEntry = -1;

    if (!currentDir.exists() || !currentDir.isDirectory())
        return;

    // Get file list
    juce::Array<juce::File> files;
    currentDir.findChildFiles(files, juce::File::findFilesAndDirectories, false);

    // Filter based on item type and showAllFiles
    juce::Array<juce::File> filtered;
    for (auto& file : files)
    {
        if (file.getFileName().startsWith("."))
            continue; // Skip hidden files

        if (file.isDirectory())
        {
            filtered.add(file);
        }
        else if (diskop.showAllFiles)
        {
            filtered.add(file);
        }
        else
        {
            // Filter by extension based on item type
            juce::String ext = file.getFileExtension().toLowerCase();
            bool include = false;

            switch (diskop.itemType)
            {
                case FT2_DISKOP_ITEM_MODULE:
                    include = ext == ".xm" || ext == ".mod" || ext == ".s3m" || ext == ".it";
                    break;
                case FT2_DISKOP_ITEM_INSTR:
                    include = ext == ".xi" || ext == ".pat";
                    break;
                case FT2_DISKOP_ITEM_SAMPLE:
                    include = ext == ".wav" || ext == ".aiff" || ext == ".aif" || ext == ".iff" ||
                              ext == ".raw" || ext == ".snd" || ext == ".au";
                    break;
                case FT2_DISKOP_ITEM_PATTERN:
                    include = ext == ".xp";
                    break;
                case FT2_DISKOP_ITEM_TRACK:
                    include = ext == ".xt";
                    break;
                default:
                    include = true;
                    break;
            }

            if (include)
                filtered.add(file);
        }
    }

    // Sort: directories first, then by extension or name based on config
    struct FileComparator
    {
        uint8_t sortPriority;  // 0 = extension first, 1 = name only
        
        int compareElements(juce::File a, juce::File b) const
        {
            bool aDir = a.isDirectory();
            bool bDir = b.isDirectory();
            if (aDir != bDir)
                return aDir ? -1 : 1;
            
            if (sortPriority == 0)
            {
                // Extension first, then name
                juce::String aExt = a.getFileExtension().toLowerCase();
                juce::String bExt = b.getFileExtension().toLowerCase();
                int extCmp = aExt.compareIgnoreCase(bExt);
                if (extCmp != 0)
                    return extCmp;
            }
            // Name only (or as secondary sort)
            return a.getFileName().compareIgnoreCase(b.getFileName());
        }
    };
    FileComparator comparator;
    comparator.sortPriority = inst->config.dirSortPriority;
    filtered.sort(comparator);

    // Check if we need a parent entry (not at root)
    bool hasParent = (currentDir.getParentDirectory() != currentDir);

    // Allocate entries (add 1 for ".." if not at root)
    int32_t count = filtered.size() + (hasParent ? 1 : 0);
    if (count > 0)
    {
        diskop.entries = (ft2_diskop_entry_t*)calloc((size_t)count, sizeof(ft2_diskop_entry_t));
        if (diskop.entries != nullptr)
        {
            diskop.fileCount = count;
            int32_t offset = 0;

            // Add parent directory entry if not at root
            if (hasParent)
            {
                ft2_diskop_entry_t& parentEntry = diskop.entries[0];
                strncpy(parentEntry.name, "..", sizeof(parentEntry.name) - 1);
                parentEntry.name[sizeof(parentEntry.name) - 1] = '\0';
                parentEntry.isDir = true;
                parentEntry.filesize = 0;
                offset = 1;
            }

            for (int32_t i = 0; i < filtered.size(); i++)
            {
                juce::File file = filtered[i];
                ft2_diskop_entry_t& entry = diskop.entries[i + offset];

                strncpy(entry.name, file.getFileName().toRawUTF8(), sizeof(entry.name) - 1);
                entry.name[sizeof(entry.name) - 1] = '\0';
                entry.isDir = file.isDirectory();
                entry.filesize = file.isDirectory() ? 0 : (int32_t)juce::jmin((juce::int64)INT32_MAX, file.getSize());
            }
        }
    }

    inst->uiState.needsFullRedraw = true;
}

void FT2PluginEditor::timerCallback()
{
    // Process disk op requests
    processDiskOpRequests();

    // Poll config action requests (reset/load/save global config)
    audioProcessor.pollConfigRequests();

    // Handle about screen requests
    ft2_instance_t* inst = audioProcessor.getInstance();
    if (inst != nullptr)
    {
        // Handle GitHub button request
        if (inst->uiState.requestOpenGitHub)
        {
            inst->uiState.requestOpenGitHub = false;
            juce::URL("https://github.com/juho/ft2-plugin").launchInDefaultBrowser();
        }
        
        // Handle update dialog request (when about screen is opened)
        if (inst->uiState.requestShowUpdateDialog)
        {
            inst->uiState.requestShowUpdateDialog = false;
            if (updateChecker.isCheckComplete() && updateChecker.isUpdateAvailable())
            {
                showUpdateDialog();
            }
        }
    }

    // Check if update dialog should be shown (once per release)
    if (!updateDialogShown && updateChecker.isCheckComplete())
    {
        if (updateChecker.shouldShowNotification(audioProcessor.getLastNotifiedVersion()))
        {
            showUpdateDialog();
        }
        updateDialogShown = true;  // Don't check again this session
    }

    // Update UI
    ft2_ui_update(ui, audioProcessor.getInstance());
    
    // Draw UI to the framebuffer
    ft2_ui_draw(ui, audioProcessor.getInstance());
    
    // Trigger OpenGL repaint after framebuffer is fully updated
    openGLContext.triggerRepaint();
}

//==============================================================================
juce::Point<int> FT2PluginEditor::screenToFT2(juce::Point<int> screenPos)
{
    const float scaleX = (float)FT2_SCREEN_W / (float)getWidth();
    const float scaleY = (float)FT2_SCREEN_H / (float)getHeight();
    
    return juce::Point<int>(
        juce::jlimit(0, FT2_SCREEN_W - 1, (int)(screenPos.x * scaleX)),
        juce::jlimit(0, FT2_SCREEN_H - 1, (int)(screenPos.y * scaleY))
    );
}

uint8_t FT2PluginEditor::getModifiers(const juce::ModifierKeys& mods)
{
    uint8_t ft2Mods = 0;
    if (mods.isShiftDown()) ft2Mods |= 1;   // Shift
    if (mods.isCtrlDown()) ft2Mods |= 2;    // Ctrl
    if (mods.isAltDown()) ft2Mods |= 4;     // Alt
    if (mods.isCommandDown()) ft2Mods |= 8; // Cmd
    return ft2Mods;
}

void FT2PluginEditor::mouseDown(const juce::MouseEvent& e)
{
    auto ft2Pos = screenToFT2(e.getPosition());
    bool leftDown = e.mods.isLeftButtonDown();
    bool rightDown = e.mods.isRightButtonDown();
    
    ft2_ui_mouse_press(ui, audioProcessor.getInstance(), ft2Pos.x, ft2Pos.y, leftDown, rightDown);
}

void FT2PluginEditor::mouseUp(const juce::MouseEvent& e)
{
    auto ft2Pos = screenToFT2(e.getPosition());
    
    /* In JUCE's mouseUp, e.mods reflects state BEFORE release, so the released button IS still "down" in mods.
     * This is opposite to mouseDown where the pressed button IS "down" in mods. */
    int button = 1; // Default to left
    if (e.mods.isLeftButtonDown()) button = 1;
    else if (e.mods.isRightButtonDown()) button = 2;
    else if (e.mods.isMiddleButtonDown()) button = 3;
    
    ft2_ui_mouse_release(ui, audioProcessor.getInstance(), ft2Pos.x, ft2Pos.y, button);
}

void FT2PluginEditor::mouseDrag(const juce::MouseEvent& e)
{
    auto ft2Pos = screenToFT2(e.getPosition());
    ft2_ui_mouse_move(ui, audioProcessor.getInstance(), ft2Pos.x, ft2Pos.y);
}

void FT2PluginEditor::mouseMove(const juce::MouseEvent& e)
{
    auto ft2Pos = screenToFT2(e.getPosition());
    ft2_ui_mouse_move(ui, audioProcessor.getInstance(), ft2Pos.x, ft2Pos.y);
}

void FT2PluginEditor::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    float delta_f = (wheel.deltaY != 0.0f) ? wheel.deltaY : wheel.deltaX;
    if (delta_f == 0.0f)
        return;
    
    auto ft2Pos = screenToFT2(e.getPosition());
    int delta = (delta_f > 0.0f) ? 1 : -1;
    int modifiers = getModifiers(juce::ModifierKeys::getCurrentModifiers());
    ft2_ui_mouse_wheel(ui, audioProcessor.getInstance(), ft2Pos.x, ft2Pos.y, delta, modifiers);
}

bool FT2PluginEditor::keyPressed(const juce::KeyPress& key)
{
    int32_t keyCode = key.getKeyCode();
    int modifiers = getModifiers(juce::ModifierKeys::getCurrentModifiers());
    
    /* Map JUCE key codes to FT2 key codes.
       Special keys use the mapping table; ASCII keys that have different
       JUCE constants (space, return, etc.) are handled explicitly;
       printable ASCII passes through unchanged. */
    int ft2Key = juceToFt2Key(keyCode);
    if (ft2Key == 0)
    {
        if (keyCode == juce::KeyPress::spaceKey)         ft2Key = FT2_KEY_SPACE;
        else if (keyCode == juce::KeyPress::returnKey)   ft2Key = FT2_KEY_RETURN;
        else if (keyCode == juce::KeyPress::escapeKey)   ft2Key = FT2_KEY_ESCAPE;
        else if (keyCode == juce::KeyPress::backspaceKey) ft2Key = FT2_KEY_BACKSPACE;
        else if (keyCode == juce::KeyPress::deleteKey)   ft2Key = FT2_KEY_DELETE;
        else if (keyCode == juce::KeyPress::tabKey)      ft2Key = FT2_KEY_TAB;
        else ft2Key = keyCode; /* Pass through for printable ASCII */
    }
    
    /* Check for text character input */
    juce::juce_wchar textChar = key.getTextCharacter();
    if (textChar >= 32 && textChar <= 126)
        ft2_ui_text_input(ui, audioProcessor.getInstance(), static_cast<char>(textChar));
    
    ft2_ui_key_press(ui, audioProcessor.getInstance(), ft2Key, modifiers);
    return true;
}

bool FT2PluginEditor::keyStateChanged(bool isKeyDown)
{
    if (!isKeyDown && ui != nullptr)
    {
        int modifiers = getModifiers(juce::ModifierKeys::getCurrentModifiers());
        ft2_instance_t* inst = audioProcessor.getInstance();
        
        /* Check ASCII key releases (FT2 codes match JUCE codes for 0-127) */
        for (int k = 0; k < 128; k++)
        {
            if (ui->input.keyDown[k] && !juce::KeyPress::isKeyCurrentlyDown(k))
                ft2_ui_key_release(ui, inst, k, modifiers);
        }
        
        /* Check remapped key releases using the mapping table */
        for (const KeyMapping* m = kKeyMap; m->juceCode != 0; ++m)
        {
            if (ui->input.keyDown[m->ft2Code] && !juce::KeyPress::isKeyCurrentlyDown(m->juceCode))
                ft2_ui_key_release(ui, inst, m->ft2Code, modifiers);
        }
    }
    return false;
}

bool FT2PluginEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    // Check if any of the files are XM, MOD, S3M, IT, WAV, etc.
    for (const auto& file : files)
    {
        juce::String ext = juce::File(file).getFileExtension().toLowerCase();
        if (ext == ".xm" || ext == ".mod" || ext == ".s3m" || ext == ".it" ||
            ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".iff" ||
            ext == ".pat" || ext == ".xi" || ext == ".flac")
        {
            return true;
        }
    }
    return false;
}

void FT2PluginEditor::filesDropped(const juce::StringArray& files, int x, int y)
{
    (void)x;
    (void)y;
    
    if (files.isEmpty())
        return;
    
    ft2_instance_t* inst = audioProcessor.getInstance();
    if (inst == nullptr)
        return;
    
    // Get the first file and try to load it
    juce::File file(files[0]);
    juce::String ext = file.getFileExtension().toLowerCase();
    
    // Module formats - check for unsaved changes first
    if (ext == ".xm" || ext == ".mod" || ext == ".s3m" || ext == ".it")
    {
        // Use the unsaved changes check mechanism
        ft2_diskop_request_drop_load(inst, file.getFullPathName().toRawUTF8());
        return;
    }
    
    // Read file into memory for non-module formats
    juce::MemoryBlock fileData;
    if (!file.loadFileAsData(fileData))
        return;
    
    const uint8_t* data = static_cast<const uint8_t*>(fileData.getData());
    uint32_t dataSize = static_cast<uint32_t>(fileData.getSize());
    
    // XI Instrument format
    if (ext == ".xi")
    {
        // Load instrument into current instrument slot
        int16_t instrNum = inst->editor.curInstr;
        
        if (ft2_load_instrument(inst, instrNum, data, dataSize))
        {
            // Instrument loaded successfully - update UI
            inst->uiState.updateInstrSwitcher = true;
            inst->uiState.updateSampleEditor = true;
        }
    }
    // Sample formats
    else if (ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".iff" || ext == ".flac")
    {
        // Load sample into current instrument/sample slot
        int16_t instrNum = inst->editor.curInstr;
        int16_t sampleNum = inst->editor.curSmp;
        
        if (ft2_load_sample(inst, instrNum, sampleNum, data, dataSize))
        {
            // Set sample name from filename
            ft2_set_sample_name_from_filename(inst, instrNum, sampleNum,
                                               file.getFileName().toRawUTF8());
            
            // Sample loaded successfully - update UI
            inst->uiState.updateSampleEditor = true;
            inst->uiState.updateInstrSwitcher = true;
        }
    }
}

//==============================================================================
// Update Checker

void FT2PluginEditor::checkForUpdates()
{
#ifdef FT2_PLUGIN_VERSION
    if (audioProcessor.isAutoUpdateCheckEnabled())
        updateChecker.checkForUpdates(FT2_PLUGIN_VERSION);
#endif
}

void FT2PluginEditor::showUpdateDialog()
{
    juce::String latestVersion = updateChecker.getLatestVersion();
    
#ifdef FT2_PLUGIN_VERSION
    juce::String currentVersion = FT2_PLUGIN_VERSION;
#else
    juce::String currentVersion = "unknown";
#endif

    juce::String message = "A newer version (v" + latestVersion + ") is available!\n\n"
                          "Your version: v" + currentVersion + "\n\n"
                          "Would you like to visit the releases page?";

    auto options = juce::MessageBoxOptions()
        .withIconType(juce::MessageBoxIconType::InfoIcon)
        .withTitle("Update Available")
        .withMessage(message)
        .withButton("Visit Releases")
        .withButton("Dismiss");

    juce::AlertWindow::showAsync(options, [this, latestVersion](int result)
    {
        if (result == 1)  // "Visit Releases" button
        {
            juce::URL(UpdateChecker::RELEASES_URL).launchInDefaultBrowser();
        }
        
        // Save that we notified about this version (whether they visit or dismiss)
        audioProcessor.setLastNotifiedVersion(latestVersion);
    });
}
