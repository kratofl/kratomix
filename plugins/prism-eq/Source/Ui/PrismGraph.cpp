#include "PrismGraph.h"

#include "AnalyzerFeatures.h"
#include "Parameters.h"

namespace kratomix::prism
{
namespace
{
constexpr float minFrequency = 20.0f;
constexpr float maxFrequency = 20000.0f;
constexpr float minGainDb = -18.0f;
constexpr float maxGainDb = 18.0f;

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
    return juce::Colour::fromRGB(32, 34, 38);
}

juce::Colour responseColour()
{
    return juce::Colour::fromRGB(241, 194, 124);
}

juce::Colour preAnalyzerColour()
{
    return juce::Colour::fromRGB(74, 142, 218);
}

juce::Colour postAnalyzerColour()
{
    return juce::Colour::fromRGB(95, 211, 186);
}

juce::Colour sidechainAnalyzerColour()
{
    return juce::Colour::fromRGB(215, 103, 183);
}
}

PrismGraph::PrismGraph()
    : fft(fftOrder),
      window(fftSize, juce::dsp::WindowingFunction<float>::hann)
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    setWantsKeyboardFocus(true);
    preSpectrum.fill(-120.0f);
    postSpectrum.fill(-120.0f);
    sidechainSpectrum.fill(-120.0f);
}

void PrismGraph::attachState(juce::AudioProcessorValueTreeState& stateToUse)
{
    state = &stateToUse;
    repaint();
}

void PrismGraph::attachAnalyzerReader(std::function<void(PrismAnalyzerFrame&)> reader)
{
    analyzerReader = std::move(reader);
    startTimerHz(30);
}

void PrismGraph::paint(juce::Graphics& g)
{
    g.fillAll(graphBackground());

    auto graph = graphBounds();
    g.setColour(juce::Colours::black.withAlpha(0.35f));
    g.fillRoundedRectangle(graph.expanded(1.0f), 7.0f);
    g.setColour(juce::Colour::fromRGB(16, 17, 20));
    g.fillRoundedRectangle(graph, 7.0f);

    const std::array<float, 10> majorFrequencies {
        20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f
    };

    for (const auto frequency : majorFrequencies)
    {
        const auto x = frequencyToX(frequency, graph);
        g.setColour(gridColour());
        g.drawVerticalLine(static_cast<int>(std::round(x)), graph.getY(), graph.getBottom());

        g.setColour(juce::Colour::fromRGB(132, 121, 103));
        g.setFont(juce::FontOptions(10.5f));
        const auto label = frequency >= 1000.0f ? juce::String(frequency / 1000.0f, frequency >= 10000.0f ? 0 : 1) + "k"
                                                : juce::String(static_cast<int>(frequency));
        g.drawText(label, juce::Rectangle<float>(x - 24.0f, graph.getBottom() - 18.0f, 48.0f, 14.0f).toNearestInt(), juce::Justification::centred);
    }

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

    for (float gain = minGainDb; gain <= maxGainDb; gain += 6.0f)
    {
        const auto y = gainToY(gain, graph);
        g.setColour(std::abs(gain) < 0.001f ? responseColour().withAlpha(0.55f) : gridColour());
        g.drawHorizontalLine(static_cast<int>(std::round(y)), graph.getX(), graph.getRight());

        g.setColour(juce::Colour::fromRGB(132, 121, 103));
        g.setFont(juce::FontOptions(10.5f));
        g.drawText((gain > 0.0f ? "+" : "") + juce::String(static_cast<int>(gain)),
                   juce::Rectangle<float>(graph.getRight() - 32.0f, y - 7.0f, 28.0f, 14.0f).toNearestInt(),
                   juce::Justification::centredRight);
    }

    const auto analyzerMode = static_cast<int>(std::round(parameterValue("analyzerMode", 3.0f)));
    const auto analyzerRange = parameterValue("analyzerRange", 72.0f);

    if (analyzerMode == 5)
        drawMaskingOverlay(g, graph);

    if (analyzerMode == 0 || analyzerMode == 3 || analyzerMode == 4)
        drawAnalyzerLane(g, graph, preSpectrum, preAnalyzerColour().withAlpha(0.45f), analyzerRange);
    if (analyzerMode == 1 || analyzerMode == 3 || analyzerMode == 4 || analyzerMode == 5)
        drawAnalyzerLane(g, graph, postSpectrum, postAnalyzerColour().withAlpha(0.48f), analyzerRange);
    if (analyzerMode == 2 || analyzerMode == 4 || analyzerMode == 5)
        drawAnalyzerLane(g, graph, sidechainSpectrum, sidechainAnalyzerColour().withAlpha(analyzerFrame.sidechainActive ? 0.52f : 0.18f), analyzerRange);

    const auto drawResponse = [this, &g, graph](bool includeDynamicGain, juce::Colour colour, float thickness)
    {
        juce::Path responsePath;
        const auto steps = juce::jmax(24, static_cast<int>(graph.getWidth()));
        for (int step = 0; step <= steps; ++step)
        {
            const auto proportion = static_cast<float>(step) / static_cast<float>(steps);
            const auto x = graph.getX() + proportion * graph.getWidth();
            const auto frequency = xToFrequency(x, graph);
            const auto y = gainToY(responseGainAt(frequency, includeDynamicGain), graph);

            if (step == 0)
                responsePath.startNewSubPath(x, y);
            else
                responsePath.lineTo(x, y);
        }

        g.setColour(colour);
        g.strokePath(responsePath, juce::PathStrokeType(thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };

    if (hasDynamicBands())
        drawResponse(false, responseColour().withAlpha(0.36f), 1.4f);
    drawResponse(true, responseColour(), 2.6f);

    if (state != nullptr)
    {
        for (int index = 1; index <= maxBands; ++index)
        {
            if (! bandEnabled(index))
                continue;

            const auto point = bandPoint(index);
            const auto selected = index == selectedBand;
            const auto radius = selected ? 7.0f : 5.5f;
            const auto prefix = bandPrefix(index);
            const auto dynamicEnabled = parameterValue(prefix + "DynamicEnabled", 0.0f) >= 0.5f;
            const auto dynamicGain = analyzerFrame.dynamicGainDb[static_cast<size_t>(index - 1)];

            g.setColour(responseColour().withAlpha(selected ? 0.32f : 0.18f));
            g.fillEllipse(point.x - radius - 5.0f, point.y - radius - 5.0f, (radius + 5.0f) * 2.0f, (radius + 5.0f) * 2.0f);

            if (dynamicEnabled)
            {
                const auto currentGain = parameterValue(prefix + "Gain", 0.0f) + dynamicGain;
                const auto currentPoint = pointForFrequencyAndGain(parameterValue(prefix + "Frequency", 1000.0f), currentGain);
                g.setColour(juce::Colour::fromRGB(95, 211, 186).withAlpha(0.55f));
                g.drawLine(point.x, point.y, currentPoint.x, currentPoint.y, 1.4f);
                g.fillEllipse(currentPoint.x - 3.5f, currentPoint.y - 3.5f, 7.0f, 7.0f);
            }

            g.setColour(selected ? responseColour() : juce::Colour::fromRGB(95, 211, 186));
            g.fillEllipse(point.x - radius, point.y - radius, radius * 2.0f, radius * 2.0f);

            g.setColour(juce::Colour::fromRGB(10, 10, 11));
            g.drawEllipse(point.x - radius, point.y - radius, radius * 2.0f, radius * 2.0f, 1.5f);
        }
    }

    g.setColour(juce::Colour::fromRGB(71, 61, 47));
    g.drawRoundedRectangle(graph, 7.0f, 1.0f);

    drawHoverReadout(g, graph, analyzerMode);
}

void PrismGraph::mouseDoubleClick(const juce::MouseEvent& event)
{
    selectOrCreateBandAt(event.position);
}

void PrismGraph::mouseDown(const juce::MouseEvent& event)
{
    grabKeyboardFocus();

    if (event.mods.isPopupMenu())
        selectBandAt(event.position);
    else if (! selectBandAt(event.position))
        clearSelection();
}

void PrismGraph::mouseDrag(const juce::MouseEvent& event)
{
    dragSelectedBandTo(event.position);
}

void PrismGraph::mouseMove(const juce::MouseEvent& event)
{
    const auto graph = graphBounds();
    if (graph.contains(event.position))
        hoverPoint = event.position;
    else
        hoverPoint.reset();

    repaint();
}

void PrismGraph::mouseExit(const juce::MouseEvent&)
{
    hoverPoint.reset();
    repaint();
}

bool PrismGraph::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress(juce::KeyPress::deleteKey)
        || key == juce::KeyPress(juce::KeyPress::backspaceKey))
        return deleteSelectedBand();

    return false;
}

void PrismGraph::timerCallback()
{
    updateSpectrum();
}

bool PrismGraph::createBandAt(juce::Point<float> point)
{
    if (state == nullptr)
        return false;

    const auto graph = graphBounds();
    if (! graph.contains(point))
        return false;

    for (int index = 1; index <= maxBands; ++index)
    {
        if (bandEnabled(index))
            continue;

        const auto prefix = bandPrefix(index);
        setSelectedBand(index);
        setParameterValue(prefix + "Enabled", 1.0f);
        setParameterValue(prefix + "Frequency", xToFrequency(point.x, graph));
        setParameterValue(prefix + "Gain", yToGain(point.y, graph));
        setParameterValue(prefix + "Q", 1.0f);
        repaint();
        return true;
    }

    return false;
}

bool PrismGraph::selectBandAt(juce::Point<float> point)
{
    if (state == nullptr)
        return false;

    int closestBand = 0;
    auto closestDistance = std::numeric_limits<float>::max();

    for (int index = 1; index <= maxBands; ++index)
    {
        if (! bandEnabled(index))
            continue;

        const auto distance = point.getDistanceFrom(bandPoint(index));
        if (distance < closestDistance)
        {
            closestDistance = distance;
            closestBand = index;
        }
    }

    if (closestBand == 0 || closestDistance > 14.0f)
        return false;

    setSelectedBand(closestBand);
    return true;
}

bool PrismGraph::selectOrCreateBandAt(juce::Point<float> point)
{
    if (selectBandAt(point))
        return true;

    return createBandAt(point);
}

void PrismGraph::dragSelectedBandTo(juce::Point<float> point)
{
    if (state == nullptr || selectedBand <= 0 || ! bandEnabled(selectedBand))
        return;

    const auto graph = graphBounds();
    const auto constrained = juce::Point<float>(
        juce::jlimit(graph.getX(), graph.getRight(), point.x),
        juce::jlimit(graph.getY(), graph.getBottom(), point.y));
    const auto prefix = bandPrefix(selectedBand);

    setParameterValue(prefix + "Frequency", xToFrequency(constrained.x, graph));
    setParameterValue(prefix + "Gain", yToGain(constrained.y, graph));
    repaint();
}

bool PrismGraph::deleteSelectedBand()
{
    if (state == nullptr || selectedBand <= 0 || ! bandEnabled(selectedBand))
        return false;

    setParameterValue(bandPrefix(selectedBand) + "Enabled", 0.0f);
    clearSelection();
    repaint();
    return true;
}

void PrismGraph::clearSelection()
{
    setSelectedBand(0);
}

juce::Point<float> PrismGraph::pointForFrequencyAndGain(float frequency, float gainDb) const
{
    const auto graph = graphBounds();
    return { frequencyToX(frequency, graph), gainToY(gainDb, graph) };
}

juce::Rectangle<float> PrismGraph::graphBounds() const
{
    return getLocalBounds().toFloat().reduced(18.0f, 14.0f);
}

float PrismGraph::frequencyToX(float frequency, juce::Rectangle<float> bounds)
{
    const auto normalized = (std::log10(frequency) - std::log10(minFrequency))
                            / (std::log10(maxFrequency) - std::log10(minFrequency));
    return bounds.getX() + juce::jlimit(0.0f, 1.0f, normalized) * bounds.getWidth();
}

float PrismGraph::xToFrequency(float x, juce::Rectangle<float> bounds)
{
    const auto normalized = juce::jlimit(0.0f, 1.0f, (x - bounds.getX()) / juce::jmax(1.0f, bounds.getWidth()));
    const auto logFrequency = juce::jmap(normalized, std::log10(minFrequency), std::log10(maxFrequency));
    return std::pow(10.0f, logFrequency);
}

float PrismGraph::gainToY(float gainDb, juce::Rectangle<float> bounds)
{
    const auto normalized = juce::jmap(juce::jlimit(minGainDb, maxGainDb, gainDb), minGainDb, maxGainDb, 1.0f, 0.0f);
    return bounds.getY() + normalized * bounds.getHeight();
}

float PrismGraph::yToGain(float y, juce::Rectangle<float> bounds)
{
    const auto normalized = juce::jlimit(0.0f, 1.0f, (y - bounds.getY()) / juce::jmax(1.0f, bounds.getHeight()));
    return juce::jmap(normalized, 1.0f, 0.0f, minGainDb, maxGainDb);
}

void PrismGraph::setSelectedBand(int oneBasedIndex)
{
    selectedBand = oneBasedIndex;

    if (onSelectedBandChanged)
        onSelectedBandChanged(selectedBand);

    repaint();
}

void PrismGraph::updateSpectrum()
{
    if (! analyzerReader)
        return;

    analyzerReader(analyzerFrame);
    updateSpectrumLane(analyzerFrame.pre, preSpectrum);
    updateSpectrumLane(analyzerFrame.post, postSpectrum);
    updateSpectrumLane(analyzerFrame.sidechain, sidechainSpectrum);
    repaint();
}

void PrismGraph::updateSpectrumLane(const std::array<float, PrismAnalyzerFrame::sampleCount>& samples,
                                    std::array<float, analyzerBinCount>& destination)
{
    std::fill(fftData.begin(), fftData.end(), 0.0f);

    const auto copyCount = juce::jmin(fftSize, PrismAnalyzerFrame::sampleCount);
    for (int index = 0; index < copyCount; ++index)
        fftData[static_cast<size_t>(index)] = samples[static_cast<size_t>(PrismAnalyzerFrame::sampleCount - copyCount + index)];

    window.multiplyWithWindowingTable(fftData.data(), fftSize);
    fft.performFrequencyOnlyForwardTransform(fftData.data());

    const auto nyquist = static_cast<float>(juce::jmax(1.0, analyzerFrame.sampleRate * 0.5));
    const auto speed = parameterValue("analyzerSpeed", 0.5f);
    const auto rise = juce::jmap(speed, 0.0f, 1.0f, 0.12f, 0.55f);
    const auto fall = juce::jmap(speed, 0.0f, 1.0f, 0.04f, 0.22f);

    for (int index = 0; index < analyzerBinCount; ++index)
    {
        const auto proportion = static_cast<float>(index) / static_cast<float>(analyzerBinCount - 1);
        const auto frequency = std::pow(10.0f, juce::jmap(proportion, std::log10(minFrequency), std::log10(maxFrequency)));
        const auto fftPosition = juce::jlimit(1.0f,
                                              static_cast<float>(fftSize / 2 - 1),
                                              frequency / nyquist * static_cast<float>(fftSize / 2));
        const auto lowerIndex = static_cast<int>(std::floor(fftPosition));
        const auto upperIndex = juce::jmin(fftSize / 2, lowerIndex + 1);
        const auto fraction = fftPosition - static_cast<float>(lowerIndex);
        const auto lowerMagnitude = fftData[static_cast<size_t>(lowerIndex)];
        const auto upperMagnitude = fftData[static_cast<size_t>(upperIndex)];
        const auto magnitude = (lowerMagnitude + (upperMagnitude - lowerMagnitude) * fraction) / static_cast<float>(fftSize);
        const auto currentDb = juce::Decibels::gainToDecibels(magnitude, -120.0f);
        auto& smoothedDb = destination[static_cast<size_t>(index)];
        const auto coefficient = currentDb > smoothedDb ? rise : fall;
        smoothedDb += (currentDb - smoothedDb) * coefficient;
    }
}

void PrismGraph::drawAnalyzerLane(juce::Graphics& g,
                                  juce::Rectangle<float> bounds,
                                  const std::array<float, analyzerBinCount>& values,
                                  juce::Colour colour,
                                  float rangeDb) const
{
    juce::Path line;
    juce::Path fill;
    const auto bottom = bounds.getBottom();
    const auto clampedRange = juce::jlimit(24.0f, 120.0f, rangeDb);

    for (int index = 0; index < analyzerBinCount; ++index)
    {
        const auto proportion = static_cast<float>(index) / static_cast<float>(analyzerBinCount - 1);
        const auto frequency = std::pow(10.0f, juce::jmap(proportion, std::log10(minFrequency), std::log10(maxFrequency)));
        const auto x = frequencyToX(frequency, bounds);
        const auto db = juce::jlimit(-clampedRange, 0.0f, values[static_cast<size_t>(index)]);
        const auto y = juce::jmap(db, -clampedRange, 0.0f, bottom, bounds.getY());

        if (index == 0)
        {
            line.startNewSubPath(x, y);
            fill.startNewSubPath(x, bottom);
            fill.lineTo(x, y);
        }
        else
        {
            line.lineTo(x, y);
            fill.lineTo(x, y);
        }
    }

    fill.lineTo(bounds.getRight(), bottom);
    fill.closeSubPath();

    g.setColour(colour.withAlpha(colour.getFloatAlpha() * 0.26f));
    g.fillPath(fill);
    g.setColour(colour);
    g.strokePath(line, juce::PathStrokeType(1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void PrismGraph::drawMaskingOverlay(juce::Graphics& g, juce::Rectangle<float> bounds) const
{
    if (! analyzerFrame.sidechainActive)
        return;

    const auto mask = computeMaskingBins(postSpectrum, sidechainSpectrum, -72.0f, 9.0f);
    const auto baseColour = juce::Colour::fromRGB(219, 86, 122);

    for (int index = 0; index < analyzerBinCount; ++index)
    {
        const auto amount = mask[static_cast<size_t>(index)];
        if (amount < 0.03f)
            continue;

        const auto frequency = frequencyForAnalyzerBin(index);
        const auto x = frequencyToX(frequency, bounds);
        const auto width = juce::jmax(2.0f, bounds.getWidth() / static_cast<float>(analyzerBinCount) * 0.82f);
        g.setColour(baseColour.withAlpha(0.10f + amount * 0.30f));
        g.fillRect(juce::Rectangle<float>(x - width * 0.5f, bounds.getY(), width, bounds.getHeight()));
    }
}

void PrismGraph::drawHoverReadout(juce::Graphics& g, juce::Rectangle<float> bounds, int analyzerMode) const
{
    if (! hoverPoint.has_value() || ! bounds.contains(*hoverPoint))
        return;

    const auto cursorFrequency = xToFrequency(hoverPoint->x, bounds);
    const auto preferredBin = analyzerBinForFrequency(cursorFrequency);
    const auto floorDb = -72.0f;

    const auto* spectrum = &postSpectrum;
    if (analyzerMode == 0)
        spectrum = &preSpectrum;
    else if (analyzerMode == 2)
        spectrum = &sidechainSpectrum;

    auto peak = findNearestAnalyzerPeak(*spectrum, static_cast<size_t>(preferredBin), floorDb);
    if (! peak.has_value() && analyzerMode == 5 && analyzerFrame.sidechainActive)
        peak = findNearestAnalyzerPeak(sidechainSpectrum, static_cast<size_t>(preferredBin), floorDb);

    const auto cursorLabel = cursorFrequency >= 1000.0f ? juce::String(cursorFrequency / 1000.0f, 2) + " kHz"
                                                        : juce::String(cursorFrequency, 0) + " Hz";

    juce::String peakLabel = "No peak";
    float markerX = hoverPoint->x;
    if (peak.has_value())
    {
        const auto peakFrequency = frequencyForAnalyzerBin(static_cast<int>(peak->index));
        markerX = frequencyToX(peakFrequency, bounds);
        peakLabel = (peakFrequency >= 1000.0f ? juce::String(peakFrequency / 1000.0f, 2) + " kHz"
                                              : juce::String(peakFrequency, 0) + " Hz")
                    + "  " + juce::String(peak->levelDb, 1) + " dB";
    }

    g.setColour(responseColour().withAlpha(0.45f));
    g.drawVerticalLine(static_cast<int>(std::round(hoverPoint->x)), bounds.getY(), bounds.getBottom());

    if (peak.has_value())
    {
        g.setColour(juce::Colour::fromRGB(245, 222, 185).withAlpha(0.58f));
        g.drawVerticalLine(static_cast<int>(std::round(markerX)), bounds.getY(), bounds.getBottom());
    }

    auto labelBounds = juce::Rectangle<float>(hoverPoint->x + 12.0f, hoverPoint->y - 38.0f, 154.0f, 38.0f);
    if (labelBounds.getRight() > bounds.getRight() - 6.0f)
        labelBounds.setX(hoverPoint->x - labelBounds.getWidth() - 12.0f);
    if (labelBounds.getY() < bounds.getY() + 6.0f)
        labelBounds.setY(hoverPoint->y + 12.0f);

    g.setColour(juce::Colour::fromRGB(9, 10, 12).withAlpha(0.92f));
    g.fillRoundedRectangle(labelBounds, 5.0f);
    g.setColour(juce::Colour::fromRGB(70, 61, 47));
    g.drawRoundedRectangle(labelBounds, 5.0f, 1.0f);

    g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    g.setColour(responseColour());
    g.drawText(cursorLabel, labelBounds.removeFromTop(18.0f).reduced(7.0f, 1.0f).toNearestInt(), juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(10.0f));
    g.setColour(juce::Colours::white.withAlpha(0.82f));
    g.drawText(peakLabel, labelBounds.reduced(7.0f, 1.0f).toNearestInt(), juce::Justification::centredLeft);
}

float PrismGraph::frequencyForAnalyzerBin(int binIndex)
{
    const auto proportion = static_cast<float>(juce::jlimit(0, analyzerBinCount - 1, binIndex))
                            / static_cast<float>(analyzerBinCount - 1);
    return std::pow(10.0f, juce::jmap(proportion, std::log10(minFrequency), std::log10(maxFrequency)));
}

int PrismGraph::analyzerBinForFrequency(float frequency)
{
    const auto normalized = (std::log10(juce::jlimit(minFrequency, maxFrequency, frequency)) - std::log10(minFrequency))
                            / (std::log10(maxFrequency) - std::log10(minFrequency));
    return juce::jlimit(0, analyzerBinCount - 1, static_cast<int>(std::round(normalized * static_cast<float>(analyzerBinCount - 1))));
}

void PrismGraph::setParameterValue(const juce::String& id, float plainValue)
{
    if (state == nullptr)
        return;

    if (auto* parameter = state->getParameter(id))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
        parameter->endChangeGesture();
    }
}

float PrismGraph::parameterValue(const juce::String& id, float fallback) const
{
    if (state == nullptr)
        return fallback;

    if (auto* value = state->getRawParameterValue(id))
        return value->load();

    return fallback;
}

bool PrismGraph::bandEnabled(int oneBasedIndex) const
{
    return parameterValue(bandPrefix(oneBasedIndex) + "Enabled", 0.0f) >= 0.5f;
}

juce::Point<float> PrismGraph::bandPoint(int oneBasedIndex) const
{
    const auto prefix = bandPrefix(oneBasedIndex);
    return pointForFrequencyAndGain(parameterValue(prefix + "Frequency", 1000.0f),
                                    parameterValue(prefix + "Gain", 0.0f));
}

bool PrismGraph::hasDynamicBands() const
{
    if (state == nullptr)
        return false;

    for (int index = 1; index <= maxBands; ++index)
        if (bandEnabled(index) && parameterValue(bandPrefix(index) + "DynamicEnabled", 0.0f) >= 0.5f)
            return true;

    return false;
}

float PrismGraph::responseGainAt(float frequency, bool includeDynamicGain) const
{
    if (state == nullptr)
        return 0.0f;

    auto totalGain = 0.0f;

    for (int index = 1; index <= maxBands; ++index)
    {
        if (! bandEnabled(index))
            continue;

        const auto prefix = bandPrefix(index);
        const auto bandFrequency = parameterValue(prefix + "Frequency", 1000.0f);
        auto bandGain = parameterValue(prefix + "Gain", 0.0f);
        if (includeDynamicGain && parameterValue(prefix + "DynamicEnabled", 0.0f) >= 0.5f)
            bandGain += analyzerFrame.dynamicGainDb[static_cast<size_t>(index - 1)];
        const auto q = juce::jmax(0.1f, parameterValue(prefix + "Q", 1.0f));
        const auto type = static_cast<BandType>(juce::jlimit(
            0,
            static_cast<int>(BandType::notch),
            static_cast<int>(std::round(parameterValue(prefix + "Type", 0.0f)))));

        const auto octaveDistance = std::log2(juce::jmax(20.0f, frequency) / juce::jmax(20.0f, bandFrequency));
        const auto bellWeight = std::exp(-0.5f * std::pow(octaveDistance * q * 1.4f, 2.0f));

        switch (type)
        {
            case BandType::lowShelf:
                totalGain += bandGain / (1.0f + std::pow(frequency / bandFrequency, q * 2.0f));
                break;
            case BandType::highShelf:
                totalGain += bandGain / (1.0f + std::pow(bandFrequency / frequency, q * 2.0f));
                break;
            case BandType::highPass:
                totalGain += frequency < bandFrequency ? -18.0f * (1.0f - frequency / bandFrequency) : 0.0f;
                break;
            case BandType::lowPass:
                totalGain += frequency > bandFrequency ? -18.0f * (1.0f - bandFrequency / frequency) : 0.0f;
                break;
            case BandType::notch:
                totalGain -= 18.0f * bellWeight;
                break;
            case BandType::bell:
            default:
                totalGain += bandGain * bellWeight;
                break;
        }
    }

    return juce::jlimit(minGainDb, maxGainDb, totalGain);
}
}
