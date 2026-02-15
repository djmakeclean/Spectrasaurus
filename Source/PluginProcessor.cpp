#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "DebugLogger.h"
#include <complex>

SpectrasaurusAudioProcessor::SpectrasaurusAudioProcessor()
     : AudioProcessor (BusesProperties()
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
       parameters(*this, nullptr, "PARAMETERS",
                  {
                      std::make_unique<juce::AudioParameterFloat>(
                          "morphX",
                          "Morph X",
                          juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                          0.0f),
                      std::make_unique<juce::AudioParameterFloat>(
                          "morphY",
                          "Morph Y",
                          juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f),
                          0.0f)
                  })
{
}

SpectrasaurusAudioProcessor::~SpectrasaurusAudioProcessor()
{
}

const juce::String SpectrasaurusAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool SpectrasaurusAudioProcessor::acceptsMidi() const
{
    return false;
}

bool SpectrasaurusAudioProcessor::producesMidi() const
{
    return false;
}

bool SpectrasaurusAudioProcessor::isMidiEffect() const
{
    return false;
}

double SpectrasaurusAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int SpectrasaurusAudioProcessor::getNumPrograms()
{
    return 1;
}

int SpectrasaurusAudioProcessor::getCurrentProgram()
{
    return 0;
}

void SpectrasaurusAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String SpectrasaurusAudioProcessor::getProgramName (int index)
{
    return {};
}

void SpectrasaurusAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

void SpectrasaurusAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    DEBUG_LOG("=== prepareToPlay called ===");
    DEBUG_LOG("Sample rate: ", sampleRate);
    DEBUG_LOG("Samples per block: ", samplesPerBlock);

    currentSampleRate = sampleRate;
    currentFFTSize = banks[0].fftSize;
    currentOverlapFactor = banks[0].overlapFactor;
    maxDelaySamples = static_cast<int>(sampleRate * 1.0);

    DEBUG_LOG("FFT size: ", currentFFTSize);
    DEBUG_LOG("Overlap factor: ", currentOverlapFactor);
    DEBUG_LOG("Max delay samples: ", maxDelaySamples);

    int fftOrder = static_cast<int>(std::log2(currentFFTSize));
    fft = std::make_unique<juce::dsp::FFT>(fftOrder);
    window = std::make_unique<juce::dsp::WindowingFunction<float>>(
        static_cast<size_t>(currentFFTSize),
        juce::dsp::WindowingFunction<float>::hann);

    // Allocate buffers - output needs to be larger for overlap-add
    int hopSize = currentFFTSize / currentOverlapFactor;
    inputBuffer.setSize(2, currentFFTSize);
    outputBuffer.setSize(2, currentFFTSize * 2); // 2x size for proper overlap-add
    fftBuffer.setSize(2, currentFFTSize * 2);

    inputBuffer.clear();
    outputBuffer.clear();
    fftBuffer.clear();

    leftFFTData.resize(currentFFTSize * 2, 0.0f);
    rightFFTData.resize(currentFFTSize * 2, 0.0f);

    inputBufferWritePos = 0;
    outputBufferReadPos = 0;
    // Start writing ahead of reading by one FFT size
    outputBufferWritePos = currentFFTSize;

    // Allocate delay buffers (sized in frames, not samples)
    int numBins = currentFFTSize / 2;
    int maxDelayFrames = maxDelaySamples / hopSize;

    leftBinDelayBuffers.clear();
    rightBinDelayBuffers.clear();
    leftBinDelayWritePos.clear();
    rightBinDelayWritePos.clear();

    // Allocate feedback buffers
    feedbackLeftReal.assign(numBins, 0.0f);
    feedbackLeftImag.assign(numBins, 0.0f);
    feedbackRightReal.assign(numBins, 0.0f);
    feedbackRightImag.assign(numBins, 0.0f);

    DEBUG_LOG("Allocating delay buffers for ", numBins, " bins, ", maxDelayFrames, " frames each");

    for (int i = 0; i < numBins; ++i)
    {
        // Each bin buffer stores: channel 0 = real, channel 1 = imag
        leftBinDelayBuffers.emplace_back(2, maxDelayFrames);
        rightBinDelayBuffers.emplace_back(2, maxDelayFrames);
        leftBinDelayBuffers[i].clear();
        rightBinDelayBuffers[i].clear();
        leftBinDelayWritePos.push_back(0);
        rightBinDelayWritePos.push_back(0);
    }

    DEBUG_LOG("=== prepareToPlay completed ===");
}

void SpectrasaurusAudioProcessor::releaseResources()
{
}

bool SpectrasaurusAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

// Removed old FIFO-based processing functions

void SpectrasaurusAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any extra output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    if (totalNumInputChannels < 2 || totalNumOutputChannels < 2)
        return;

    static int blockCounter = 0;
    blockCounter++;

    // Log first few blocks and then occasionally
    if (blockCounter <= 5 || blockCounter % 100 == 0)
    {
        DEBUG_LOG("processBlock #", blockCounter, " - samples: ", buffer.getNumSamples());
        float maxL = buffer.getMagnitude(0, 0, buffer.getNumSamples());
        float maxR = buffer.getMagnitude(1, 0, buffer.getNumSamples());
        DEBUG_LOG("  Input levels - L: ", maxL, " R: ", maxR);
    }

    int hopSize = currentFFTSize / currentOverlapFactor;
    int numSamples = buffer.getNumSamples();

    float maxOutputL = 0.0f;
    float maxOutputR = 0.0f;

    // Compute bank gain/clip from morph interpolation (once per block)
    float mx = getMorphX();
    float my = getMorphY();
    float wA = (1.0f - mx) * (1.0f - my);
    float wB = mx * (1.0f - my);
    float wC = (1.0f - mx) * my;
    float wD = mx * my;

    float bankGainDB = wA * banks[0].gainDB + wB * banks[1].gainDB +
                       wC * banks[2].gainDB + wD * banks[3].gainDB;
    float bankGain = juce::Decibels::decibelsToGain(bankGainDB);

    float bankClipDB = wA * banks[0].softClipThresholdDB + wB * banks[1].softClipThresholdDB +
                       wC * banks[2].softClipThresholdDB + wD * banks[3].softClipThresholdDB;
    float bankClipT = juce::Decibels::decibelsToGain(bankClipDB);
    bool doBankClip = bankClipDB < -0.01f;

    // Compute per-bank pan (equal-power, morphed)
    float bankPan = wA * banks[0].panValue + wB * banks[1].panValue +
                    wC * banks[2].panValue + wD * banks[3].panValue;
    // Convert pan (-1..+1) to equal-power gains, normalized so center = unity
    float panAngle = (bankPan + 1.0f) * 0.5f * static_cast<float>(M_PI) * 0.5f;
    float panGainL = std::cos(panAngle) * std::sqrt(2.0f);
    float panGainR = std::sin(panAngle) * std::sqrt(2.0f);

    // Compute master gain/clip once per block
    float mGain = juce::Decibels::decibelsToGain(masterGainDB.load());
    float mClipDB = masterClipDB.load();
    float mClipT = juce::Decibels::decibelsToGain(mClipDB);
    bool doMasterClip = mClipDB < -0.01f;

    // Dry/wet mix (0 = dry, 1 = wet)
    float dryWet = masterDryWet.load();

    for (int sampleIdx = 0; sampleIdx < numSamples; ++sampleIdx)
    {
        // Write input samples to buffer
        inputBuffer.setSample(0, inputBufferWritePos, buffer.getSample(0, sampleIdx));
        inputBuffer.setSample(1, inputBufferWritePos, buffer.getSample(1, sampleIdx));

        // Read output samples from buffer (post overlap-add)
        float sampleL = outputBuffer.getSample(0, outputBufferReadPos);
        float sampleR = outputBuffer.getSample(1, outputBufferReadPos);

        // Apply bank gain (morphed)
        sampleL *= bankGain;
        sampleR *= bankGain;

        // Apply bank soft clip (operates on final summed signal, not per-frame)
        if (doBankClip)
        {
            sampleL = bankClipT * std::tanh(sampleL / bankClipT);
            sampleR = bankClipT * std::tanh(sampleR / bankClipT);
        }

        // Apply per-bank pan (equal-power)
        sampleL *= panGainL;
        sampleR *= panGainR;

        // Apply master gain
        sampleL *= mGain;
        sampleR *= mGain;

        // Apply master soft clip (tanh)
        if (doMasterClip)
        {
            sampleL = mClipT * std::tanh(sampleL / mClipT);
            sampleR = mClipT * std::tanh(sampleR / mClipT);
        }

        // Dry/wet mix: blend processed (wet) with original dry signal
        if (dryWet < 1.0f)
        {
            float dryL = buffer.getSample(0, sampleIdx);
            float dryR = buffer.getSample(1, sampleIdx);
            sampleL = dryL + dryWet * (sampleL - dryL);
            sampleR = dryR + dryWet * (sampleR - dryR);
        }

        buffer.setSample(0, sampleIdx, sampleL);
        buffer.setSample(1, sampleIdx, sampleR);

        // Track peak levels for metering
        maxOutputL = std::max(maxOutputL, std::abs(sampleL));
        maxOutputR = std::max(maxOutputR, std::abs(sampleR));

        // Clear the read position
        outputBuffer.setSample(0, outputBufferReadPos, 0.0f);
        outputBuffer.setSample(1, outputBufferReadPos, 0.0f);

        inputBufferWritePos++;
        outputBufferReadPos = (outputBufferReadPos + 1) % outputBuffer.getNumSamples();

        // Process when we have filled the entire FFT window
        if (inputBufferWritePos >= currentFFTSize)
        {
            processFFTFrame();

            // Shift input buffer left by hopSize samples
            for (int i = 0; i < currentFFTSize - hopSize; ++i)
            {
                inputBuffer.setSample(0, i, inputBuffer.getSample(0, i + hopSize));
                inputBuffer.setSample(1, i, inputBuffer.getSample(1, i + hopSize));
            }
            // Clear the newly available space at the end
            for (int i = currentFFTSize - hopSize; i < currentFFTSize; ++i)
            {
                inputBuffer.setSample(0, i, 0.0f);
                inputBuffer.setSample(1, i, 0.0f);
            }

            // Reset write position to where new samples should go
            inputBufferWritePos = currentFFTSize - hopSize;
        }
    }

    // Update level meters (smoothed peak decay)
    float smoothing = 0.3f;
    outputLevelL = outputLevelL.load() * (1.0f - smoothing) + maxOutputL * smoothing;
    outputLevelR = outputLevelR.load() * (1.0f - smoothing) + maxOutputR * smoothing;
}

// Removed old FIFO-based processStereoFFT function

bool SpectrasaurusAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* SpectrasaurusAudioProcessor::createEditor()
{
    return new SpectrasaurusAudioProcessorEditor (*this);
}

void SpectrasaurusAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto* root = new juce::DynamicObject();
    root->setProperty("spectrasaurus_version", "1.0");

    juce::Array<juce::var> banksArray;
    for (int i = 0; i < 4; ++i)
        banksArray.add(banks[i].toVar());
    root->setProperty("banks", juce::var(banksArray));

    // Save morph parameters
    root->setProperty("morphX", static_cast<double>(getMorphX()));
    root->setProperty("morphY", static_cast<double>(getMorphY()));

    // Save active bank and master controls
    root->setProperty("activeBankIndex", activeBankIndex.load());
    root->setProperty("masterGainDB", static_cast<double>(masterGainDB.load()));
    root->setProperty("masterClipDB", static_cast<double>(masterClipDB.load()));
    root->setProperty("masterDryWet", static_cast<double>(masterDryWet.load()));
    root->setProperty("notesText", notesText);

    // UI view state
    root->setProperty("dynamicsLCurveIndex", dynamicsLCurveIndex);
    root->setProperty("dynamicsRCurveIndex", dynamicsRCurveIndex);
    root->setProperty("shiftLCurveIndex", shiftLCurveIndex);
    root->setProperty("shiftRCurveIndex", shiftRCurveIndex);

    auto json = juce::JSON::toString(juce::var(root));
    destData.append(json.toRawUTF8(), json.getNumBytesAsUTF8());
}

void SpectrasaurusAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto json = juce::String::fromUTF8(static_cast<const char*>(data), sizeInBytes);
    auto parsed = juce::JSON::parse(json);

    if (auto* root = parsed.getDynamicObject())
    {
        auto banksVar = root->getProperty("banks");
        if (auto* banksArray = banksVar.getArray())
        {
            int count = std::min(static_cast<int>(banksArray->size()), 4);
            for (int i = 0; i < count; ++i)
                banks[i].fromVar((*banksArray)[i]);
        }

        if (root->hasProperty("morphX"))
        {
            if (auto* param = parameters.getParameter("morphX"))
                param->setValueNotifyingHost(static_cast<float>(static_cast<double>(root->getProperty("morphX"))));
        }
        if (root->hasProperty("morphY"))
        {
            if (auto* param = parameters.getParameter("morphY"))
                param->setValueNotifyingHost(static_cast<float>(static_cast<double>(root->getProperty("morphY"))));
        }

        // Restore active bank and master controls
        if (root->hasProperty("activeBankIndex"))
            activeBankIndex.store(static_cast<int>(root->getProperty("activeBankIndex")));
        if (root->hasProperty("masterGainDB"))
            masterGainDB.store(static_cast<float>(static_cast<double>(root->getProperty("masterGainDB"))));
        if (root->hasProperty("masterClipDB"))
            masterClipDB.store(static_cast<float>(static_cast<double>(root->getProperty("masterClipDB"))));
        if (root->hasProperty("masterDryWet"))
            masterDryWet.store(static_cast<float>(static_cast<double>(root->getProperty("masterDryWet"))));
        if (root->hasProperty("notesText"))
            notesText = root->getProperty("notesText").toString();

        // UI view state (backward compatible — defaults to 0 if absent)
        if (root->hasProperty("dynamicsLCurveIndex"))
            dynamicsLCurveIndex = static_cast<int>(root->getProperty("dynamicsLCurveIndex"));
        if (root->hasProperty("dynamicsRCurveIndex"))
            dynamicsRCurveIndex = static_cast<int>(root->getProperty("dynamicsRCurveIndex"));
        if (root->hasProperty("shiftLCurveIndex"))
            shiftLCurveIndex = static_cast<int>(root->getProperty("shiftLCurveIndex"));
        if (root->hasProperty("shiftRCurveIndex"))
            shiftRCurveIndex = static_cast<int>(root->getProperty("shiftRCurveIndex"));
    }
}

SpectrasaurusAudioProcessor::BinParameters SpectrasaurusAudioProcessor::evaluateBinParameters(int binIndex)
{
    float x = getMorphX();
    float y = getMorphY();

    static int logCounter = 0;
    bool shouldLogMorph = (logCounter++ % 100 == 0 && binIndex == 10);

    if (shouldLogMorph)
    {
        DEBUG_LOG("=== MORPH DEBUG (frame ", logCounter, ") ===");
        DEBUG_LOG("  X param value: ", x, " Y param value: ", y);
        DEBUG_LOG("  Weights: A=", (1.0f - x) * (1.0f - y), " B=", x * (1.0f - y),
                  " C=", (1.0f - x) * y, " D=", x * y);
    }

    // Bilinear interpolation weights
    // A: top-left (x=0, y=0)
    // B: top-right (x=1, y=0)
    // C: bottom-left (x=0, y=1)
    // D: bottom-right (x=1, y=1)
    float weightA = (1.0f - x) * (1.0f - y);
    float weightB = x * (1.0f - y);
    float weightC = (1.0f - x) * y;
    float weightD = x * y;

    BinParameters params;

    // Step 1: Interpolate NORMALIZED curve values (0-1) from each bank
    float delayLNorm_A = banks[0].evaluateCurveNormalized(CurveType::DelayL, binIndex, currentSampleRate);
    float delayLNorm_B = banks[1].evaluateCurveNormalized(CurveType::DelayL, binIndex, currentSampleRate);
    float delayLNorm_C = banks[2].evaluateCurveNormalized(CurveType::DelayL, binIndex, currentSampleRate);
    float delayLNorm_D = banks[3].evaluateCurveNormalized(CurveType::DelayL, binIndex, currentSampleRate);
    float delayLNorm = weightA * delayLNorm_A + weightB * delayLNorm_B + weightC * delayLNorm_C + weightD * delayLNorm_D;

    float delayRNorm_A = banks[0].evaluateCurveNormalized(CurveType::DelayR, binIndex, currentSampleRate);
    float delayRNorm_B = banks[1].evaluateCurveNormalized(CurveType::DelayR, binIndex, currentSampleRate);
    float delayRNorm_C = banks[2].evaluateCurveNormalized(CurveType::DelayR, binIndex, currentSampleRate);
    float delayRNorm_D = banks[3].evaluateCurveNormalized(CurveType::DelayR, binIndex, currentSampleRate);
    float delayRNorm = weightA * delayRNorm_A + weightB * delayRNorm_B + weightC * delayRNorm_C + weightD * delayRNorm_D;

    float panL_A = banks[0].evaluateCurveNormalized(CurveType::PanL, binIndex, currentSampleRate);
    float panL_B = banks[1].evaluateCurveNormalized(CurveType::PanL, binIndex, currentSampleRate);
    float panL_C = banks[2].evaluateCurveNormalized(CurveType::PanL, binIndex, currentSampleRate);
    float panL_D = banks[3].evaluateCurveNormalized(CurveType::PanL, binIndex, currentSampleRate);
    params.panL = weightA * panL_A + weightB * panL_B + weightC * panL_C + weightD * panL_D;

    float panR_A = banks[0].evaluateCurveNormalized(CurveType::PanR, binIndex, currentSampleRate);
    float panR_B = banks[1].evaluateCurveNormalized(CurveType::PanR, binIndex, currentSampleRate);
    float panR_C = banks[2].evaluateCurveNormalized(CurveType::PanR, binIndex, currentSampleRate);
    float panR_D = banks[3].evaluateCurveNormalized(CurveType::PanR, binIndex, currentSampleRate);
    params.panR = weightA * panR_A + weightB * panR_B + weightC * panR_C + weightD * panR_D;

    // Step 2: Interpolate bank SETTINGS (delayMax per channel, log scale, gain)
    float delayMaxMsL = weightA * banks[0].delayMaxTimeMsL + weightB * banks[1].delayMaxTimeMsL +
                        weightC * banks[2].delayMaxTimeMsL + weightD * banks[3].delayMaxTimeMsL;
    float delayMaxMsR = weightA * banks[0].delayMaxTimeMsR + weightB * banks[1].delayMaxTimeMsR +
                        weightC * banks[2].delayMaxTimeMsR + weightD * banks[3].delayMaxTimeMsR;

    // Per-channel log scale (weighted vote across banks)
    float logScaleWeightL = weightA * (banks[0].delayLogScaleL ? 1.0f : 0.0f) +
                            weightB * (banks[1].delayLogScaleL ? 1.0f : 0.0f) +
                            weightC * (banks[2].delayLogScaleL ? 1.0f : 0.0f) +
                            weightD * (banks[3].delayLogScaleL ? 1.0f : 0.0f);
    float logScaleWeightR = weightA * (banks[0].delayLogScaleR ? 1.0f : 0.0f) +
                            weightB * (banks[1].delayLogScaleR ? 1.0f : 0.0f) +
                            weightC * (banks[2].delayLogScaleR ? 1.0f : 0.0f) +
                            weightD * (banks[3].delayLogScaleR ? 1.0f : 0.0f);
    bool useLogScaleL = logScaleWeightL > 0.5f;
    bool useLogScaleR = logScaleWeightR > 0.5f;

    float gainDB = weightA * banks[0].gainDB + weightB * banks[1].gainDB +
                   weightC * banks[2].gainDB + weightD * banks[3].gainDB;

    // Interpolate feedback curves (normalized 0-1, then convert to linear)
    float fbLNorm_A = banks[0].evaluateCurveNormalized(CurveType::FeedbackL, binIndex, currentSampleRate);
    float fbLNorm_B = banks[1].evaluateCurveNormalized(CurveType::FeedbackL, binIndex, currentSampleRate);
    float fbLNorm_C = banks[2].evaluateCurveNormalized(CurveType::FeedbackL, binIndex, currentSampleRate);
    float fbLNorm_D = banks[3].evaluateCurveNormalized(CurveType::FeedbackL, binIndex, currentSampleRate);
    float fbLNorm = weightA * fbLNorm_A + weightB * fbLNorm_B + weightC * fbLNorm_C + weightD * fbLNorm_D;

    float fbRNorm_A = banks[0].evaluateCurveNormalized(CurveType::FeedbackR, binIndex, currentSampleRate);
    float fbRNorm_B = banks[1].evaluateCurveNormalized(CurveType::FeedbackR, binIndex, currentSampleRate);
    float fbRNorm_C = banks[2].evaluateCurveNormalized(CurveType::FeedbackR, binIndex, currentSampleRate);
    float fbRNorm_D = banks[3].evaluateCurveNormalized(CurveType::FeedbackR, binIndex, currentSampleRate);
    float fbRNorm = weightA * fbRNorm_A + weightB * fbRNorm_B + weightC * fbRNorm_C + weightD * fbRNorm_D;

    // Convert feedback normalized values to linear gain (-60 dB floor)
    if (fbLNorm <= 0.0f)
        params.feedbackL = 0.0f;
    else
        params.feedbackL = std::pow(10.0f, ((fbLNorm * 66.0f) - 60.0f) / 20.0f);

    if (fbRNorm <= 0.0f)
        params.feedbackR = 0.0f;
    else
        params.feedbackR = std::pow(10.0f, ((fbRNorm * 66.0f) - 60.0f) / 20.0f);

    // Step 3: Convert interpolated normalized values to actual delay using interpolated settings
    if (useLogScaleL)
        params.delayL = std::pow(delayMaxMsL, delayLNorm) / 1000.0f * currentSampleRate;
    else
        params.delayL = (delayLNorm * delayMaxMsL) / 1000.0f * currentSampleRate;

    if (useLogScaleR)
        params.delayR = std::pow(delayMaxMsR, delayRNorm) / 1000.0f * currentSampleRate;
    else
        params.delayR = (delayRNorm * delayMaxMsR) / 1000.0f * currentSampleRate;

    // Interpolate dynamics curves (normalized 0-1, then convert to linear via dB = y*60 - 60)
    auto interpolateDynamicsCurve = [&](CurveType ct) -> float
    {
        float a = banks[0].evaluateCurveNormalized(ct, binIndex, currentSampleRate);
        float b = banks[1].evaluateCurveNormalized(ct, binIndex, currentSampleRate);
        float c = banks[2].evaluateCurveNormalized(ct, binIndex, currentSampleRate);
        float d = banks[3].evaluateCurveNormalized(ct, binIndex, currentSampleRate);
        float norm = weightA * a + weightB * b + weightC * c + weightD * d;
        if (norm <= 0.0f) return 0.0f;
        float dB = (norm * 60.0f) - 60.0f;
        return std::pow(10.0f, dB / 20.0f);
    };

    params.preGainL = interpolateDynamicsCurve(CurveType::PreGainL);
    params.preGainR = interpolateDynamicsCurve(CurveType::PreGainR);
    params.minGateL = interpolateDynamicsCurve(CurveType::MinGateL);
    params.minGateR = interpolateDynamicsCurve(CurveType::MinGateR);
    params.maxClipL = interpolateDynamicsCurve(CurveType::MaxClipL);
    params.maxClipR = interpolateDynamicsCurve(CurveType::MaxClipR);

    // Interpolate shift/multiply (keep as normalized 0-1, convert to Hz/factor in processFFTFrame)
    auto interpolateNormCurve = [&](CurveType ct) -> float
    {
        float a = banks[0].evaluateCurveNormalized(ct, binIndex, currentSampleRate);
        float b = banks[1].evaluateCurveNormalized(ct, binIndex, currentSampleRate);
        float c = banks[2].evaluateCurveNormalized(ct, binIndex, currentSampleRate);
        float d = banks[3].evaluateCurveNormalized(ct, binIndex, currentSampleRate);
        return weightA * a + weightB * b + weightC * c + weightD * d;
    };

    params.shiftL = interpolateNormCurve(CurveType::ShiftL);
    params.shiftR = interpolateNormCurve(CurveType::ShiftR);
    params.multiplyL = interpolateNormCurve(CurveType::MultiplyL);
    params.multiplyR = interpolateNormCurve(CurveType::MultiplyR);

    if (shouldLogMorph)
    {
        DEBUG_LOG("  X=", x, " Y=", y);
        DEBUG_LOG("  Normalized curves (bin ", binIndex, "): delayL=", delayLNorm, " delayR=", delayRNorm);
        DEBUG_LOG("  Interpolated settings: maxDelayL=", delayMaxMsL, "ms, maxDelayR=", delayMaxMsR, "ms, gain=", gainDB, "dB");
        DEBUG_LOG("  Final delays: L=", params.delayL, " R=", params.delayR, " samples");
        DEBUG_LOG("  Weights: A=", weightA, " B=", weightB, " C=", weightC, " D=", weightD);
    }

    return params;
}

void SpectrasaurusAudioProcessor::processFFTFrame()
{
    static int frameCounter = 0;
    frameCounter++;
    bool shouldLog = (frameCounter <= 3 || frameCounter % 100 == 0);

    if (shouldLog)
        DEBUG_LOG("=== Processing FFT Frame #", frameCounter, " ===");

    int hopSize = currentFFTSize / currentOverlapFactor;

    // Copy input to FFT buffers and apply window
    for (int i = 0; i < currentFFTSize; ++i)
    {
        leftFFTData[i] = inputBuffer.getSample(0, i);
        rightFFTData[i] = inputBuffer.getSample(1, i);
    }

    if (shouldLog)
    {
        float inputMagL = 0.0f, inputMagR = 0.0f;
        for (int i = 0; i < currentFFTSize; ++i)
        {
            inputMagL = std::max(inputMagL, std::abs(leftFFTData[i]));
            inputMagR = std::max(inputMagR, std::abs(rightFFTData[i]));
        }
        DEBUG_LOG("  Pre-FFT input max - L: ", inputMagL, " R: ", inputMagR);
    }

    // DO NOT window before FFT - we'll window after IFFT for proper COLA

    // Perform FFT
    fft->performRealOnlyForwardTransform(leftFFTData.data());
    fft->performRealOnlyForwardTransform(rightFFTData.data());

    if (shouldLog)
    {
        DEBUG_LOG("  FFT completed, processing bins...");
        // Log first few FFT values to understand the format
        DEBUG_LOG("  Left FFT data[0-10]: ",
                  leftFFTData[0], " ", leftFFTData[1], " ", leftFFTData[2], " ",
                  leftFFTData[3], " ", leftFFTData[4], " ", leftFFTData[5]);
        DEBUG_LOG("  Left FFT data[2040-2047]: ",
                  leftFFTData[2040], " ", leftFFTData[2041], " ", leftFFTData[2042], " ",
                  leftFFTData[2043], " ", leftFFTData[2044], " ", leftFFTData[2045], " ",
                  leftFFTData[2046], " ", leftFFTData[2047]);
        DEBUG_LOG("  currentFFTSize: ", currentFFTSize);
    }

    // Process bins: dynamics, delay, feedback, panning
    int numBins = currentFFTSize / 2;
    int maxDelayFrames = maxDelaySamples / hopSize;
    float halfN = currentFFTSize / 2.0f; // normalization factor for dBFS

    // Minimum delay in frames for feedback to be safe (~1ms)
    int minFeedbackDelayFrames = static_cast<int>((currentSampleRate * 0.001f) / hopSize);
    if (minFeedbackDelayFrames < 1) minFeedbackDelayFrames = 1;

    // Morph weights for per-bin interpolation
    float mx = getMorphX();
    float my = getMorphY();
    float wA = (1.0f - mx) * (1.0f - my);
    float wB = mx * (1.0f - my);
    float wC = (1.0f - mx) * my;
    float wD = mx * my;

    // Shift order: use bank with highest weight
    float maxW = std::max({wA, wB, wC, wD});
    bool shiftBeforeMult = (maxW == wA) ? banks[0].shiftBeforeMultiply :
                           (maxW == wB) ? banks[1].shiftBeforeMultiply :
                           (maxW == wC) ? banks[2].shiftBeforeMultiply :
                                          banks[3].shiftBeforeMultiply;

    // Spectrograph capture buffers
    bool captureSpectrograph = spectrographEnabled.load();
    float localSpecL[kMaxSpectrographBins];
    float localSpecR[kMaxSpectrographBins];

    // ===== PHASE 1: Per-bin feedback + dynamics + spectrograph capture =====
    // Store results in temp arrays for shift/multiply scatter
    std::vector<float> tempLeftReal(numBins, 0.0f);
    std::vector<float> tempLeftImag(numBins, 0.0f);
    std::vector<float> tempRightReal(numBins, 0.0f);
    std::vector<float> tempRightImag(numBins, 0.0f);
    // Store per-bin parameters for phase 3
    std::vector<BinParameters> allParams(numBins);

    for (int bin = 0; bin < numBins; ++bin)
    {
        int realIdx = bin;
        int imagIdx = numBins + bin;

        float leftReal = leftFFTData[realIdx];
        float leftImag = (bin == 0 || bin == numBins) ? 0.0f : leftFFTData[imagIdx];
        float rightReal = rightFFTData[realIdx];
        float rightImag = (bin == 0 || bin == numBins) ? 0.0f : rightFFTData[imagIdx];

        BinParameters params = evaluateBinParameters(bin);
        allParams[bin] = params;

        // Add feedback
        leftReal  += feedbackLeftReal[bin];
        leftImag  += feedbackLeftImag[bin];
        rightReal += feedbackRightReal[bin];
        rightImag += feedbackRightImag[bin];

        // PreGain
        leftReal  *= params.preGainL;
        leftImag  *= params.preGainL;
        rightReal *= params.preGainR;
        rightImag *= params.preGainR;

        // Gate/Clip
        auto applyGateClip = [halfN](float& real, float& imag, float gateThresh, float clipThresh)
        {
            float mag = std::sqrt(real * real + imag * imag);
            float magNorm = mag / halfN;
            if (magNorm < gateThresh)
            {
                real = 0.0f;
                imag = 0.0f;
            }
            else if (magNorm > clipThresh && mag > 0.0f)
            {
                float scale = (clipThresh * halfN) / mag;
                real *= scale;
                imag *= scale;
            }
        };
        applyGateClip(leftReal, leftImag, params.minGateL, params.maxClipL);
        applyGateClip(rightReal, rightImag, params.minGateR, params.maxClipR);

        // Spectrograph capture
        if (captureSpectrograph && bin < kMaxSpectrographBins)
        {
            float magL = std::sqrt(leftReal * leftReal + leftImag * leftImag);
            float magR = std::sqrt(rightReal * rightReal + rightImag * rightImag);
            float magLNorm = magL / halfN;
            float magRNorm = magR / halfN;
            localSpecL[bin] = (magLNorm > 0.0f) ? std::max(-60.0f, 20.0f * std::log10(magLNorm)) : -60.0f;
            localSpecR[bin] = (magRNorm > 0.0f) ? std::max(-60.0f, 20.0f * std::log10(magRNorm)) : -60.0f;
        }

        tempLeftReal[bin] = leftReal;
        tempLeftImag[bin] = leftImag;
        tempRightReal[bin] = rightReal;
        tempRightImag[bin] = rightImag;
    }

    // ===== PHASE 2: Spectral shift/multiply (forward scatter) =====
    std::vector<float> shiftedLeftReal(numBins, 0.0f);
    std::vector<float> shiftedLeftImag(numBins, 0.0f);
    std::vector<float> shiftedRightReal(numBins, 0.0f);
    std::vector<float> shiftedRightImag(numBins, 0.0f);

    float binFreqStep = static_cast<float>(currentSampleRate) / static_cast<float>(currentFFTSize);

    for (int bin = 0; bin < numBins; ++bin)
    {
        float binFreq = bin * binFreqStep;
        auto& params = allParams[bin];

        // Fixed absolute formulas (display ranges are zoom-only, don't affect audio)
        // Shift: Y=0 → -10000Hz, Y=0.5 → 0Hz, Y=1 → +10000Hz
        float shiftHzL = (params.shiftL - 0.5f) * 20000.0f;
        float shiftHzR = (params.shiftR - 0.5f) * 20000.0f;

        // Multiply: Y=0 → 0.1x, Y=0.5 → 1.0x, Y=1 → 10.0x (logarithmic)
        float multFactorL = 0.1f * std::pow(100.0f, params.multiplyL);
        float multFactorR = 0.1f * std::pow(100.0f, params.multiplyR);

        // Compute target frequency based on application order
        float targetFreqL, targetFreqR;
        if (shiftBeforeMult)
        {
            targetFreqL = (binFreq + shiftHzL) * multFactorL;
            targetFreqR = (binFreq + shiftHzR) * multFactorR;
        }
        else
        {
            targetFreqL = binFreq * multFactorL + shiftHzL;
            targetFreqR = binFreq * multFactorR + shiftHzR;
        }

        // Convert target frequency back to bin index (fractional)
        float targetBinL = targetFreqL / binFreqStep;
        float targetBinR = targetFreqR / binFreqStep;

        // Forward scatter with linear interpolation (left channel)
        if (targetBinL >= 0.0f && targetBinL < numBins - 1)
        {
            int lo = static_cast<int>(targetBinL);
            float frac = targetBinL - lo;
            int hi = lo + 1;
            if (lo >= 0 && hi < numBins)
            {
                shiftedLeftReal[lo] += tempLeftReal[bin] * (1.0f - frac);
                shiftedLeftImag[lo] += tempLeftImag[bin] * (1.0f - frac);
                shiftedLeftReal[hi] += tempLeftReal[bin] * frac;
                shiftedLeftImag[hi] += tempLeftImag[bin] * frac;
            }
            else if (lo >= 0 && lo < numBins)
            {
                shiftedLeftReal[lo] += tempLeftReal[bin];
                shiftedLeftImag[lo] += tempLeftImag[bin];
            }
        }
        else if (targetBinL >= 0.0f && targetBinL < numBins)
        {
            int idx = static_cast<int>(targetBinL);
            shiftedLeftReal[idx] += tempLeftReal[bin];
            shiftedLeftImag[idx] += tempLeftImag[bin];
        }

        // Forward scatter (right channel)
        if (targetBinR >= 0.0f && targetBinR < numBins - 1)
        {
            int lo = static_cast<int>(targetBinR);
            float frac = targetBinR - lo;
            int hi = lo + 1;
            if (lo >= 0 && hi < numBins)
            {
                shiftedRightReal[lo] += tempRightReal[bin] * (1.0f - frac);
                shiftedRightImag[lo] += tempRightImag[bin] * (1.0f - frac);
                shiftedRightReal[hi] += tempRightReal[bin] * frac;
                shiftedRightImag[hi] += tempRightImag[bin] * frac;
            }
            else if (lo >= 0 && lo < numBins)
            {
                shiftedRightReal[lo] += tempRightReal[bin];
                shiftedRightImag[lo] += tempRightImag[bin];
            }
        }
        else if (targetBinR >= 0.0f && targetBinR < numBins)
        {
            int idx = static_cast<int>(targetBinR);
            shiftedRightReal[idx] += tempRightReal[bin];
            shiftedRightImag[idx] += tempRightImag[bin];
        }
    }

    // ===== PHASE 3: Per-bin delay + pan + feedback store from shifted arrays =====
    for (int bin = 0; bin < numBins; ++bin)
    {
        int realIdx = bin;
        int imagIdx = numBins + bin;

        float leftReal = shiftedLeftReal[bin];
        float leftImag = shiftedLeftImag[bin];
        float rightReal = shiftedRightReal[bin];
        float rightImag = shiftedRightImag[bin];

        auto& params = allParams[bin];

        // Delay
        int delayL_samples = static_cast<int>(params.delayL);
        int delayR_samples = static_cast<int>(params.delayR);
        int delayL = delayL_samples / hopSize;
        int delayR = delayR_samples / hopSize;
        delayL = std::clamp(delayL, 0, maxDelayFrames - 1);
        delayR = std::clamp(delayR, 0, maxDelayFrames - 1);

        float delayedLeftReal, delayedLeftImag, delayedRightReal, delayedRightImag;

        if (delayL > 0)
        {
            int& wp = leftBinDelayWritePos[bin];
            leftBinDelayBuffers[bin].setSample(0, wp, leftReal);
            leftBinDelayBuffers[bin].setSample(1, wp, leftImag);
            int rp = (wp - delayL + maxDelayFrames) % maxDelayFrames;
            delayedLeftReal = leftBinDelayBuffers[bin].getSample(0, rp);
            delayedLeftImag = leftBinDelayBuffers[bin].getSample(1, rp);
            wp = (wp + 1) % maxDelayFrames;
        }
        else
        {
            delayedLeftReal = leftReal;
            delayedLeftImag = leftImag;
        }

        if (delayR > 0)
        {
            int& wp = rightBinDelayWritePos[bin];
            rightBinDelayBuffers[bin].setSample(0, wp, rightReal);
            rightBinDelayBuffers[bin].setSample(1, wp, rightImag);
            int rp = (wp - delayR + maxDelayFrames) % maxDelayFrames;
            delayedRightReal = rightBinDelayBuffers[bin].getSample(0, rp);
            delayedRightImag = rightBinDelayBuffers[bin].getSample(1, rp);
            wp = (wp + 1) % maxDelayFrames;
        }
        else
        {
            delayedRightReal = rightReal;
            delayedRightImag = rightImag;
        }

        // Pan crossfeed
        float panL = params.panL;
        float panR = params.panR;
        float leftToLeft   = std::cos(panL * M_PI * 0.5f);
        float leftToRight  = std::sin(panL * M_PI * 0.5f);
        float rightToRight = std::cos(panR * M_PI * 0.5f);
        float rightToLeft  = std::sin(panR * M_PI * 0.5f);

        float outputLeftReal  = delayedLeftReal * leftToLeft + delayedRightReal * rightToLeft;
        float outputLeftImag  = delayedLeftImag * leftToLeft + delayedRightImag * rightToLeft;
        float outputRightReal = delayedRightReal * rightToRight + delayedLeftReal * leftToRight;
        float outputRightImag = delayedRightImag * rightToRight + delayedLeftImag * leftToRight;

        // Feedback store
        float fbL = (delayL >= minFeedbackDelayFrames) ? params.feedbackL : 0.0f;
        float fbR = (delayR >= minFeedbackDelayFrames) ? params.feedbackR : 0.0f;

        feedbackLeftReal[bin]  = outputLeftReal * fbL;
        feedbackLeftImag[bin]  = outputLeftImag * fbL;
        feedbackRightReal[bin] = outputRightReal * fbR;
        feedbackRightImag[bin] = outputRightImag * fbR;

        auto sanitize = [](float& v) { if (!std::isfinite(v)) v = 0.0f; };
        sanitize(feedbackLeftReal[bin]);
        sanitize(feedbackLeftImag[bin]);
        sanitize(feedbackRightReal[bin]);
        sanitize(feedbackRightImag[bin]);

        // Write to output FFT buffer
        leftFFTData[realIdx] = outputLeftReal;
        if (bin != 0 && bin != numBins)
            leftFFTData[imagIdx] = outputLeftImag;

        rightFFTData[realIdx] = outputRightReal;
        if (bin != 0 && bin != numBins)
            rightFFTData[imagIdx] = outputRightImag;
    }

    // Write spectrograph data under lock
    if (captureSpectrograph)
    {
        if (spectrographLock.tryEnter())
        {
            spectrographNumBins = std::min(numBins, kMaxSpectrographBins);
            std::memcpy(spectrographDataL, localSpecL, spectrographNumBins * sizeof(float));
            std::memcpy(spectrographDataR, localSpecR, spectrographNumBins * sizeof(float));
            spectrographLock.exit();
        }
    }

    if (shouldLog)
        DEBUG_LOG("  Bin processing completed, performing IFFT...");

    // Perform IFFT
    fft->performRealOnlyInverseTransform(leftFFTData.data());
    fft->performRealOnlyInverseTransform(rightFFTData.data());

    // Apply Hann window AFTER IFFT (synthesis window)
    window->multiplyWithWindowingTable(leftFFTData.data(), currentFFTSize);
    window->multiplyWithWindowingTable(rightFFTData.data(), currentFFTSize);

    // Overlap-add to output buffer
    // Empirically determined scale factor for unity gain with:
    // - Hann window applied after IFFT
    // - 4x overlap (75%)
    // - JUCE FFT (which doesn't normalize)
    // 0.21 was -1.5dB, so multiply by 1.189 to get unity gain
    float scaleFactor = 0.25f;

    if (shouldLog)
        DEBUG_LOG("  Using scale factor: ", scaleFactor);

    if (shouldLog)
    {
        float outputMagL = 0.0f, outputMagR = 0.0f;
        for (int i = 0; i < currentFFTSize; ++i)
        {
            outputMagL = std::max(outputMagL, std::abs(leftFFTData[i] * scaleFactor));
            outputMagR = std::max(outputMagR, std::abs(rightFFTData[i] * scaleFactor));
        }
        DEBUG_LOG("  Post-IFFT output max - L: ", outputMagL, " R: ", outputMagR);
    }

    // Overlap-add to output buffer (gain and clip are applied later in processBlock
    // after all overlapping frames are summed, so they work on the final signal)
    for (int i = 0; i < currentFFTSize; ++i)
    {
        float sL = leftFFTData[i] * scaleFactor;
        float sR = rightFFTData[i] * scaleFactor;

        int outputPos = (outputBufferWritePos + i) % outputBuffer.getNumSamples();
        outputBuffer.setSample(0, outputPos, outputBuffer.getSample(0, outputPos) + sL);
        outputBuffer.setSample(1, outputPos, outputBuffer.getSample(1, outputPos) + sR);
    }

    // Advance write position by hop size
    outputBufferWritePos = (outputBufferWritePos + hopSize) % outputBuffer.getNumSamples();

    if (shouldLog)
        DEBUG_LOG("=== FFT Frame #", frameCounter, " completed ===");
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SpectrasaurusAudioProcessor();
}
