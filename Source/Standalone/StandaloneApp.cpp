// Custom standalone app (JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1) adding the
// verification flags from SPEC section 18:
//
//   Lumen.exe --version
//   Lumen.exe --screenshot <file.png> [--view play|deep] [--preset <name>]
//              [--notes 48,55,60]      (hold these notes via simulated incoming
//                                       MIDI pumped through processBlock, so the
//                                       screenshot shows the playing state)
//              [--lens-image <png>]    (load an image through the Lens engine
//                                       first, so the screenshot shows the Lens
//                                       panel with image + scanline — Phase 6)
//   Lumen.exe --check-params            (JSON: APVTS params not reachable in the UI)
//   Lumen.exe --stress <seconds> [--view play|deep]
//       Real audio device + 8-voice chord + random parameter wiggling at
//       60 Hz with the frame HUD on; prints JSON frame/dropout stats and
//       exits 0 only if frame avg <= 16.7 ms and dropouts == 0 (Phase 5 gate).
//
// --preset is accepted and becomes meaningful in Phase 7.

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

#include "Lens/LensController.h"
#include "PluginProcessor.h"
#include "State/PresetManager.h"
#include "UI/LumenLookAndFeel.h"
#include "UI/PluginEditor.h"

#include <cstdio>
#include <vector>

extern juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

namespace
{
    void printToStdout (const juce::String& text)
    {
        std::fputs (text.toRawUTF8(), stdout);
        std::fflush (stdout);
    }

    int requestedViewIndex (const juce::StringArray& args)
    {
        const auto viewIndex = args.indexOf ("--view");
        if (viewIndex >= 0 && viewIndex + 1 < args.size())
            return args[viewIndex + 1] == "deep" ? 1 : 0;
        return 0;
    }

    // Borderless standalone window: the app header IS the title bar. Dropping
    // the JUCE title bar to height 0 removes both the drawn title strip and the
    // StandaloneFilterWindow "Options" button; the header's gear/minimize/close
    // take over and dragging an empty header region moves the window.
    class LumenStandaloneWindow final : public juce::StandaloneFilterWindow
    {
    public:
        LumenStandaloneWindow (const juce::String& title, juce::Colour backgroundColour,
                               juce::PropertySet* settingsToUse, bool takeOwnershipOfSettings)
            : juce::StandaloneFilterWindow (title, backgroundColour,
                                            settingsToUse, takeOwnershipOfSettings)
        {
            setTitleBarHeight (0);
        }
    };
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

        if (args.contains ("--check-params"))
        {
            runParameterCheck();
            return;
        }

        const auto stressIndex = args.indexOf ("--stress");
        if (stressIndex >= 0)
        {
            const int seconds = stressIndex + 1 < args.size()
                                    ? juce::jlimit (1, 600, args[stressIndex + 1].getIntValue())
                                    : 60;
            startStress (seconds, requestedViewIndex (args));
            return;
        }

        const auto screenshotIndex = args.indexOf ("--screenshot");
        if (screenshotIndex >= 0)
        {
            if (screenshotIndex + 1 >= args.size() || args[screenshotIndex + 1].startsWith ("--"))
            {
                printToStdout ("Usage: Lumen.exe --screenshot <file.png> [--view play|deep] [--preset <name>] [--notes n,n,...]\n");
                setApplicationReturnValue (2);
                quit();
                return;
            }

            screenshotFile = juce::File::getCurrentWorkingDirectory()
                                 .getChildFile (args[screenshotIndex + 1]);

            // Software rendering only: identical paint path (SPEC section 2).
            LumenAudioProcessorEditor::disableOpenGL = true;

            // --chrome plugin|standalone forces the header style so both the
            // borderless standalone header and the frameless plugin header can
            // be captured from this one binary (default: auto = standalone).
            if (const auto chromeIndex = args.indexOf ("--chrome");
                chromeIndex >= 0 && chromeIndex + 1 < args.size())
                LumenAudioProcessorEditor::chromeOverride =
                    args[chromeIndex + 1] == "plugin" ? 0 : 1;

            // --menu preset|gear opens the branded header menu before the snap.
            if (const auto menuIndex = args.indexOf ("--menu");
                menuIndex >= 0 && menuIndex + 1 < args.size())
                menuMode = args[menuIndex + 1];

            // --menu-bg bright|dark composites the (per-pixel transparent)
            // menu image over a backdrop so the rounded corners are checkable
            // against both a bright desktop and a dark UI region.
            if (const auto bgIndex = args.indexOf ("--menu-bg");
                bgIndex >= 0 && bgIndex + 1 < args.size())
                menuBg = args[bgIndex + 1];

            // --tooltip <paramId> pins that knob's tooltip before the snap
            // (tooltips pass proof).
            if (const auto tipIndex = args.indexOf ("--tooltip");
                tipIndex >= 0 && tipIndex + 1 < args.size())
                tooltipParam = args[tipIndex + 1];

            harnessProcessor.reset (::createPluginFilter());

            // --lens-image: run the Lens engine before the editor opens so
            // the screenshot shows the loaded image + scanline (Phase 6).
            const auto lensIndex = args.indexOf ("--lens-image");
            if (lensIndex >= 0 && lensIndex + 1 < args.size())
                if (auto* lumenProcessor = dynamic_cast<LumenAudioProcessor*> (harnessProcessor.get()))
                {
                    const auto imageFile = juce::File::getCurrentWorkingDirectory()
                                               .getChildFile (args[lensIndex + 1]);
                    if (! lumenProcessor->lensController().loadImageFile (imageFile))
                        printToStdout ("Warning: --lens-image could not decode "
                                       + imageFile.getFullPathName() + "\n");
                }

            harnessEditor.reset (harnessProcessor->createEditorAndMakeActive());

            if (harnessEditor == nullptr)
            {
                printToStdout ("Error: no editor available for screenshot\n");
                setApplicationReturnValue (1);
                quit();
                return;
            }

            if (auto* editor = dynamic_cast<LumenAudioProcessorEditor*> (harnessEditor.get()))
                editor->setView (requestedViewIndex (args));

            harnessEditor->setTopLeftPosition (0, 0);
            harnessEditor->addToDesktop (juce::ComponentPeer::windowIsTemporary);
            harnessEditor->setVisible (true);

            // --notes: hold a chord via simulated incoming MIDI, pumped through
            // processBlock exactly like hardware/host MIDI (no audio device).
            const auto notesIndex = args.indexOf ("--notes");
            if (notesIndex >= 0 && notesIndex + 1 < args.size())
            {
                juce::StringArray tokens;
                tokens.addTokens (args[notesIndex + 1], ",", "");
                std::vector<int> notes;
                for (const auto& t : tokens)
                    if (const int n = t.getIntValue(); n >= 0 && n <= 127)
                        notes.push_back (n);

                if (! notes.empty())
                {
                    harnessProcessor->setPlayConfigDetails (0, 2, 48000.0, 512);
                    harnessProcessor->prepareToPlay (48000.0, 512);
                    midiPump = std::make_unique<MidiPumpTimer> (*harnessProcessor, std::move (notes));
                    midiPump->startTimerHz (60);
                }
            }

            // Let first paints and timers run before snapshotting (SPEC section
            // 18). --settle overrides the wait, e.g. to catch the Lens morph
            // journey mid-image with --notes held.
            int settleMs = 700;
            if (const auto settleIndex = args.indexOf ("--settle");
                settleIndex >= 0 && settleIndex + 1 < args.size())
                settleMs = juce::jlimit (100, 20000, args[settleIndex + 1].getIntValue());
            if (menuMode.isNotEmpty())
                juce::Timer::callAfterDelay (settleMs, [this] { captureMenuAndQuit(); });
            else
                juce::Timer::callAfterDelay (settleMs, [this] { takeScreenshotAndQuit(); });
            return;
        }

        juce::PropertiesFile::Options options;
        options.applicationName     = "Lumen";
        options.filenameSuffix      = ".settings";
        options.folderName          = "Lumen";
        options.osxLibrarySubFolder = "Application Support";
        properties.setStorageParameters (options);

        // The borderless header's gear opens the audio/MIDI settings dialog.
        // Resolved lazily so the shared UI never links standalone headers.
        LumenAudioProcessorEditor::standaloneSettingsHook = []
        {
            if (auto* holder = juce::StandalonePluginHolder::getInstance())
                holder->showAudioSettingsDialog();
        };

        window = std::make_unique<LumenStandaloneWindow> (getApplicationName(),
                                                          juce::Colour (0xff1c1c1f),
                                                          properties.getUserSettings(),
                                                          false);
        window->setVisible (true);
    }

    void shutdown() override
    {
        window = nullptr;
        stressTimer = nullptr;
        midiPump = nullptr;
        if (player != nullptr)
            deviceManager.removeAudioCallback (player.get());
        player = nullptr;
        releaseHarnessObjects();
        properties.saveIfNeeded();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

private:
    // --- --check-params ---------------------------------------------------
    void runParameterCheck()
    {
        LumenAudioProcessorEditor::disableOpenGL = true;
        harnessProcessor.reset (::createPluginFilter());
        harnessEditor.reset (harnessProcessor->createEditorAndMakeActive());

        auto* editor = dynamic_cast<LumenAudioProcessorEditor*> (harnessEditor.get());
        if (editor == nullptr)
        {
            printToStdout ("{\"error\":\"no editor\"}\n");
            setApplicationReturnValue (1);
            quit();
            return;
        }

        int total = 0;
        for (auto* parameter : harnessProcessor->getParameters())
            if (dynamic_cast<juce::RangedAudioParameter*> (parameter) != nullptr)
                ++total;

        const auto missing = editor->missingParameterIds();
        juce::String json = "{\"total_params\":" + juce::String (total)
                          + ",\"attached\":" + juce::String (total - missing.size())
                          + ",\"missing\":[";
        for (int i = 0; i < missing.size(); ++i)
            json += (i > 0 ? "," : "") + juce::String ("\"") + missing[i] + "\"";
        json += "]}\n";
        printToStdout (json);
        setApplicationReturnValue (missing.isEmpty() ? 0 : 1);
        quit();
    }

    // --- --stress ----------------------------------------------------------
    void startStress (int seconds, int viewIndex)
    {
        auto* lumenProcessor = static_cast<LumenAudioProcessor*> (::createPluginFilter());
        harnessProcessor.reset (lumenProcessor);

        const auto error = deviceManager.initialiseWithDefaultDevices (0, 2);
        if (error.isNotEmpty() || deviceManager.getCurrentAudioDevice() == nullptr)
        {
            printToStdout ("{\"error\":\"no audio device: " + error + "\"}\n");
            setApplicationReturnValue (2);
            quit();
            return;
        }

        player = std::make_unique<juce::AudioProcessorPlayer>();
        player->setProcessor (harnessProcessor.get());
        deviceManager.addAudioCallback (player.get());

        harnessEditor.reset (harnessProcessor->createEditorAndMakeActive());
        auto* editor = dynamic_cast<LumenAudioProcessorEditor*> (harnessEditor.get());
        if (editor == nullptr)
        {
            printToStdout ("{\"error\":\"no editor\"}\n");
            setApplicationReturnValue (1);
            quit();
            return;
        }
        editor->setView (viewIndex);
        harnessEditor->setTopLeftPosition (40, 40);
        harnessEditor->addToDesktop (0);
        harnessEditor->setVisible (true);

        // Wiggle every continuous parameter except masterGain (no full-level
        // blasts out of the monitors during an automated test).
        for (auto* parameter : harnessProcessor->getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
                if (dynamic_cast<juce::AudioParameterFloat*> (ranged) != nullptr
                    && ranged->getParameterID() != "masterGain")
                    wiggleTargets.add (ranged);

        // 8-voice chord, held for the whole run (SPEC/PHASES: 8 sounding voices).
        for (const int note : { 36, 43, 48, 55, 60, 64, 67, 72 })
            lumenProcessor->uiNoteOn (note, 0.8f);

        // Let prepareToPlay/table builds settle, then measure clean.
        juce::Timer::callAfterDelay (1000, [this, editor, lumenProcessor, seconds]
        {
            lumenProcessor->resetPerfCounters();
            editor->setHudEnabled (true);

            stressTimer = std::make_unique<WiggleTimer> (*this);
            stressTimer->startTimerHz (60);

            juce::Timer::callAfterDelay (seconds * 1000, [this, editor, lumenProcessor, seconds]
            {
                stressTimer->stopTimer();
                const auto stats = editor->getFrameStats();
                const int dropoutTotal = lumenProcessor->dropoutCount();

                const auto json = juce::String ("{\"seconds\":") + juce::String (seconds)
                    + ",\"frames\":" + juce::String (stats.frames)
                    + ",\"frame_avg_ms\":" + juce::String (stats.averageMs, 3)
                    + ",\"frame_max_ms\":" + juce::String (stats.maxMs, 3)
                    + ",\"dropouts\":" + juce::String (dropoutTotal)
                    + ",\"audio_load\":" + juce::String (lumenProcessor->currentAudioLoad(), 3)
                    + "}\n";
                printToStdout (json);
                setApplicationReturnValue (stats.averageMs <= 16.7 && dropoutTotal == 0 ? 0 : 1);
                quit();
            });
        });
    }

    void wiggleOnce()
    {
        auto& random = juce::Random::getSystemRandom();
        for (int i = 0; i < 6; ++i)
            if (! wiggleTargets.isEmpty())
                if (auto* parameter = wiggleTargets[random.nextInt (wiggleTargets.size())])
                    parameter->setValueNotifyingHost (random.nextFloat());
    }

    struct WiggleTimer final : public juce::Timer
    {
        explicit WiggleTimer (LumenStandaloneApp& appRef) : app (appRef) {}
        void timerCallback() override { app.wiggleOnce(); }
        LumenStandaloneApp& app;
    };

    // Drives processBlock at ~real time on the message thread with the note-on
    // chord in the first buffer — the same code path incoming hardware MIDI
    // takes, so the keyboard display and audio-activity plumbing are exercised
    // for real during a --screenshot run.
    struct MidiPumpTimer final : public juce::Timer
    {
        MidiPumpTimer (juce::AudioProcessor& processorRef, std::vector<int> notesToHold)
            : processor (processorRef), notes (std::move (notesToHold)) {}

        void timerCallback() override
        {
            juce::AudioBuffer<float> buffer (2, 512);
            for (int block = 0; block < 2; ++block)
            {
                juce::MidiBuffer midi;
                if (! sent)
                {
                    for (const int note : notes)
                        midi.addEvent (juce::MidiMessage::noteOn (1, note, 0.8f), 0);
                    sent = true;
                }
                processor.processBlock (buffer, midi);
            }
        }

        juce::AudioProcessor& processor;
        std::vector<int> notes;
        bool sent = false;
    };

    // --- --screenshot -------------------------------------------------------
    bool writeSnapshot (juce::Component& component)
    {
        const auto image = component.createComponentSnapshot (component.getLocalBounds(), true, 1.0f);
        if (! image.isValid())
            return false;
        screenshotFile.deleteFile();
        juce::FileOutputStream stream (screenshotFile);
        return stream.openedOk() && juce::PNGImageFormat().writeImageToStream (image, stream);
    }

    void finishScreenshot (bool ok)
    {
        printToStdout (ok ? "Screenshot written: " + screenshotFile.getFullPathName() + "\n"
                          : "Screenshot FAILED: " + screenshotFile.getFullPathName() + "\n");
        setApplicationReturnValue (ok ? 0 : 1);
        quit();
    }

    void takeScreenshotAndQuit()
    {
        midiPump = nullptr;
        if (tooltipParam.isNotEmpty())
            if (auto* editor = dynamic_cast<LumenAudioProcessorEditor*> (harnessEditor.get()))
                if (! editor->showTooltipFor (tooltipParam))
                    printToStdout ("Warning: --tooltip found no visible knob for '"
                                   + tooltipParam + "'\n");
        finishScreenshot (harnessEditor != nullptr && writeSnapshot (*harnessEditor));
    }

    // Renders the branded menu through LumenMenuLookAndFeel's own draw methods
    // (the exact code that styles the live popup) into an offscreen image. A
    // real async popup can't be snapshotted headlessly — with no pointer over
    // it the message loop dismisses it before it paints — so this drives the
    // same LnF directly, which is what needs verifying.
    juce::Image renderBrandedMenu (bool gear)
    {
        struct Row { int type; juce::String text; bool ticked, highlighted; int height; };
        enum { kHeader, kItem, kSeparator };

        LumenMenuLookAndFeel lnf;
        std::vector<Row> rows;
        int width = 210;

        auto measure = [&] (const juce::String& t)
        {
            int iw = 0, ih = 0;
            lnf.getIdealPopupMenuItemSize (t, false, 26, iw, ih);
            width = juce::jmax (width, iw);
            return ih;
        };

        if (gear)
        {
            rows.push_back ({ kHeader, "Lumen", false, false, 24 });
            rows.push_back ({ kItem, "Version " + juce::String (JucePlugin_VersionString), false, false,
                              measure ("Version " + juce::String (JucePlugin_VersionString)) });
            rows.push_back ({ kSeparator, {}, false, false, 11 });
            rows.push_back ({ kItem, "MIDI Learn: right-click any knob", false, true,
                              measure ("MIDI Learn: right-click any knob") });
        }
        else if (auto* lumen = dynamic_cast<LumenAudioProcessor*> (harnessProcessor.get()))
        {
            auto& pm = lumen->presetManager();
            pm.refresh();
            const auto& entries = pm.entries();
            const int current = pm.currentIndex();
            juce::String lastCat;
            int hoverRow = -1;
            for (int i = 0; i < (int) entries.size(); ++i)
            {
                const auto& e = entries[(size_t) i];
                if (e.category != lastCat) { rows.push_back ({ kHeader, e.category, false, false, 24 }); lastCat = e.category; }
                if (hoverRow < 0 && i != current) hoverRow = (int) rows.size();
                rows.push_back ({ kItem, e.name, i == current, false, measure (e.name) });
            }
            if (hoverRow >= 0) rows[(size_t) hoverRow].highlighted = true; // demo the hover row
            rows.push_back ({ kSeparator, {}, false, false, 11 });
            rows.push_back ({ kItem, "Save Preset...", false, false, measure ("Save Preset...") });
        }

        width = juce::jlimit (210, 320, width);
        const int border = lnf.getPopupMenuBorderSize();

        // The preset browser shows in three columns (rows flow top to bottom,
        // column by column, like PopupMenu's own layout); the gear menu in one.
        const int numColumns = gear ? 1 : 3;
        int totalH = 0;
        for (const auto& r : rows) totalH += r.height;
        const int targetH = (totalH + numColumns - 1) / numColumns;

        std::vector<std::vector<Row>> columns (static_cast<size_t> (numColumns));
        int columnH = 0, maxColumnH = 0;
        size_t c = 0;
        for (const auto& r : rows)
        {
            if (columnH >= targetH && c + 1 < columns.size())
            {
                c++;
                columnH = 0;
            }
            columns[c].push_back (r);
            columnH += r.height;
            maxColumnH = juce::jmax (maxColumnH, columnH);
        }

        const int imageW = border * 2 + width * numColumns;
        const int imageH = border * 2 + maxColumnH;
        juce::Image img (juce::Image::ARGB, imageW, imageH, true);
        juce::Graphics g (img);
        lnf.drawPopupMenuBackground (g, imageW, imageH);

        for (size_t col = 0; col < columns.size(); ++col)
        {
            const int x = border + static_cast<int> (col) * width;
            int y = border;
            for (const auto& r : columns[col])
            {
                const juce::Rectangle<int> area (x, y, width, r.height);
                if (r.type == kHeader)
                    lnf.drawPopupMenuSectionHeader (g, area, r.text);
                else if (r.type == kSeparator)
                    lnf.drawPopupMenuItem (g, area, true, false, false, false, false, {}, {}, nullptr, nullptr);
                else
                    lnf.drawPopupMenuItem (g, area, false, true, r.highlighted, r.ticked, false,
                                           r.text, {}, nullptr, nullptr);
                y += r.height;
            }
        }
        return img;
    }

    void captureMenuAndQuit()
    {
        auto menu = renderBrandedMenu (menuMode == "gear");
        bool ok = false;
        if (menu.isValid())
        {
            // --menu-bg: prove the corners really are transparent by
            // compositing the ARGB menu over a backdrop with a margin.
            if (menuBg.isNotEmpty())
            {
                const auto backdrop = menuBg == "dark" ? juce::Colour (0xff0f0f12)
                                                       : juce::Colour (0xffe9e9ec);
                juce::Image composite (juce::Image::RGB, menu.getWidth() + 40,
                                       menu.getHeight() + 40, false);
                juce::Graphics g (composite);
                g.fillAll (backdrop);
                g.drawImageAt (menu, 20, 20);
                menu = composite;
            }

            screenshotFile.deleteFile();
            juce::FileOutputStream stream (screenshotFile);
            ok = stream.openedOk() && juce::PNGImageFormat().writeImageToStream (menu, stream);
        }
        finishScreenshot (ok);
    }

    void releaseHarnessObjects()
    {
        if (harnessEditor != nullptr && harnessProcessor != nullptr)
            harnessProcessor->editorBeingDeleted (harnessEditor.get());

        harnessEditor = nullptr;
        harnessProcessor = nullptr;
    }

    juce::ApplicationProperties properties;
    std::unique_ptr<juce::StandaloneFilterWindow> window;

    std::unique_ptr<juce::AudioProcessor> harnessProcessor;
    std::unique_ptr<juce::AudioProcessorEditor> harnessEditor;
    juce::File screenshotFile;
    juce::String menuMode; // "" | "preset" | "gear" for --screenshot --menu
    juce::String menuBg;   // "" | "bright" | "dark" backdrop for --menu-bg
    juce::String tooltipParam; // paramId whose tooltip --tooltip pins

    juce::AudioDeviceManager deviceManager;
    std::unique_ptr<juce::AudioProcessorPlayer> player;
    std::unique_ptr<WiggleTimer> stressTimer;
    std::unique_ptr<MidiPumpTimer> midiPump;
    juce::Array<juce::RangedAudioParameter*> wiggleTargets;
};

#if JucePlugin_Build_Standalone
 START_JUCE_APPLICATION (LumenStandaloneApp)
#endif
