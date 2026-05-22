#include "MultibandGraph.h"

namespace kratomix::multiband
{
namespace
{
constexpr float minFrequency = 20.0f;
constexpr float maxFrequency = 20000.0f;
constexpr float minimumCrossoverGap = 30.0f;

juce::Colour graphBackground()
{
    return juce::Colour::fromRGB(12, 13, 15);
}

juce::Colour gridColour()
{
    return juce::Colour::fromRGB(48, 50, 55);
}

juce::Colour fineGridColour()
{
    return juce::Colour::fromRGB(31, 33, 37);
}

juce::Colour accentColour()
{
    return juce::Colour::fromRGB(241, 194, 124);
}

juce::Colour bandColour(int index)
{
    static constexpr std::array<juce::uint32, maxBands> colours {{
        0xffb74ad9,
        0xff4a8eda,
        0xff5fd3ba,
        0xff8bdc50,
        0xffffc84a,
        0xffff8b3d
    }};

    return juce::Colour(colours[static_cast<size_t>(juce::jlimit(0, maxBands - 1, index))]);
}

juce::String frequencyLabel(float frequency)
{
    if (frequency >= 1000.0f)
        return juce::String(frequency / 1000.0f, frequency >= 10000.0f ? 0 : 1) + "k";

    return juce::String(static_cast<int>(std::round(frequency)));
}
}

MultibandGraph::MultibandGraph()
    : fft(fftOrder),
      window(fftSize, juce::dsp::WindowingFunction<float>::hann)
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    inputSpectrum.fill(-100.0f);
    outputSpectrum.fill(-100.0f);
}

MultibandGraph::~MultibandGraph()
{
    if (draggedCrossoverParameter != nullptr)
        draggedCrossoverParameter->endChangeGesture();
}

void MultibandGraph::attachState(juce::AudioProcessorValueTreeState& stateToUse)
{
    state = &stateToUse;
    repaint();
}

void MultibandGraph::attachAnalyzerReader(std::function<void(MultibandAnalyzerFrame&)> reader)
{
    analyzerReader = std::move(reader);
    startTimerHz(30);
}

void MultibandGraph::paint(juce::Graphics& g)
{
    g.fillAll(graphBackground());
    const auto graph = graphBounds();

    g.setColour(juce::Colours::black.withAlpha(0.35f));
    g.fillRoundedRectangle(graph.expanded(1.0f), 8.0f);
    g.setColour(juce::Colour::fromRGB(16, 17, 20));
    g.fillRoundedRectangle(graph, 8.0f);

    drawBands(g, graph);
    drawGrid(g, graph);

    const auto analyzerMode = static_cast<int>(std::round(parameterValue(analyzerModeId, static_cast<float>(AnalyzerMode::inputOutput))));
    if (analyzerMode == static_cast<int>(AnalyzerMode::input)
        || analyzerMode == static_cast<int>(AnalyzerMode::inputOutput))
        drawSpectrum(g, graph, inputSpectrum, juce::Colour::fromRGB(74, 142, 218).withAlpha(0.46f));

    if (analyzerMode == static_cast<int>(AnalyzerMode::output)
        || analyzerMode == static_cast<int>(AnalyzerMode::inputOutput)
        || analyzerMode == static_cast<int>(AnalyzerMode::gainReduction))
        drawSpectrum(g, graph, outputSpectrum, juce::Colour::fromRGB(95, 211, 186).withAlpha(0.55f));

    drawDynamics(g, graph);

    const auto crossovers = readCrossoverFrequencies();
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    for (int index = 0; index < crossoverCount; ++index)
    {
        const auto x = frequencyToX(crossovers[static_cast<size_t>(index)], graph);
        g.setColour(accentColour().withAlpha(index == draggedCrossover ? 0.9f : 0.65f));
        g.drawVerticalLine(static_cast<int>(std::round(x)), graph.getY(), graph.getBottom());
        g.fillRoundedRectangle(x - 4.0f, graph.getY() + 10.0f, 8.0f, 24.0f, 3.0f);
    }

    g.setColour(juce::Colour::fromRGB(71, 61, 47));
    g.drawRoundedRectangle(graph, 8.0f, 1.0f);

    if (hoverPoint.has_value() && graph.contains(*hoverPoint))
    {
        const auto frequency = xToFrequency(hoverPoint->x, graph);
        g.setColour(juce::Colour::fromRGB(250, 231, 202));
        g.setFont(juce::FontOptions(11.0f));
        g.drawText(frequencyLabel(frequency) + " Hz",
                   juce::Rectangle<float>(hoverPoint->x - 38.0f, graph.getY() + 8.0f, 76.0f, 16.0f).toNearestInt(),
                   juce::Justification::centred);
    }
}

void MultibandGraph::mouseDown(const juce::MouseEvent& event)
{
    const auto graph = graphBounds();
    if (! graph.contains(event.position))
        return;

    draggedCrossover = crossoverHandleAt(event.position.x);
    if (draggedCrossover >= 0 && state != nullptr)
    {
        draggedCrossoverParameter = state->getParameter(crossoverFrequencyIds[static_cast<size_t>(draggedCrossover)]);
        if (draggedCrossoverParameter != nullptr)
            draggedCrossoverParameter->beginChangeGesture();
        return;
    }

    setSelectedBand(bandIndexForX(event.position.x));
}

void MultibandGraph::mouseDrag(const juce::MouseEvent& event)
{
    if (draggedCrossover < 0)
        return;

    const auto graph = graphBounds();
    setCrossoverFrequency(draggedCrossover, xToFrequency(event.position.x, graph));
}

void MultibandGraph::mouseUp(const juce::MouseEvent&)
{
    if (draggedCrossoverParameter != nullptr)
        draggedCrossoverParameter->endChangeGesture();

    draggedCrossoverParameter = nullptr;
    draggedCrossover = -1;
}

void MultibandGraph::mouseMove(const juce::MouseEvent& event)
{
    hoverPoint = event.position;
    repaint();
}

void MultibandGraph::mouseExit(const juce::MouseEvent&)
{
    hoverPoint.reset();
    repaint();
}

void MultibandGraph::timerCallback()
{
    if (analyzerReader != nullptr)
        analyzerReader(analyzerFrame);

    updateSpectrum();
    repaint();
}

juce::Rectangle<float> MultibandGraph::graphBounds() const
{
    return getLocalBounds().toFloat().reduced(10.0f, 8.0f);
}

std::array<float, crossoverCount> MultibandGraph::readCrossoverFrequencies() const
{
    auto frequencies = defaultCrossoverFrequencies;
    auto previous = minFrequency;

    for (int index = 0; index < crossoverCount; ++index)
    {
        const auto requested = parameterValue(crossoverFrequencyIds[static_cast<size_t>(index)], defaultCrossoverFrequencies[static_cast<size_t>(index)]);
        const auto remaining = static_cast<float>(crossoverCount - index - 1);
        const auto lowLimit = previous + minimumCrossoverGap;
        const auto highLimit = maxFrequency - remaining * minimumCrossoverGap;
        frequencies[static_cast<size_t>(index)] = juce::jlimit(lowLimit, juce::jmax(lowLimit, highLimit), requested);
        previous = frequencies[static_cast<size_t>(index)];
    }

    return frequencies;
}

int MultibandGraph::bandIndexForX(float x) const
{
    const auto graph = graphBounds();
    const auto frequency = xToFrequency(x, graph);
    const auto crossovers = readCrossoverFrequencies();

    for (int index = 0; index < crossoverCount; ++index)
        if (frequency < crossovers[static_cast<size_t>(index)])
            return index;

    return maxBands - 1;
}

int MultibandGraph::crossoverHandleAt(float x) const
{
    const auto graph = graphBounds();
    const auto crossovers = readCrossoverFrequencies();

    for (int index = 0; index < crossoverCount; ++index)
    {
        const auto handleX = frequencyToX(crossovers[static_cast<size_t>(index)], graph);
        if (std::abs(x - handleX) <= 8.0f)
            return index;
    }

    return -1;
}

void MultibandGraph::setSelectedBand(int zeroBasedIndex)
{
    const auto clamped = juce::jlimit(0, maxBands - 1, zeroBasedIndex);
    if (selectedBand == clamped)
        return;

    selectedBand = clamped;
    if (onSelectedBandChanged != nullptr)
        onSelectedBandChanged(selectedBand);
    repaint();
}

void MultibandGraph::updateSpectrum()
{
    updateSpectrumLane(analyzerFrame.input, inputSpectrum);
    updateSpectrumLane(analyzerFrame.output, outputSpectrum);
}

void MultibandGraph::updateSpectrumLane(const std::array<float, MultibandAnalyzerFrame::sampleCount>& samples,
                                        std::array<float, analyzerBinCount>& destination)
{
    fftData.fill(0.0f);
    const auto start = MultibandAnalyzerFrame::sampleCount - fftSize;
    for (int index = 0; index < fftSize; ++index)
        fftData[static_cast<size_t>(index)] = samples[static_cast<size_t>(start + index)];

    window.multiplyWithWindowingTable(fftData.data(), fftSize);
    fft.performFrequencyOnlyForwardTransform(fftData.data());

    const auto frameSampleRate = analyzerFrame.sampleRate > 0.0 ? analyzerFrame.sampleRate : 44100.0;

    for (int bin = 0; bin < analyzerBinCount; ++bin)
    {
        const auto frequency = analyzerFrequencyForBin(bin);
        const auto sourceBin = juce::jlimit(1, fftSize / 2 - 1, static_cast<int>(std::round(frequency * static_cast<float>(fftSize) / static_cast<float>(frameSampleRate))));
        const auto magnitude = fftData[static_cast<size_t>(sourceBin)] / static_cast<float>(fftSize);
        const auto decibels = juce::Decibels::gainToDecibels(magnitude, -100.0f);
        destination[static_cast<size_t>(bin)] = destination[static_cast<size_t>(bin)] * 0.82f + decibels * 0.18f;
    }
}

void MultibandGraph::drawGrid(juce::Graphics& g, juce::Rectangle<float> graph) const
{
    const std::array<float, 10> majorFrequencies {{
        20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f
    }};

    for (float frequency = 30.0f; frequency < maxFrequency; frequency *= 10.0f)
    {
        for (int multiplier = 2; multiplier < 10; ++multiplier)
        {
            const auto value = frequency * static_cast<float>(multiplier);
            if (value > maxFrequency)
                break;

            const auto x = frequencyToX(value, graph);
            g.setColour(fineGridColour());
            g.drawVerticalLine(static_cast<int>(std::round(x)), graph.getY(), graph.getBottom());
        }
    }

    for (const auto frequency : majorFrequencies)
    {
        const auto x = frequencyToX(frequency, graph);
        g.setColour(gridColour());
        g.drawVerticalLine(static_cast<int>(std::round(x)), graph.getY(), graph.getBottom());
        g.setColour(juce::Colour::fromRGB(132, 121, 103));
        g.setFont(juce::FontOptions(10.5f));
        g.drawText(frequencyLabel(frequency),
                   juce::Rectangle<float>(x - 24.0f, graph.getBottom() - 18.0f, 48.0f, 14.0f).toNearestInt(),
                   juce::Justification::centred);
    }

    const std::array<float, 7> decibels {{ -90.0f, -72.0f, -54.0f, -36.0f, -18.0f, -9.0f, 0.0f }};
    for (const auto value : decibels)
    {
        const auto y = juce::jmap(value, -96.0f, 12.0f, graph.getBottom(), graph.getY());
        g.setColour(value == 0.0f ? accentColour().withAlpha(0.42f) : gridColour());
        g.drawHorizontalLine(static_cast<int>(std::round(y)), graph.getX(), graph.getRight());
    }
}

void MultibandGraph::drawBands(juce::Graphics& g, juce::Rectangle<float> graph) const
{
    const auto crossovers = readCrossoverFrequencies();
    auto left = graph.getX();

    for (int index = 0; index < maxBands; ++index)
    {
        const auto right = index < crossoverCount ? frequencyToX(crossovers[static_cast<size_t>(index)], graph) : graph.getRight();
        const auto enabled = parameterValue(bandEnabledIds[static_cast<size_t>(index)], index < 4 ? 1.0f : 0.0f) >= 0.5f;
        auto colour = bandColour(index);

        g.setColour(colour.withAlpha(enabled ? (index == selectedBand ? 0.23f : 0.13f) : 0.055f));
        g.fillRect(juce::Rectangle<float>(left, graph.getY(), right - left, graph.getHeight()));

        if (index == selectedBand)
        {
            g.setColour(colour.withAlpha(0.72f));
            g.drawRect(juce::Rectangle<float>(left + 1.0f, graph.getY() + 1.0f, right - left - 2.0f, graph.getHeight() - 2.0f), 2.0f);
        }

        g.setColour(colour.withAlpha(enabled ? 0.95f : 0.35f));
        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.drawText(juce::String(index + 1),
                   juce::Rectangle<float>(left + 6.0f, graph.getY() + 8.0f, 24.0f, 18.0f).toNearestInt(),
                   juce::Justification::centredLeft);

        left = right;
    }
}

void MultibandGraph::drawSpectrum(juce::Graphics& g,
                                  juce::Rectangle<float> graph,
                                  const std::array<float, analyzerBinCount>& values,
                                  juce::Colour colour) const
{
    juce::Path path;

    for (int bin = 0; bin < analyzerBinCount; ++bin)
    {
        const auto frequency = analyzerFrequencyForBin(bin);
        const auto x = frequencyToX(frequency, graph);
        const auto y = juce::jmap(juce::jlimit(-96.0f, 12.0f, values[static_cast<size_t>(bin)]), -96.0f, 12.0f, graph.getBottom(), graph.getY());

        if (bin == 0)
            path.startNewSubPath(x, y);
        else
            path.lineTo(x, y);
    }

    g.setColour(colour);
    g.strokePath(path, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void MultibandGraph::drawDynamics(juce::Graphics& g, juce::Rectangle<float> graph) const
{
    const auto crossovers = readCrossoverFrequencies();
    auto left = graph.getX();

    for (int index = 0; index < maxBands; ++index)
    {
        const auto right = index < crossoverCount ? frequencyToX(crossovers[static_cast<size_t>(index)], graph) : graph.getRight();
        const auto movement = analyzerFrame.dynamicGainDb[static_cast<size_t>(index)];
        const auto height = juce::jlimit(-24.0f, 24.0f, movement) / 24.0f * (graph.getHeight() * 0.42f);
        const auto centre = graph.getCentreY();
        const auto rect = height >= 0.0f
                              ? juce::Rectangle<float>(left + 3.0f, centre - height, right - left - 6.0f, height)
                              : juce::Rectangle<float>(left + 3.0f, centre, right - left - 6.0f, -height);

        g.setColour((height >= 0.0f ? juce::Colour::fromRGB(120, 196, 255) : juce::Colour::fromRGB(255, 198, 74)).withAlpha(0.28f));
        g.fillRoundedRectangle(rect, 3.0f);

        if (index == selectedBand)
        {
            g.setColour(accentColour());
            g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
            g.drawText((movement > 0.0f ? "+" : "") + juce::String(movement, 1) + " dB",
                       juce::Rectangle<float>(left + 8.0f, graph.getBottom() - 38.0f, right - left - 16.0f, 18.0f).toNearestInt(),
                       juce::Justification::centred);
        }

        left = right;
    }
}

void MultibandGraph::setCrossoverFrequency(int crossoverIndex, float frequency)
{
    if (state == nullptr || crossoverIndex < 0 || crossoverIndex >= crossoverCount)
        return;

    const auto crossovers = readCrossoverFrequencies();
    const auto lowLimit = crossoverIndex == 0 ? minFrequency + minimumCrossoverGap
                                              : crossovers[static_cast<size_t>(crossoverIndex - 1)] + minimumCrossoverGap;
    const auto highLimit = crossoverIndex == crossoverCount - 1 ? maxFrequency - minimumCrossoverGap
                                                                : crossovers[static_cast<size_t>(crossoverIndex + 1)] - minimumCrossoverGap;
    const auto clamped = juce::jlimit(lowLimit, juce::jmax(lowLimit, highLimit), frequency);

    if (auto* parameter = state->getParameter(crossoverFrequencyIds[static_cast<size_t>(crossoverIndex)]))
        parameter->setValueNotifyingHost(parameter->convertTo0to1(clamped));
}

float MultibandGraph::parameterValue(const juce::String& id, float fallback) const
{
    if (state == nullptr)
        return fallback;

    if (auto* value = state->getRawParameterValue(id))
        return value->load();

    return fallback;
}

float MultibandGraph::frequencyToX(float frequency, juce::Rectangle<float> bounds)
{
    const auto proportion = juce::jmap(std::log10(juce::jlimit(minFrequency, maxFrequency, frequency)),
                                       std::log10(minFrequency),
                                       std::log10(maxFrequency),
                                       0.0f,
                                       1.0f);
    return bounds.getX() + proportion * bounds.getWidth();
}

float MultibandGraph::xToFrequency(float x, juce::Rectangle<float> bounds)
{
    const auto proportion = juce::jlimit(0.0f, 1.0f, (x - bounds.getX()) / juce::jmax(1.0f, bounds.getWidth()));
    return std::pow(10.0f,
                    juce::jmap(proportion,
                               std::log10(minFrequency),
                               std::log10(maxFrequency)));
}

float MultibandGraph::analyzerFrequencyForBin(int binIndex)
{
    const auto proportion = static_cast<float>(binIndex) / static_cast<float>(analyzerBinCount - 1);
    return std::pow(10.0f,
                    juce::jmap(proportion,
                               std::log10(minFrequency),
                               std::log10(maxFrequency)));
}
}
