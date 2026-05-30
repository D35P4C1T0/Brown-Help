#include "BrownHelpEditor.h"
#include "BrownHelpProcessor.h"

#include <juce_gui_extra/juce_gui_extra.h>

namespace
{
juce::File outputFileFromCommandLine(const juce::String& commandLine)
{
    const auto trimmed = commandLine.trim();

    if (trimmed.isNotEmpty())
        return juce::File::getCurrentWorkingDirectory().getChildFile(trimmed);

    return juce::File::getCurrentWorkingDirectory().getChildFile("docs/plugin-ui.png");
}

class ScreenshotApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "Brown Help UI Screenshot"; }
    const juce::String getApplicationVersion() override { return "1.0"; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise(const juce::String& commandLine) override
    {
        outputFile = outputFileFromCommandLine(commandLine);
        processor = std::make_unique<BrownHelp::BrownHelpProcessor>();
        processor->prepareToPlay(48000.0, 512);
        editor = std::make_unique<BrownHelp::BrownHelpEditor>(*processor);
        editor->setBounds(0, 0, editor->getWidth(), editor->getHeight());

        juce::MessageManager::callAsync([this] { renderAndQuit(); });
    }

    void shutdown() override
    {
        editor.reset();
        processor.reset();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted(const juce::String&) override {}

private:
    void renderAndQuit()
    {
        const auto width = editor->getWidth();
        const auto height = editor->getHeight();
        juce::Image image(juce::Image::ARGB, width, height, true);
        juce::Graphics graphics(image);
        editor->paintEntireComponent(graphics, true);

        outputFile.getParentDirectory().createDirectory();
        outputFile.deleteFile();

        juce::PNGImageFormat png;
        juce::FileOutputStream stream(outputFile);

        if (! stream.openedOk() || ! png.writeImageToStream(image, stream))
        {
            setApplicationReturnValue(1);
            quit();
            return;
        }

        setApplicationReturnValue(0);
        quit();
    }

    juce::File outputFile;
    std::unique_ptr<BrownHelp::BrownHelpProcessor> processor;
    std::unique_ptr<BrownHelp::BrownHelpEditor> editor;
};
}

START_JUCE_APPLICATION(ScreenshotApplication)
