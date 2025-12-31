#include "UpdateChecker.h"
#include <juce_core/juce_core.h>

UpdateChecker::UpdateChecker()
    : Thread("UpdateChecker")
{
}

UpdateChecker::~UpdateChecker()
{
    stopThread(2000);  // Wait up to 2 seconds for thread to finish
}

void UpdateChecker::checkForUpdates(const juce::String& version)
{
    {
        const juce::ScopedLock lock(versionLock);
        currentVersion = version;
    }
    
    // Start background thread
    startThread(juce::Thread::Priority::low);
}

juce::String UpdateChecker::getLatestVersion() const
{
    const juce::ScopedLock lock(versionLock);
    return latestVersion;
}

bool UpdateChecker::shouldShowNotification(const juce::String& lastNotifiedVersion) const
{
    if (!updateAvailable.load())
        return false;
    
    const juce::ScopedLock lock(versionLock);
    
    // Don't show if we already notified about this version
    if (latestVersion == lastNotifiedVersion)
        return false;
    
    return true;
}

bool UpdateChecker::parseVersion(const juce::String& versionStr, int& major, int& minor, int& patch)
{
    juce::String v = versionStr.trim();
    
    // Remove leading 'v' or 'V' if present
    if (v.startsWithIgnoreCase("v"))
        v = v.substring(1);
    
    // Split by '.'
    juce::StringArray parts;
    parts.addTokens(v, ".", "");
    
    if (parts.size() != 3)
        return false;
    
    // Parse each part
    major = parts[0].getIntValue();
    minor = parts[1].getIntValue();
    patch = parts[2].getIntValue();
    
    // Validate - must have valid numbers
    if (major == 0 && parts[0] != "0")
        return false;
    
    return true;
}

int UpdateChecker::compareVersions(const juce::String& v1, const juce::String& v2)
{
    int major1, minor1, patch1;
    int major2, minor2, patch2;
    
    if (!parseVersion(v1, major1, minor1, patch1))
        return 0;
    if (!parseVersion(v2, major2, minor2, patch2))
        return 0;
    
    if (major1 != major2)
        return major1 - major2;
    if (minor1 != minor2)
        return minor1 - minor2;
    return patch1 - patch2;
}

void UpdateChecker::run()
{
    // Create URL for GitHub API
    juce::URL url(API_URL);
    
    // Set up request options (chain calls since InputStreamOptions can't be reassigned)
    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
        .withConnectionTimeoutMs(5000)  // 5 second timeout
        .withExtraHeaders("User-Agent: FT2Plugin UpdateChecker");
    
    // Fetch the response
    std::unique_ptr<juce::InputStream> stream = url.createInputStream(options);
    
    if (stream == nullptr)
    {
        // Network error - silently fail
        checkComplete.store(true);
        return;
    }
    
    // Read response
    juce::String response = stream->readEntireStreamAsString();
    
    if (response.isEmpty())
    {
        checkComplete.store(true);
        return;
    }
    
    // Parse JSON to find tag_name
    // Simple parsing - look for "tag_name":"v1.0.17" pattern
    int tagIndex = response.indexOf("\"tag_name\"");
    if (tagIndex < 0)
    {
        checkComplete.store(true);
        return;
    }
    
    // Find the value after "tag_name":
    int colonIndex = response.indexOf(tagIndex, ":");
    if (colonIndex < 0)
    {
        checkComplete.store(true);
        return;
    }
    
    // Find the opening quote
    int openQuote = response.indexOf(colonIndex, "\"");
    if (openQuote < 0)
    {
        checkComplete.store(true);
        return;
    }
    
    // Find the closing quote
    int closeQuote = response.indexOf(openQuote + 1, "\"");
    if (closeQuote < 0)
    {
        checkComplete.store(true);
        return;
    }
    
    // Extract the version string
    juce::String tagName = response.substring(openQuote + 1, closeQuote);
    
    // Validate it's a three-part version
    int major, minor, patch;
    if (!parseVersion(tagName, major, minor, patch))
    {
        checkComplete.store(true);
        return;
    }
    
    // Store the latest version (without 'v' prefix for consistency)
    {
        const juce::ScopedLock lock(versionLock);
        latestVersion = juce::String(major) + "." + juce::String(minor) + "." + juce::String(patch);
    }
    
    // Compare with current version
    juce::String currentVer;
    {
        const juce::ScopedLock lock(versionLock);
        currentVer = currentVersion;
    }
    
    if (compareVersions(latestVersion, currentVer) > 0)
    {
        updateAvailable.store(true);
    }
    
    checkComplete.store(true);
}

