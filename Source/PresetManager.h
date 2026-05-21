#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// ---------------------------------------------------------------------------
// PresetManager — lightweight .oracle file save/load for APVTS state.
// Lives in the processor (owns the apvts reference).
// ---------------------------------------------------------------------------
class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& a) : apvts (a) {}

    juce::File getPresetsDirectory() const
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                          .getChildFile ("OraclePadPresets");
    }

    juce::Array<juce::File> getPresetFiles() const
    {
        juce::Array<juce::File> files;
        getPresetsDirectory().findChildFiles (files, juce::File::findFiles, false, "*.oracle");
        files.sort();
        return files;
    }

    bool savePreset (const juce::String& name)
    {
        auto dir = getPresetsDirectory();
        if (! dir.createDirectory().wasOk()) return false;
        auto file = dir.getChildFile (name.trim() + ".oracle");
        auto state = apvts.copyState();
        auto xml   = state.createXml();
        if (! xml) return false;
        xml->setAttribute ("presetName", name.trim());
        if (! xml->writeTo (file)) return false;
        currentPresetName = name.trim();
        auto files = getPresetFiles();
        for (int i = 0; i < files.size(); ++i)
            if (files[i].getFileNameWithoutExtension() == currentPresetName)
                { currentPresetIndex = i; return true; }
        return true;
    }

    bool loadPreset (const juce::File& file)
    {
        auto xml = juce::XmlDocument::parse (file);
        if (! xml || ! xml->hasTagName (apvts.state.getType())) return false;
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
        currentPresetName = xml->getStringAttribute ("presetName",
                                file.getFileNameWithoutExtension());
        return true;
    }

    void cyclePreset (int delta)
    {
        auto files = getPresetFiles();
        if (files.isEmpty()) return;
        const int count = files.size();
        const int next = ((currentPresetIndex + delta) % count + count) % count;
        if (loadPreset (files[next]))
            currentPresetIndex = next;
    }

    juce::String currentPresetName  { "INIT PATCH" };
    int          currentPresetIndex { -1 };

private:
    juce::AudioProcessorValueTreeState& apvts;
};
