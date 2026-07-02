// Custom standalone app (JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1) adding the
// verification flags from SPEC section 18:
//
//   Lumen.exe --version
//   Lumen.exe --screenshot <file.png> [--view play|deep] [--preset <name>]
//
// --view/--preset are accepted now and become meaningful in Phase 5.

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

#include <cstdio>

extern juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

namespace
{
    void printToStdout (const juce::String& text)
    {
        std::fputs (text.toRawUTF8(), stdout);
        std::fflush (stdout);
    }
} // namespace

class LumenStandaloneApp final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "Lumen"; }
    const juce::String getApplicationVersion() override { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override          { return true; }
    void anotherInstanceStarted (const juce::String&) override {}

    void initialise (const juce::String&) override
    {
        const auto args = getCommandLineParameterArray();

        if (args.contains ("--version"))
        {
            printToStdout (juce::String ("Lumen ") + JucePlugin_VersionString
                           + " (" + juce::SystemStats::getJUCEVersion() + ")\n");
            quit();
            return;
        }

        const auto screenshotIndex = args.indexOf ("--screenshot");
        if (screenshotIndex >= 0)
        {
            if (screenshotIndex + 1 >= args.size() || args[screenshotIndex + 1].startsWith ("--"))
            {
                printToStdout ("Usage: Lumen.exe --screenshot <file.png> [--view play|deep] [--preset <name>]\n");
                setApplicationReturnValue (2);
                quit();
                return;
            }

            screenshotFile = juce::File::getCurrentWorkingDirectory()
                                 .getChildFile (args[screenshotIndex + 1]);

            screenshotProcessor.reset (::createPluginFilter());
            screenshotEditor.reset (screenshotProcessor->createEditorAndMakeActive());

            if (screenshotEditor == nullptr)
            {
                printToStdout ("Error: no editor available for screenshot\n");
                setApplicationReturnValue (1);
                quit();
                return;
            }

            screenshotEditor->setTopLeftPosition (0, 0);
            screenshotEditor->addToDesktop (juce::ComponentPeer::windowIsTemporary);
            screenshotEditor->setVisible (true);

            // Let first paints and timers run before snapshotting (SPEC section 18).
            juce::Timer::callAfterDelay (700, [this] { takeScreenshotAndQuit(); });
            return;
        }

        juce::PropertiesFile::Options options;
        options.applicationName     = "Lumen";
        options.filenameSuffix      = ".settings";
        options.folderName          = "Lumen";
        options.osxLibrarySubFolder = "Application Support";
        properties.setStorageParameters (options);

        window = std::make_unique<juce::StandaloneFilterWindow> (getApplicationName(),
                                                                 juce::Colour (0xff1c1c1f),
                                                                 properties.getUserSettings(),
                                                                 false);
        window->setVisible (true);
    }

    void shutdown() override
    {
        window = nullptr;
        releaseScreenshotObjects();
        properties.saveIfNeeded();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

private:
    void takeScreenshotAndQuit()
    {
        bool ok = false;

        if (screenshotEditor != nullptr)
        {
            const auto image = screenshotEditor->createComponentSnapshot (
                screenshotEditor->getLocalBounds(), true, 1.0f);

            if (image.isValid())
            {
                screenshotFile.deleteFile();
                juce::FileOutputStream stream (screenshotFile);
                ok = stream.openedOk() && juce::PNGImageFormat().writeImageToStream (image, stream);
            }
        }

        printToStdout (ok ? "Screenshot written: " + screenshotFile.getFullPathName() + "\n"
                          : "Screenshot FAILED: " + screenshotFile.getFullPathName() + "\n");
        setApplicationReturnValue (ok ? 0 : 1);
        quit();
    }

    void releaseScreenshotObjects()
    {
        if (screenshotEditor != nullptr && screenshotProcessor != nullptr)
            screenshotProcessor->editorBeingDeleted (screenshotEditor.get());

        screenshotEditor = nullptr;
        screenshotProcessor = nullptr;
    }

    juce::ApplicationProperties properties;
    std::unique_ptr<juce::StandaloneFilterWindow> window;
    std::unique_ptr<juce::AudioProcessor> screenshotProcessor;
    std::unique_ptr<juce::AudioProcessorEditor> screenshotEditor;
    juce::File screenshotFile;
};

#if JucePlugin_Build_Standalone
 START_JUCE_APPLICATION (LumenStandaloneApp)
#endif
