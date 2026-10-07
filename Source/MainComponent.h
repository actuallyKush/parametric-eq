#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <BinaryData.h>
#include <array>

enum class FilterType { LowCut, LowShelf, Peak, HighShelf, HighCut };

struct BandParams
{
    float freq = 1000.f;
    float gain = 0.f;
    float q    = 0.707f;
    FilterType type    = FilterType::Peak;
    bool       enabled = true;
};

static const BandParams defaultBands[4] = {
    { 80.f,   0.f, 0.707f, FilterType::LowShelf  },
    { 500.f,  0.f, 1.0f,   FilterType::Peak      },
    { 3000.f, 0.f, 1.0f,   FilterType::Peak      },
    { 10000.f,0.f, 0.707f, FilterType::HighShelf }
};

static const char* filterTypeName(FilterType t)
{
    switch(t) {
        case FilterType::LowCut:    return "HP";
        case FilterType::LowShelf:  return "LS";
        case FilterType::Peak:      return "PK";
        case FilterType::HighShelf: return "HS";
        case FilterType::HighCut:   return "LP";
    }
    return "";
}

static const juce::Colour bandColours[4] = {
    juce::Colour(0xffff9933),
    juce::Colour(0xffff9933),
    juce::Colour(0xffff9933),
    juce::Colour(0xffff9933)
};

// ──────────────────────────────────────────────
class EQDisplay : public juce::Component, private juce::Timer
{
public:
    EQDisplay();
    void pushBuffer(const float* data, int numSamples);
    void setSampleRate(double sr) { sampleRate = sr; }
    void setBands(const std::array<BandParams, 4>& b) { bands = b; }
    void setSelectedBand(int i) { selectedBand = i; repaint(); }

    std::function<void(int)>               onBandSelected;
    std::function<void(int, float, float)> onBandDragged;
    std::function<void(int, FilterType)>   onFilterTypeChanged;

    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void paint(juce::Graphics&) override;

private:
    void timerCallback() override { repaint(); }
    void drawGrid(juce::Graphics&, juce::Rectangle<float> area);
    void drawSpectrum(juce::Graphics&, juce::Rectangle<float> area);
    void drawEQCurve(juce::Graphics&, juce::Rectangle<float> area);
    void drawHandles(juce::Graphics&, juce::Rectangle<float> area);

    float freqToX(float freq, juce::Rectangle<float> area) const;
    float gainToY(float gain, juce::Rectangle<float> area) const;
    float xToFreq(float x, juce::Rectangle<float> area)    const;
    float yToGain(float y, juce::Rectangle<float> area)    const;
    int   bandHitTest(juce::Point<float> p) const;
    juce::Rectangle<float> graphArea() const;

    static constexpr int fftOrder = 11;
    static constexpr int fftSize  = 1 << fftOrder;

    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window {
        (size_t)fftSize, juce::dsp::WindowingFunction<float>::hann };

    std::array<float, fftSize * 2> fifo    {};
    std::array<float, fftSize * 2> fftData {};
    std::array<float, fftSize / 2> spectrumData {};
    int    fifoIndex         = 0;
    bool   nextFFTBlockReady = false;
    double sampleRate        = 44100.0;

    std::array<BandParams, 4> bands;
    int selectedBand = 0;
    int draggingBand = -1;

    static constexpr float dBMin = -12.f;
    static constexpr float dBMax = +12.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EQDisplay)
};

// ──────────────────────────────────────────────
class ControlPanel : public juce::Component
{
public:
    ControlPanel();
    std::function<void(float, float, float)> onChange;
    std::function<void(FilterType)>          onFilterTypeChanged;
    std::function<void()>                    onInit;
    void setValues(float freq, float gain, float q);
    void setFilterType(FilterType t);
    void setBandIndex(int i);
    void resized() override;
    void paint(juce::Graphics&) override;

private:
    juce::Slider freqSlider, gainSlider, qSlider;
    juce::Label  freqLabel, gainLabel, qLabel, bandLabel;
    juce::TextButton initButton { "INIT" };
    juce::ComboBox filterTypeBox;
    int currentBand = 0;
    bool updating = false;
};

// ──────────────────────────────────────────────
class MainComponent : public juce::AudioAppComponent
{
public:
    MainComponent();
    ~MainComponent() override;

    void prepareToPlay(int, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo&) override;
    void releaseResources() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void updateFilters();
    void selectBand(int i);
    void resetAll();
    void onBandDragged(int i, float freq, float gain);
    void onControlChanged(float freq, float gain, float q);
    void onFilterTypeChanged(FilterType t);
    void onDisplayFilterTypeChanged(int band, FilterType t);

    using Filter      = juce::dsp::IIR::Filter<float>;
    using FilterChain = juce::dsp::ProcessorChain<Filter, Filter, Filter, Filter>;
    FilterChain filterChainL, filterChainR;

    std::array<BandParams, 4> bands;
    int    selectedBand      = 0;
    double currentSampleRate = 44100.0;

    EQDisplay    eqDisplay;
    ControlPanel controlPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};