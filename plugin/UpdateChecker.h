#pragma once

#include <juce_core/juce_core.h>

/**
 * Asynchronous update checker that queries GitHub releases API.
 * 
 * Checks for newer plugin versions without blocking the UI.
 * Network failures are silently ignored (update check is optional).
 */
class UpdateChecker : private juce::Thread
{
public:
    UpdateChecker();
    ~UpdateChecker() override;
    
    /**
     * Start checking for updates asynchronously.
     * @param currentVersion Current plugin version (e.g., "1.0.16")
     */
    void checkForUpdates(const juce::String& currentVersion);
    
    /**
     * Check if the update check has completed.
     */
    bool isCheckComplete() const { return checkComplete.load(); }
    
    /**
     * Check if a newer version is available.
     * Only valid after isCheckComplete() returns true.
     */
    bool isUpdateAvailable() const { return updateAvailable.load(); }
    
    /**
     * Get the latest version string (e.g., "1.0.17").
     * Only valid after isCheckComplete() returns true.
     */
    juce::String getLatestVersion() const;
    
    /**
     * Check if we should show a notification for this version.
     * Returns true if update is available AND latestVersion != lastNotifiedVersion.
     */
    bool shouldShowNotification(const juce::String& lastNotifiedVersion) const;
    
    /** GitHub releases page URL */
    static constexpr const char* RELEASES_URL = "https://github.com/juho/ft2-plugin/releases";
    
private:
    void run() override;
    
    /**
     * Parse version string "v1.0.16" or "1.0.16" into components.
     * Returns true if valid three-part version.
     */
    static bool parseVersion(const juce::String& versionStr, int& major, int& minor, int& patch);
    
    /**
     * Compare two version strings.
     * Returns > 0 if v1 > v2, < 0 if v1 < v2, 0 if equal.
     */
    static int compareVersions(const juce::String& v1, const juce::String& v2);
    
    juce::String currentVersion;
    juce::String latestVersion;
    std::atomic<bool> updateAvailable { false };
    std::atomic<bool> checkComplete { false };
    
    juce::CriticalSection versionLock;
    
    static constexpr const char* API_URL = "https://api.github.com/repos/juho/ft2-plugin/releases/latest";
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UpdateChecker)
};

