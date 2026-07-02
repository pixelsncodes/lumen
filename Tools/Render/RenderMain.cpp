// lumen_render — headless render harness. Phase 0: minimal console app that
// proves the JUCE + MSVC toolchain builds. Phase 1 adds WAV rendering.

#include <juce_core/juce_core.h>

#include <iostream>

int main (int argc, char* argv[])
{
    juce::ignoreUnused (argc, argv);
    std::cout << "lumen_render skeleton (" << juce::SystemStats::getJUCEVersion() << ")\n";
    return 0;
}
