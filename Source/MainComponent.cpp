#include "MainComponent.h"

// ══════════════════════════════════════════════
// EQDisplay
// ══════════════════════════════════════════════

EQDisplay::EQDisplay()
{
    startTimerHz(30);
    setInterceptsMouseClicks(true, false);
}

juce::Rectangle<float> EQDisplay::graphArea() const
{
    return getLocalBounds().toFloat().reduced(30, 8);
}

float EQDisplay::freqToX(float freq, juce::Rectangle<float> a) const
{
    return a.getX() + a.getWidth() * std::log10(freq / 20.f) / std::log10(20000.f / 20.f);
}
float EQDisplay::gainToY(float gain, juce::Rectangle<float> a) const
{
    return juce::jmap(gain, dBMin, dBMax, a.getBottom(), a.getY());
}
float EQDisplay::xToFreq(float x, juce::Rectangle<float> a) const
{
    return 20.f * std::pow(20000.f / 20.f, (x - a.getX()) / a.getWidth());
}
float EQDisplay::yToGain(float y, juce::Rectangle<float> a) const
{
    return juce::jmap(y, a.getBottom(), a.getY(), dBMin, dBMax);
}

int EQDisplay::bandHitTest(juce::Point<float> p) const
{
    auto a = graphArea();
    for (int i = 0; i < 4; ++i)
    {
        float x = juce::jlimit(a.getX(), a.getRight(),  freqToX(bands[i].freq, a));
        float y = juce::jlimit(a.getY(), a.getBottom(), gainToY(bands[i].gain, a));
        if (std::abs(p.x - x) < 14.f && std::abs(p.y - y) < 14.f)
            return i;
    }
    return -1;
}

void EQDisplay::pushBuffer(const float* data, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        fifo[fifoIndex++] = data[i];
        if (fifoIndex == fftSize)
        {
            fftData = fifo;
            window.multiplyWithWindowingTable(fftData.data(), fftSize);
            fft.performFrequencyOnlyForwardTransform(fftData.data());
            for (int j = 0; j < fftSize / 2; ++j)
                spectrumData[j] = juce::Decibels::gainToDecibels(fftData[j] / fftSize);
            fifoIndex = 0;
            nextFFTBlockReady = true;
        }
    }
}

void EQDisplay::mouseDown(const juce::MouseEvent& e)
{
    draggingBand = bandHitTest(e.position);
    if (draggingBand >= 0)
    {
        selectedBand = draggingBand;
        if (onBandSelected) onBandSelected(draggingBand);

        if (e.mods.isRightButtonDown())
        {
            juce::PopupMenu menu;
            menu.addItem(1, "High Pass");
            menu.addItem(2, "Lo Shelf");
            menu.addItem(3, "Peak");
            menu.addItem(4, "Hi Shelf");
            menu.addItem(5, "Low Pass");
            int hit = draggingBand;
            menu.showMenuAsync(juce::PopupMenu::Options{}.withTargetScreenArea(
                juce::Rectangle<int>((int)e.getScreenX(), (int)e.getScreenY(), 1, 1)),
                [this, hit](int result) {
                    if (result <= 0) return;
                    FilterType t = static_cast<FilterType>(result - 1);
                    bands[hit].type = t;
                    if (onFilterTypeChanged) onFilterTypeChanged(hit, t);
                    repaint();
                });
            draggingBand = -1;
        }
    }
}

void EQDisplay::mouseDrag(const juce::MouseEvent& e)
{
    if (draggingBand < 0) return;
    auto a = graphArea();
    float freq = juce::jlimit(20.f, 20000.f, xToFreq(e.position.x, a));
    float gain = juce::jlimit(dBMin, dBMax,  yToGain(e.position.y, a));
    bands[draggingBand].freq = freq;
    bands[draggingBand].gain = gain;
    if (onBandDragged) onBandDragged(draggingBand, freq, gain);
    repaint();
}

void EQDisplay::drawGrid(juce::Graphics& g, juce::Rectangle<float> a)
{
    // Background
    g.setColour(juce::Colour(0xff1a1a2e));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.f);

    // Grid lines
    g.setColour(juce::Colours::white.withAlpha(0.06f));
    for (float freq : {30.f, 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f})
    {
        float x = freqToX(freq, a);
        g.drawVerticalLine((int)x, a.getY(), a.getBottom());
    }
    for (float db : {-9.f, -6.f, -3.f, 3.f, 6.f, 9.f})
    {
        float y = gainToY(db, a);
        g.drawHorizontalLine((int)y, a.getX(), a.getRight());
    }

    // 0dB line
    g.setColour(juce::Colours::white.withAlpha(0.15f));
    float y0 = gainToY(0.f, a);
    g.drawHorizontalLine((int)y0, a.getX(), a.getRight());

    // Frequency labels
    g.setColour(juce::Colours::grey.withAlpha(0.6f));
    g.setFont(9.f);
    for (auto [f, label] : std::initializer_list<std::pair<float, const char*>>{
        {100.f,"100"},{1000.f,"1k"},{10000.f,"10k"}})
    {
        float x = freqToX(f, a);
        g.drawText(label, juce::Rectangle<float>(x-15, a.getBottom()+2, 30, 12),
                   juce::Justification::centred);
    }

    // dB labels
    for (auto [db, label] : std::initializer_list<std::pair<float, const char*>>{
        {12.f,"+12"},{6.f,"+6"},{0.f,"0"},{-6.f,"-6"},{-12.f,"-12"}})
    {
        float y = gainToY(db, a);
        g.drawText(label, juce::Rectangle<float>(2, y-6, 26, 12),
                   juce::Justification::centredRight);
    }
}

void EQDisplay::drawSpectrum(juce::Graphics& g, juce::Rectangle<float> a)
{
    if (!nextFFTBlockReady) return;
    juce::Path p;
    bool started = false;
    for (int i = 1; i < fftSize / 2; ++i)
    {
        float freq = (float)i * (float)sampleRate / fftSize;
        if (freq < 20.f || freq > 20000.f) continue;
        float x  = freqToX(freq, a);
        float dB = juce::jlimit(-60.f, 6.f, spectrumData[i]);
        float y  = juce::jmap(dB, -60.f, 6.f, a.getBottom(), a.getY());
        if (!started) { p.startNewSubPath(x, a.getBottom()); p.lineTo(x, y); started = true; }
        else p.lineTo(x, y);
    }
    if (!started) return;
    p.lineTo(a.getRight(), a.getBottom());
    p.closeSubPath();

    g.saveState();
    g.reduceClipRegion(a.toNearestInt());
    g.setColour(juce::Colours::white.withAlpha(0.06f));
    g.fillPath(p);
    g.setColour(juce::Colours::white.withAlpha(0.15f));
    g.strokePath(p, juce::PathStrokeType(1.f));
    g.restoreState();
}

void EQDisplay::drawEQCurve(juce::Graphics& g, juce::Rectangle<float> a)
{
    juce::Path curve;
    bool started = false;

    for (int px = (int)a.getX(); px < (int)a.getRight(); ++px)
    {
        float freq = xToFreq((float)px, a);
        double totalGain = 1.0;

        for (auto& band : bands)
        {
            if (!band.enabled) continue;
            double w0    = 2.0 * juce::MathConstants<double>::pi * band.freq / sampleRate;
            double cosW  = std::cos(w0), sinW = std::sin(w0);
            double A     = std::pow(10.0, band.gain / 40.0);
            double alpha = sinW / (2.0 * band.q);
            double wTest = 2.0 * juce::MathConstants<double>::pi * freq / sampleRate;
            double b0, b1, b2, a0, a1, a2;

            switch (band.type)
            {
                case FilterType::LowShelf:
                    b0=A*((A+1)-(A-1)*cosW+2*std::sqrt(A)*alpha);
                    b1=2*A*((A-1)-(A+1)*cosW);
                    b2=A*((A+1)-(A-1)*cosW-2*std::sqrt(A)*alpha);
                    a0=(A+1)+(A-1)*cosW+2*std::sqrt(A)*alpha;
                    a1=-2*((A-1)+(A+1)*cosW);
                    a2=(A+1)+(A-1)*cosW-2*std::sqrt(A)*alpha;
                    break;
                case FilterType::HighShelf:
                    b0=A*((A+1)+(A-1)*cosW+2*std::sqrt(A)*alpha);
                    b1=-2*A*((A-1)+(A+1)*cosW);
                    b2=A*((A+1)+(A-1)*cosW-2*std::sqrt(A)*alpha);
                    a0=(A+1)-(A-1)*cosW+2*std::sqrt(A)*alpha;
                    a1=2*((A-1)-(A+1)*cosW);
                    a2=(A+1)-(A-1)*cosW-2*std::sqrt(A)*alpha;
                    break;
                case FilterType::LowCut:
                    b0=(1-cosW)/2; b1=1-cosW;    b2=(1-cosW)/2;
                    a0=1+alpha;    a1=-2*cosW;    a2=1-alpha;
                    break;
                case FilterType::HighCut:
                    b0=(1+cosW)/2; b1=-(1+cosW); b2=(1+cosW)/2;
                    a0=1+alpha;    a1=-2*cosW;   a2=1-alpha;
                    break;
                default:
                    b0=1+alpha*A; b1=-2*cosW; b2=1-alpha*A;
                    a0=1+alpha/A; a1=-2*cosW; a2=1-alpha/A;
                    break;
            }

            double w      = wTest;
            double ejw_re  = std::cos(-w),   ejw_im  = std::sin(-w);
            double ej2w_re = std::cos(-2*w), ej2w_im = std::sin(-2*w);
            double num_re  = b0 + b1*ejw_re + b2*ej2w_re;
            double num_im  =      b1*ejw_im + b2*ej2w_im;
            double den_re  = a0 + a1*ejw_re + a2*ej2w_re;
            double den_im  =      a1*ejw_im + a2*ej2w_im;
            double mag = std::sqrt((num_re*num_re + num_im*num_im) /
                                   (den_re*den_re + den_im*den_im + 1e-12));
            totalGain *= mag;
        }

        float dB = (float)juce::Decibels::gainToDecibels(totalGain);
        dB = juce::jlimit(dBMin, dBMax, dB);
        float y = gainToY(dB, a);
        if (!started) { curve.startNewSubPath((float)px, y); started = true; }
        else curve.lineTo((float)px, y);
    }

    float zeroY = gainToY(0.f, a);
    juce::Path filled = curve;
    filled.lineTo(a.getRight(), zeroY);
    filled.lineTo(a.getX(), zeroY);
    filled.closeSubPath();

    g.saveState();
    g.reduceClipRegion(a.toNearestInt());
    g.setColour(juce::Colour(0xff00aaff).withAlpha(0.10f));
    g.fillPath(filled);
    g.setColour(juce::Colour(0xff44ccff));
    g.strokePath(curve, juce::PathStrokeType(2.f));
    g.restoreState();
}

void EQDisplay::drawHandles(juce::Graphics& g, juce::Rectangle<float> a)
{
    for (int i = 0; i < 4; ++i)
    {
        float x = juce::jlimit(a.getX() + 10, a.getRight()  - 10, freqToX(bands[i].freq, a));
        float y = juce::jlimit(a.getY() + 10, a.getBottom() - 10, gainToY(bands[i].gain, a));
        float r = (i == selectedBand) ? 10.f : 7.f;

        // Outer ring
        g.setColour(bandColours[i]);
        g.drawEllipse(x - r, y - r, r * 2, r * 2, 2.f);
        // Inner fill
        g.setColour(bandColours[i].withAlpha(i == selectedBand ? 0.8f : 0.4f));
        g.fillEllipse(x - r + 2, y - r + 2, (r - 2) * 2, (r - 2) * 2);

        // Number
        g.setColour(juce::Colours::white);
        g.setFont(9.f);
        g.drawText(juce::String(i + 1),
                   juce::Rectangle<float>(x - 5, y - 5, 10, 10),
                   juce::Justification::centred);
    }
}

void EQDisplay::paint(juce::Graphics& g)
{
    auto a = graphArea();
    drawGrid(g, a);
    drawSpectrum(g, a);
    drawEQCurve(g, a);
    drawHandles(g, a);
}

// ══════════════════════════════════════════════
// ControlPanel (left side, Ableton style)
// ══════════════════════════════════════════════

ControlPanel::ControlPanel()
{
    auto setupSlider = [&](juce::Slider& s, juce::Label& l, const juce::String& name)
    {
        s.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 13);
        s.setColour(juce::Slider::rotarySliderFillColourId,    juce::Colour(0xff888888));
        s.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff333333));
        s.setColour(juce::Slider::textBoxTextColourId,         juce::Colours::lightgrey);
        s.setColour(juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
        s.onValueChange = [this]{
            if (!updating && onChange)
                onChange((float)freqSlider.getValue(),
                         (float)gainSlider.getValue(),
                         (float)qSlider.getValue());
        };
        addAndMakeVisible(s);
        l.setText(name, juce::dontSendNotification);
        l.setJustificationType(juce::Justification::centred);
        l.setFont(juce::Font(juce::FontOptions(9.f)));
        l.setColour(juce::Label::textColourId, juce::Colours::grey);
        addAndMakeVisible(l);
    };

    freqSlider.setRange(20.0, 20000.0, 1.0);
    freqSlider.setSkewFactorFromMidPoint(1000.0);
    freqSlider.setValue(1000.0);
    freqSlider.setTextValueSuffix(" Hz");

    gainSlider.setRange(-12.0, 12.0, 0.1);
    gainSlider.setValue(0.0);
    gainSlider.setTextValueSuffix(" dB");

    qSlider.setRange(0.1, 10.0, 0.01);
    qSlider.setSkewFactorFromMidPoint(1.0);
    qSlider.setValue(0.707);

    setupSlider(freqSlider, freqLabel, "Freq");
    setupSlider(gainSlider, gainLabel, "Gain");
    setupSlider(qSlider,    qLabel,    "Q");

    bandLabel.setJustificationType(juce::Justification::centred);
    bandLabel.setFont(juce::Font(juce::FontOptions(11.f).withStyle("Bold")));
    bandLabel.setColour(juce::Label::textColourId, juce::Colour(0xffff9933));
    addAndMakeVisible(bandLabel);

    filterTypeBox.addItem("Low Pass", 1);  // cuts lows = high pass ✓
    filterTypeBox.addItem("Lo Shelf",  2);
    filterTypeBox.addItem("Peak",      3);
    filterTypeBox.addItem("Hi Shelf",  4);
    filterTypeBox.addItem("High Pass",  5);  // cuts highs = low pass ✓
    filterTypeBox.setSelectedId(3, juce::dontSendNotification);
    filterTypeBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff1a1a2e));
    filterTypeBox.setColour(juce::ComboBox::textColourId,       juce::Colours::lightgrey);
    filterTypeBox.setColour(juce::ComboBox::outlineColourId,    juce::Colours::grey.withAlpha(0.3f));
    filterTypeBox.onChange = [this]{
        if (!updating && onFilterTypeChanged)
            onFilterTypeChanged(static_cast<FilterType>(filterTypeBox.getSelectedId() - 1));
    };
    addAndMakeVisible(filterTypeBox);

    initButton.setColour(juce::TextButton::buttonColourId,  juce::Colour(0xff2a2a3e));
    initButton.setColour(juce::TextButton::textColourOffId, juce::Colours::grey);
    initButton.onClick = [this]{ if (onInit) onInit(); };
    addAndMakeVisible(initButton);
}

void ControlPanel::setValues(float freq, float gain, float q)
{
    updating = true;
    freqSlider.setValue(freq, juce::dontSendNotification);
    gainSlider.setValue(gain, juce::dontSendNotification);
    qSlider.setValue(q,       juce::dontSendNotification);
    updating = false;
}

void ControlPanel::setFilterType(FilterType t)
{
    updating = true;
    filterTypeBox.setSelectedId((int)t + 1, juce::dontSendNotification);
    updating = false;
}

void ControlPanel::setBandIndex(int i)
{
    currentBand = i;
    bandLabel.setText("Band " + juce::String(i + 1), juce::dontSendNotification);
}

void ControlPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff111122));
}

void ControlPanel::resized()
{
    auto b = getLocalBounds().reduced(4);
    bandLabel.setBounds(b.removeFromTop(18));
    filterTypeBox.setBounds(b.removeFromTop(22).reduced(2, 2));

    int h3 = (b.getHeight() - 22) / 3;
    for (auto [s, l] : { std::pair{&freqSlider, &freqLabel},
                          std::pair{&gainSlider, &gainLabel},
                          std::pair{&qSlider,    &qLabel} })
    {
        auto row = b.removeFromTop(h3);
        l->setBounds(row.removeFromTop(12));
        s->setBounds(row);
    }

    initButton.setBounds(b.removeFromBottom(20).reduced(6, 2));
}

// ══════════════════════════════════════════════
// MainComponent
// ══════════════════════════════════════════════

MainComponent::MainComponent()
{
    for (int i = 0; i < 4; ++i) bands[i] = defaultBands[i];

    eqDisplay.setBands(bands);
    eqDisplay.onBandSelected      = [this](int i){ selectBand(i); };
    eqDisplay.onBandDragged       = [this](int i, float f, float g){ onBandDragged(i, f, g); };
    eqDisplay.onFilterTypeChanged = [this](int i, FilterType t){ onDisplayFilterTypeChanged(i, t); };
    addAndMakeVisible(eqDisplay);

    controlPanel.onChange            = [this](float f, float g, float q){ onControlChanged(f, g, q); };
    controlPanel.onFilterTypeChanged = [this](FilterType t){ onFilterTypeChanged(t); };
    controlPanel.onInit              = [this]{ resetAll(); };
    addAndMakeVisible(controlPanel);

    selectBand(0);
    setSize(700, 260);
    setAudioChannels(2, 2);
}

MainComponent::~MainComponent() { shutdownAudio(); }

void MainComponent::prepareToPlay(int samplesPerBlock, double sampleRate)
{
    currentSampleRate = sampleRate;
    eqDisplay.setSampleRate(sampleRate);
    juce::dsp::ProcessSpec spec { sampleRate, (uint32_t)samplesPerBlock, 1 };
    filterChainL.prepare(spec);
    filterChainR.prepare(spec);
    updateFilters();
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
{
    auto* buffer = info.buffer;
    juce::dsp::AudioBlock<float> block(*buffer, (size_t)info.startSample);
    auto blockL = block.getSingleChannelBlock(0);
    auto blockR = block.getSingleChannelBlock(1);
    juce::dsp::ProcessContextReplacing<float> ctxL(blockL);
    juce::dsp::ProcessContextReplacing<float> ctxR(blockR);
    filterChainL.process(ctxL);
    filterChainR.process(ctxR);
    eqDisplay.pushBuffer(buffer->getReadPointer(0, info.startSample), info.numSamples);
}

void MainComponent::releaseResources() {}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff111122));
}

void MainComponent::resized()
{
    auto b = getLocalBounds();
    controlPanel.setBounds(b.removeFromLeft(85));
    eqDisplay.setBounds(b);
}

void MainComponent::selectBand(int i)
{
    selectedBand = i;
    eqDisplay.setSelectedBand(i);
    controlPanel.setValues(bands[i].freq, bands[i].gain, bands[i].q);
    controlPanel.setFilterType(bands[i].type);
    controlPanel.setBandIndex(i);
}

void MainComponent::resetAll()
{
    for (int i = 0; i < 4; ++i) bands[i] = defaultBands[i];
    eqDisplay.setBands(bands);
    selectBand(0);
    updateFilters();
}

void MainComponent::onBandDragged(int i, float freq, float gain)
{
    bands[i].freq = freq;
    bands[i].gain = gain;
    if (i == selectedBand)
        controlPanel.setValues(freq, gain, bands[i].q);
    updateFilters();
}

void MainComponent::onControlChanged(float freq, float gain, float q)
{
    bands[selectedBand].freq = freq;
    bands[selectedBand].gain = gain;
    bands[selectedBand].q    = q;
    eqDisplay.setBands(bands);
    updateFilters();
}

void MainComponent::onFilterTypeChanged(FilterType t)
{
    bands[selectedBand].type = t;
    eqDisplay.setBands(bands);
    updateFilters();
}

void MainComponent::onDisplayFilterTypeChanged(int band, FilterType t)
{
    bands[band].type = t;
    eqDisplay.setBands(bands);
    if (band == selectedBand)
        controlPanel.setFilterType(t);
    updateFilters();
}

void MainComponent::updateFilters()
{
    eqDisplay.setBands(bands);

    auto makeCoeffs = [&](const BandParams& p)
        -> juce::ReferenceCountedObjectPtr<juce::dsp::IIR::Coefficients<float>>
    {
        float gain = juce::Decibels::decibelsToGain(p.gain);
        switch (p.type)
        {
            case FilterType::LowShelf:
                if (std::abs(p.gain) < 0.1f)
                    return juce::dsp::IIR::Coefficients<float>::makeAllPass(
                        currentSampleRate, p.freq);
                return juce::dsp::IIR::Coefficients<float>::makeLowShelf(
                    currentSampleRate, p.freq, p.q, gain);
            case FilterType::HighShelf:
                if (std::abs(p.gain) < 0.1f)
                    return juce::dsp::IIR::Coefficients<float>::makeAllPass(
                        currentSampleRate, p.freq);
                return juce::dsp::IIR::Coefficients<float>::makeHighShelf(
                    currentSampleRate, p.freq, p.q, gain);
            case FilterType::LowCut:
                return juce::dsp::IIR::Coefficients<float>::makeHighPass(
                    currentSampleRate, p.freq, 0.707f);
            case FilterType::HighCut:
                return juce::dsp::IIR::Coefficients<float>::makeLowPass(
                    currentSampleRate, p.freq, 0.707f);
            default:
                if (std::abs(p.gain) < 0.1f)
                    return juce::dsp::IIR::Coefficients<float>::makeAllPass(
                        currentSampleRate, p.freq);
                return juce::dsp::IIR::Coefficients<float>::makePeakFilter(
                    currentSampleRate, p.freq, p.q, gain);
        }
    };

    *filterChainL.get<0>().coefficients = *makeCoeffs(bands[0]);
    *filterChainL.get<1>().coefficients = *makeCoeffs(bands[1]);
    *filterChainL.get<2>().coefficients = *makeCoeffs(bands[2]);
    *filterChainL.get<3>().coefficients = *makeCoeffs(bands[3]);
    *filterChainR.get<0>().coefficients = *makeCoeffs(bands[0]);
    *filterChainR.get<1>().coefficients = *makeCoeffs(bands[1]);
    *filterChainR.get<2>().coefficients = *makeCoeffs(bands[2]);
    *filterChainR.get<3>().coefficients = *makeCoeffs(bands[3]);
}