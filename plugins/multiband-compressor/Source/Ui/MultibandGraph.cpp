#include "MultibandGraph.h"

namespace kratomix::multiband
{
namespace
{
constexpr float minFrequency = 20.0f;
constexpr float maxFrequency = 20000.0f;
constexpr float defaultWidthOctaves = 2.0f;

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
    setMouseCursor(juce::MouseCursor::CrosshairCursor);
    setWantsKeyboardFocus(true);
    inputSpectrum.fill(-100.0f);
    outputSpectrum.fill(-100.0f);
}

MultibandGraph::~MultibandGraph()
{
    endDragGesture();
}

void MultibandGraph::attachState(juce::AudioProcessorValueTreeState& stateToUse)
{
    endDragGesture();
    state = &stateToUse;
    selectedBand = -1;

    for (int index = 0; index < maxBands; ++index)
    {
        if (bandEnabled(index))
        {
            selectedBand = index;
            break;
        }
    }

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

    drawGrid(g, graph);

    const auto analyzerMode = static_cast<int>(std::round(parameterValue(analyzerModeId, static_cast<float>(AnalyzerMode::inputOutput))));
    if (analyzerMode == static_cast<int>(AnalyzerMode::input)
        || analyzerMode == static_cast<int>(AnalyzerMode::inputOutput))
        drawSpectrum(g, graph, inputSpectrum, juce::Colour::fromRGB(74, 142, 218).withAlpha(0.46f));

    if (analyzerMode == static_cast<int>(AnalyzerMode::output)
        || analyzerMode == static_cast<int>(AnalyzerMode::inputOutput)
        || analyzerMode == static_cast<int>(AnalyzerMode::gainReduction))
        drawSpectrum(g, graph, outputSpectrum, juce::Colour::fromRGB(95, 211, 186).withAlpha(0.55f));

    drawBands(g, graph);
    drawDynamics(g, graph);

    g.setColour(juce::Colour::fromRGB(71, 61, 47));
    g.drawRoundedRectangle(graph, 8.0f, 1.0f);

    const auto hasAnyBand = std::any_of(bandEnabledIds.begin(), bandEnabledIds.end(), [this](const auto* id)
    {
        return parameterValue(id, 0.0f) >= 0.5f;
    });

    if (! hasAnyBand)
    {
        g.setColour(juce::Colour::fromRGB(250, 231, 202).withAlpha(0.72f));
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        g.drawText("DOUBLE-CLICK TO ADD A DYNAMIC BAND", graph.toNearestInt(), juce::Justification::centred);
    }
    else if (hoverPoint.has_value() && graph.contains(*hoverPoint))
    {
        const auto frequency = xToFrequency(hoverPoint->x, graph);
        g.setColour(juce::Colour::fromRGB(250, 231, 202));
        g.setFont(juce::FontOptions(11.0f));
        g.drawText(frequencyLabel(frequency) + " Hz",
                   juce::Rectangle<float>(hoverPoint->x - 38.0f, graph.getY() + 8.0f, 76.0f, 16.0f).toNearestInt(),
                   juce::Justification::centred);
    }
}

void MultibandGraph::mouseDoubleClick(const juce::MouseEvent& event)
{
    if (! selectBandAt(event.position))
        createBandAt(event.position);
}

void MultibandGraph::mouseDown(const juce::MouseEvent& event)
{
    endDragGesture();
    grabKeyboardFocus();
    const auto graph = graphBounds();
    if (! graph.contains(event.position))
        return;

    if (! selectBandAt(event.position))
    {
        setSelectedBand(-1);
        return;
    }

    dragMode = dragModeAt(event.position, selectedBand);
    updateMouseCursor(event.position);
    if (dragMode == DragMode::none)
        return;

    dragStartPosition = event.position;
    dragStartFrequency = bandFrequency(selectedBand);
    dragStartThreshold = bandThreshold(selectedBand);
    beginDragGesture();
}

void MultibandGraph::mouseDrag(const juce::MouseEvent& event)
{
    if (selectedBand < 0 || dragMode == DragMode::none)
        return;

    if (dragMode == DragMode::body)
    {
        const auto graph = graphBounds();
        const auto startX = frequencyToX(dragStartFrequency, graph);
        setSelectedBandFrequency(xToFrequency(startX + event.position.x - dragStartPosition.x, graph));
        return;
    }

    if (dragMode == DragMode::threshold)
    {
        const auto graph = graphBounds();
        const auto startY = thresholdToY(dragStartThreshold, graph);
        setSelectedBandThreshold(yToThreshold(startY + event.position.y - dragStartPosition.y, graph));
        return;
    }

    const auto centre = bandFrequency(selectedBand);
    const auto edge = xToFrequency(event.position.x, graphBounds());
    const auto width = 2.0f * std::abs(std::log2(juce::jmax(1.0e-3f, edge / centre)));
    setSelectedBandWidth(width);
}

void MultibandGraph::mouseUp(const juce::MouseEvent& event)
{
    endDragGesture();
    updateMouseCursor(event.position);
}

void MultibandGraph::mouseMove(const juce::MouseEvent& event)
{
    hoverPoint = graphBounds().contains(event.position) ? std::optional<juce::Point<float>>(event.position) : std::nullopt;
    updateMouseCursor(event.position);
    repaint();
}

void MultibandGraph::mouseExit(const juce::MouseEvent&)
{
    hoverPoint.reset();
    setMouseCursor(juce::MouseCursor::NormalCursor);
    repaint();
}

void MultibandGraph::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (selectedBand >= 0 && boundsForBand(selectedBand).contains(event.position))
        setSelectedBandWidth(bandWidth(selectedBand) + wheel.deltaY * 0.75f);
}

bool MultibandGraph::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress(juce::KeyPress::deleteKey)
        || key == juce::KeyPress(juce::KeyPress::backspaceKey))
        return deleteSelectedBand();

    return false;
}

bool MultibandGraph::createBandAt(juce::Point<float> point)
{
    if (state == nullptr || ! graphBounds().contains(point))
        return false;

    for (int index = 0; index < maxBands; ++index)
    {
        if (bandEnabled(index))
            continue;

        setParameterValue(bandFrequencyIds[static_cast<size_t>(index)], xToFrequency(point.x, graphBounds()));
        setParameterValue(bandWidthIds[static_cast<size_t>(index)], defaultWidthOctaves);
        setParameterValue(bandEnabledIds[static_cast<size_t>(index)], 1.0f);
        setSelectedBand(index);
        return true;
    }

    return false;
}

bool MultibandGraph::selectBandAt(juce::Point<float> point)
{
    const auto hitBand = bandAt(point);
    if (hitBand < 0)
        return false;

    setSelectedBand(hitBand);
    return true;
}

bool MultibandGraph::deleteSelectedBand()
{
    if (selectedBand < 0 || ! bandEnabled(selectedBand))
        return false;

    setParameterValue(bandEnabledIds[static_cast<size_t>(selectedBand)], 0.0f);

    auto nextBand = -1;
    for (int index = 0; index < maxBands; ++index)
        if (bandEnabled(index))
        {
            nextBand = index;
            break;
        }

    setSelectedBand(nextBand);
    return true;
}

void MultibandGraph::setSelectedBandFrequency(float frequency)
{
    if (selectedBand < 0)
        return;

    setParameterValue(bandFrequencyIds[static_cast<size_t>(selectedBand)], juce::jlimit(minFrequency, maxFrequency, frequency));
    repaint();
}

void MultibandGraph::setSelectedBandWidth(float widthOctaves)
{
    if (selectedBand < 0)
        return;

    setParameterValue(bandWidthIds[static_cast<size_t>(selectedBand)], juce::jlimit(0.25f, 6.0f, widthOctaves));
    repaint();
}

void MultibandGraph::setSelectedBandThreshold(float thresholdDb)
{
    if (selectedBand < 0)
        return;

    setParameterValue(bandThresholdIds[static_cast<size_t>(selectedBand)], juce::jlimit(-90.0f, 0.0f, thresholdDb));
    repaint();
}

juce::Rectangle<float> MultibandGraph::boundsForBand(int zeroBasedIndex) const
{
    if (zeroBasedIndex < 0 || zeroBasedIndex >= maxBands)
        return {};

    const auto graph = graphBounds();
    const auto centre = bandFrequency(zeroBasedIndex);
    const auto ratio = std::pow(2.0f, bandWidth(zeroBasedIndex) * 0.5f);
    const auto low = juce::jlimit(minFrequency, maxFrequency, centre / ratio);
    const auto high = juce::jlimit(minFrequency, maxFrequency, centre * ratio);
    const auto left = frequencyToX(low, graph);
    const auto right = frequencyToX(high, graph);
    return { left, graph.getY(), juce::jmax(2.0f, right - left), graph.getHeight() };
}

float MultibandGraph::thresholdYForBand(int zeroBasedIndex) const
{
    if (zeroBasedIndex < 0 || zeroBasedIndex >= maxBands)
        return graphBounds().getCentreY();

    return thresholdToY(bandThreshold(zeroBasedIndex), graphBounds());
}

void MultibandGraph::timerCallback()
{
    if (analyzerReader)
    {
        analyzerReader(analyzerFrame);
        updateSpectrum();
        repaint();
    }
}

juce::Rectangle<float> MultibandGraph::graphBounds() const
{
    return getLocalBounds().toFloat().reduced(18.0f, 14.0f);
}

int MultibandGraph::bandAt(juce::Point<float> point) const
{
    auto nearestBand = -1;
    auto nearestDistance = std::numeric_limits<float>::max();

    for (int index = 0; index < maxBands; ++index)
    {
        if (! bandEnabled(index) || ! boundsForBand(index).expanded(8.0f, 0.0f).contains(point))
            continue;

        const auto distance = std::abs(point.x - frequencyToX(bandFrequency(index), graphBounds()));
        if (distance < nearestDistance)
        {
            nearestDistance = distance;
            nearestBand = index;
        }
    }

    return nearestBand;
}

MultibandGraph::DragMode MultibandGraph::dragModeAt(juce::Point<float> point, int zeroBasedBandIndex) const
{
    if (zeroBasedBandIndex < 0 || ! bandEnabled(zeroBasedBandIndex))
        return DragMode::none;

    const auto bounds = boundsForBand(zeroBasedBandIndex);
    const auto centreX = frequencyToX(bandFrequency(zeroBasedBandIndex), graphBounds());
    const auto thresholdY = thresholdYForBand(zeroBasedBandIndex);

    if (std::abs(point.x - centreX) <= 12.0f && std::abs(point.y - thresholdY) <= 12.0f)
        return DragMode::threshold;

    if (zeroBasedBandIndex == selectedBand && std::abs(point.y - thresholdY) <= 18.0f)
    {
        if (std::abs(point.x - bounds.getX()) <= 8.0f)
            return DragMode::leftEdge;
        if (std::abs(point.x - bounds.getRight()) <= 8.0f)
            return DragMode::rightEdge;
    }

    return bounds.contains(point) ? DragMode::body : DragMode::none;
}

void MultibandGraph::updateMouseCursor(juce::Point<float> point)
{
    const auto hitBand = bandAt(point);
    switch (dragModeAt(point, hitBand))
    {
        case DragMode::threshold:
            setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
            break;
        case DragMode::leftEdge:
        case DragMode::rightEdge:
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
            break;
        case DragMode::body:
            setMouseCursor(juce::MouseCursor::DraggingHandCursor);
            break;
        case DragMode::none:
            setMouseCursor(graphBounds().contains(point) ? juce::MouseCursor::CrosshairCursor
                                                         : juce::MouseCursor::NormalCursor);
            break;
    }
}

void MultibandGraph::setSelectedBand(int zeroBasedIndex)
{
    const auto clamped = zeroBasedIndex < 0 ? -1 : juce::jlimit(0, maxBands - 1, zeroBasedIndex);
    if (selectedBand == clamped)
        return;

    endDragGesture();
    selectedBand = clamped;
    if (onSelectedBandChanged)
        onSelectedBandChanged(selectedBand);
    repaint();
}

void MultibandGraph::beginDragGesture()
{
    if (state == nullptr || selectedBand < 0)
        return;

    const auto index = static_cast<size_t>(selectedBand);
    if (dragMode == DragMode::body)
        draggedParameters[0] = state->getParameter(bandFrequencyIds[index]);
    else if (dragMode == DragMode::threshold)
        draggedParameters[0] = state->getParameter(bandThresholdIds[index]);
    else if (dragMode == DragMode::leftEdge || dragMode == DragMode::rightEdge)
        draggedParameters[0] = state->getParameter(bandWidthIds[index]);

    for (auto* parameter : draggedParameters)
        if (parameter != nullptr)
            parameter->beginChangeGesture();
}

void MultibandGraph::endDragGesture()
{
    for (auto*& parameter : draggedParameters)
    {
        if (parameter != nullptr)
            parameter->endChangeGesture();
        parameter = nullptr;
    }
    dragMode = DragMode::none;
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
        for (int multiplier = 2; multiplier < 10; ++multiplier)
        {
            const auto value = frequency * static_cast<float>(multiplier);
            if (value > maxFrequency)
                break;
            g.setColour(fineGridColour());
            g.drawVerticalLine(static_cast<int>(std::round(frequencyToX(value, graph))), graph.getY(), graph.getBottom());
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

void MultibandGraph::drawBands(juce::Graphics& g, juce::Rectangle<float>) const
{
    for (int index = 0; index < maxBands; ++index)
    {
        if (! bandEnabled(index))
            continue;

        const auto bounds = boundsForBand(index);
        const auto colour = bandColour(index);
        const auto selected = index == selectedBand;
        g.setColour(colour.withAlpha(selected ? 0.24f : 0.12f));
        g.fillRoundedRectangle(bounds.reduced(1.0f, 2.0f), 6.0f);
        g.setColour(colour.withAlpha(selected ? 0.95f : 0.52f));
        g.drawRoundedRectangle(bounds.reduced(1.0f, 2.0f), 6.0f, selected ? 2.0f : 1.0f);

        const auto centreX = frequencyToX(bandFrequency(index), graphBounds());
        const auto thresholdY = thresholdYForBand(index);
        g.drawHorizontalLine(static_cast<int>(std::round(thresholdY)), bounds.getX() + 3.0f, bounds.getRight() - 3.0f);
        g.fillEllipse(centreX - 7.0f, thresholdY - 7.0f, 14.0f, 14.0f);
        if (selected)
        {
            g.fillRoundedRectangle(bounds.getX() - 3.0f, thresholdY - 14.0f, 6.0f, 28.0f, 2.0f);
            g.fillRoundedRectangle(bounds.getRight() - 3.0f, thresholdY - 14.0f, 6.0f, 28.0f, 2.0f);
            g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
            g.drawText(juce::String(bandThreshold(index), 1) + " dB",
                       juce::Rectangle<float>(centreX + 10.0f, thresholdY - 10.0f, 66.0f, 18.0f).toNearestInt(),
                       juce::Justification::centredLeft);
        }

        if (bounds.getWidth() >= 48.0f)
        {
            g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
            g.drawText(juce::String(index + 1) + "  " + frequencyLabel(bandFrequency(index)) + " Hz",
                       juce::Rectangle<float>(bounds.getX() + 6.0f, bounds.getY() + 8.0f, bounds.getWidth() - 12.0f, 18.0f).toNearestInt(),
                       juce::Justification::centred);
        }
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
        const auto x = frequencyToX(analyzerFrequencyForBin(bin), graph);
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
    for (int index = 0; index < maxBands; ++index)
    {
        if (! bandEnabled(index))
            continue;

        const auto bounds = boundsForBand(index).reduced(4.0f, 0.0f);
        const auto movement = analyzerFrame.dynamicGainDb[static_cast<size_t>(index)];
        const auto height = juce::jlimit(-24.0f, 24.0f, movement) / 24.0f * (graph.getHeight() * 0.42f);
        const auto centre = graph.getCentreY();
        const auto rect = height >= 0.0f
                              ? juce::Rectangle<float>(bounds.getX(), centre - height, bounds.getWidth(), height)
                              : juce::Rectangle<float>(bounds.getX(), centre, bounds.getWidth(), -height);
        g.setColour((height >= 0.0f ? juce::Colour::fromRGB(120, 196, 255) : juce::Colour::fromRGB(255, 198, 74)).withAlpha(0.34f));
        g.fillRoundedRectangle(rect, 3.0f);

        if (index == selectedBand)
        {
            g.setColour(accentColour());
            g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
            g.drawText((movement > 0.0f ? "+" : "") + juce::String(movement, 1) + " dB",
                       juce::Rectangle<float>(bounds.getX(), graph.getBottom() - 38.0f, bounds.getWidth(), 18.0f).toNearestInt(),
                       juce::Justification::centred);
        }
    }
}

float MultibandGraph::parameterValue(const juce::String& id, float fallback) const
{
    if (state != nullptr)
        if (auto* value = state->getRawParameterValue(id))
            return value->load();
    return fallback;
}

void MultibandGraph::setParameterValue(const juce::String& id, float plainValue)
{
    if (state == nullptr)
        return;
    if (auto* parameter = state->getParameter(id))
    {
        const auto normalized = parameter->convertTo0to1(plainValue);
        if (std::abs(parameter->getValue() - normalized) < 1.0e-6f)
            return;
        const auto isDragged = std::find(draggedParameters.begin(), draggedParameters.end(), parameter) != draggedParameters.end();
        if (! isDragged)
            parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(normalized);
        if (! isDragged)
            parameter->endChangeGesture();
    }
}

bool MultibandGraph::bandEnabled(int zeroBasedIndex) const
{
    return zeroBasedIndex >= 0 && zeroBasedIndex < maxBands
           && parameterValue(bandEnabledIds[static_cast<size_t>(zeroBasedIndex)], 0.0f) >= 0.5f;
}

float MultibandGraph::bandFrequency(int zeroBasedIndex) const
{
    return parameterValue(bandFrequencyIds[static_cast<size_t>(zeroBasedIndex)], defaultBandFrequencies[static_cast<size_t>(zeroBasedIndex)]);
}

float MultibandGraph::bandWidth(int zeroBasedIndex) const
{
    return parameterValue(bandWidthIds[static_cast<size_t>(zeroBasedIndex)], defaultWidthOctaves);
}

float MultibandGraph::bandThreshold(int zeroBasedIndex) const
{
    return parameterValue(bandThresholdIds[static_cast<size_t>(zeroBasedIndex)], -24.0f);
}

float MultibandGraph::thresholdToY(float thresholdDb, juce::Rectangle<float> bounds)
{
    return juce::jmap(juce::jlimit(-90.0f, 0.0f, thresholdDb), -96.0f, 12.0f, bounds.getBottom(), bounds.getY());
}

float MultibandGraph::yToThreshold(float y, juce::Rectangle<float> bounds)
{
    return juce::jlimit(-90.0f, 0.0f, juce::jmap(y, bounds.getBottom(), bounds.getY(), -96.0f, 12.0f));
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
    return std::pow(10.0f, juce::jmap(proportion, std::log10(minFrequency), std::log10(maxFrequency)));
}

float MultibandGraph::analyzerFrequencyForBin(int binIndex)
{
    const auto proportion = static_cast<float>(binIndex) / static_cast<float>(analyzerBinCount - 1);
    return std::pow(10.0f, juce::jmap(proportion, std::log10(minFrequency), std::log10(maxFrequency)));
}
}
