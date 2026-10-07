#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "MainComponent.h"

class ParametricEQApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "ParametricEQ"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }

    void initialise(const juce::String&) override
    {
        mainWindow.reset(new MainWindow("Parametric EQ", new MainComponent(), *this));
    }

    void shutdown() override { mainWindow = nullptr; }

    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow(juce::String name, juce::Component* c, JUCEApplication& a)
            : DocumentWindow(name, juce::Colours::darkgrey, DocumentWindow::allButtons), app(a)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(c, true);
            setResizable(true, true);
            centreWithSize(800, 500);
            setVisible(true);
        }
        void closeButtonPressed() override { app.systemRequestedQuit(); }
    private:
        JUCEApplication& app;
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(ParametricEQApplication)