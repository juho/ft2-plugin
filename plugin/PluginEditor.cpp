#include "PluginEditor.h"

extern "C" {
#include "ft2_plugin_diskop.h"
#include "ft2_plugin_loader.h"
#include "ft2_plugin_timemap.h"
#include "ft2_plugin_gui.h"
#include "ft2_plugin_sample_ed.h"
#include "ft2_plugin_instr_ed.h"
#include "ft2_plugin_pattern_ed.h"
}

// Use JUCE's OpenGL namespace
using namespace ::juce::gl;

//==============================================================================
FT2PluginEditor::FT2PluginEditor (FT2PluginProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Initialize UI system
    ft2_ui_init(&ui);
    
    // Link the UI to the instance for multi-instance support
    auto* inst = audioProcessor.getInstance();
    if (inst != nullptr)
        inst->ui = &ui;
    
    // Set the window size (2x upscale by default)
    setSize (FT2_SCREEN_W * upscaleFactor, FT2_SCREEN_H * upscaleFactor);
    
    // Make the component opaque - important for OpenGL
    setOpaque(true);
    
    // Enable OpenGL rendering
    openGLContext.setRenderer(this);
    openGLContext.setContinuousRepainting(true);  // Enable continuous rendering
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
    ft2_ui_shutdown(&ui);
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
    const uint32_t* framebuffer = ft2_ui_get_framebuffer(&ui);
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
        strncpy(diskop.currentPath, "/", FT2_PATH_MAX - 1);
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
    ft2_instance_t* inst = audioProcessor.getInstance();
    if (inst == nullptr)
        return;

    auto& diskop = inst->diskop;
    int32_t idx = diskop.selectedEntry + diskop.dirPos;

    if (diskop.entries == nullptr || idx < 0 || idx >= diskop.fileCount)
        return;

    juce::File current(diskop.currentPath);
    juce::File file = current.getChildFile(diskop.entries[idx].name);

    if (file.exists())
    {
        // Confirm before delete
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
    ft2_instance_t* inst = audioProcessor.getInstance();
    if (inst == nullptr)
        return;

    auto& diskop = inst->diskop;
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

    // Sort: directories first, then by name
    struct FileComparator
    {
        int compareElements(juce::File a, juce::File b)
        {
            bool aDir = a.isDirectory();
            bool bDir = b.isDirectory();
            if (aDir != bDir)
                return aDir ? -1 : 1;
            return a.getFileName().compareIgnoreCase(b.getFileName());
        }
    };
    FileComparator comparator;
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

    // Update UI
    ft2_ui_update(&ui, audioProcessor.getInstance());
    
    // Draw UI to the framebuffer
    ft2_ui_draw(&ui, audioProcessor.getInstance());
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
    
    ft2_ui_mouse_press(&ui, audioProcessor.getInstance(), ft2Pos.x, ft2Pos.y, leftDown, rightDown);
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
    
    ft2_ui_mouse_release(&ui, audioProcessor.getInstance(), ft2Pos.x, ft2Pos.y, button);
}

void FT2PluginEditor::mouseDrag(const juce::MouseEvent& e)
{
    auto ft2Pos = screenToFT2(e.getPosition());
    ft2_ui_mouse_move(&ui, audioProcessor.getInstance(), ft2Pos.x, ft2Pos.y);
}

void FT2PluginEditor::mouseMove(const juce::MouseEvent& e)
{
    auto ft2Pos = screenToFT2(e.getPosition());
    ft2_ui_mouse_move(&ui, audioProcessor.getInstance(), ft2Pos.x, ft2Pos.y);
}

void FT2PluginEditor::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    auto ft2Pos = screenToFT2(e.getPosition());
    int delta = (int)(wheel.deltaY * 120.0f); // Convert to standard wheel units
    ft2_ui_mouse_wheel(&ui, audioProcessor.getInstance(), ft2Pos.x, ft2Pos.y, delta);
}

bool FT2PluginEditor::keyPressed(const juce::KeyPress& key)
{
    int32_t keyCode = key.getKeyCode();
    int modifiers = getModifiers(juce::ModifierKeys::getCurrentModifiers());
    
    /* Map JUCE key codes to FT2 key codes */
    int ft2Key = 0;
    if (keyCode == juce::KeyPress::spaceKey)        ft2Key = FT2_KEY_SPACE;
    else if (keyCode == juce::KeyPress::returnKey)  ft2Key = FT2_KEY_RETURN;
    else if (keyCode == juce::KeyPress::escapeKey)  ft2Key = FT2_KEY_ESCAPE;
    else if (keyCode == juce::KeyPress::backspaceKey) ft2Key = FT2_KEY_BACKSPACE;
    else if (keyCode == juce::KeyPress::deleteKey)  ft2Key = FT2_KEY_DELETE;
    else if (keyCode == juce::KeyPress::insertKey)  ft2Key = FT2_KEY_INSERT;
    else if (keyCode == juce::KeyPress::leftKey)    ft2Key = FT2_KEY_LEFT;
    else if (keyCode == juce::KeyPress::rightKey)   ft2Key = FT2_KEY_RIGHT;
    else if (keyCode == juce::KeyPress::upKey)      ft2Key = FT2_KEY_UP;
    else if (keyCode == juce::KeyPress::downKey)    ft2Key = FT2_KEY_DOWN;
    else if (keyCode == juce::KeyPress::pageUpKey)  ft2Key = FT2_KEY_PAGEUP;
    else if (keyCode == juce::KeyPress::pageDownKey) ft2Key = FT2_KEY_PAGEDOWN;
    else if (keyCode == juce::KeyPress::homeKey)    ft2Key = FT2_KEY_HOME;
    else if (keyCode == juce::KeyPress::endKey)     ft2Key = FT2_KEY_END;
    else if (keyCode == juce::KeyPress::tabKey)     ft2Key = FT2_KEY_TAB;
    else if (keyCode == juce::KeyPress::F1Key)      ft2Key = FT2_KEY_F1;
    else if (keyCode == juce::KeyPress::F2Key)      ft2Key = FT2_KEY_F2;
    else if (keyCode == juce::KeyPress::F3Key)      ft2Key = FT2_KEY_F3;
    else if (keyCode == juce::KeyPress::F4Key)      ft2Key = FT2_KEY_F4;
    else if (keyCode == juce::KeyPress::F5Key)      ft2Key = FT2_KEY_F5;
    else if (keyCode == juce::KeyPress::F6Key)      ft2Key = FT2_KEY_F6;
    else if (keyCode == juce::KeyPress::F7Key)      ft2Key = FT2_KEY_F7;
    else if (keyCode == juce::KeyPress::F8Key)      ft2Key = FT2_KEY_F8;
    else if (keyCode == juce::KeyPress::F9Key)      ft2Key = FT2_KEY_F9;
    else if (keyCode == juce::KeyPress::F10Key)     ft2Key = FT2_KEY_F10;
    else if (keyCode == juce::KeyPress::F11Key)     ft2Key = FT2_KEY_F11;
    else if (keyCode == juce::KeyPress::F12Key)     ft2Key = FT2_KEY_F12;
    else ft2Key = keyCode; /* Pass through for regular characters */
    
    /* Check for text character input */
    juce::juce_wchar textChar = key.getTextCharacter();
    if (textChar >= 32 && textChar <= 126)
    {
        ft2_ui_text_input(&ui, static_cast<char>(textChar));
    }
    
    ft2_ui_key_press(&ui, audioProcessor.getInstance(), ft2Key, modifiers);
    return true; // Mark as handled
}

bool FT2PluginEditor::keyStateChanged(bool isKeyDown)
{
    if (!isKeyDown)
    {
        // A key was released - find which one(s) by comparing tracked state vs actual
        for (int key = 0; key < 512; key++)
        {
            if (ui.input.keyDown[key] && !juce::KeyPress::isKeyCurrentlyDown(key))
            {
                int modifiers = getModifiers(juce::ModifierKeys::getCurrentModifiers());
                ft2_ui_key_release(&ui, audioProcessor.getInstance(), key, modifiers);
            }
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
