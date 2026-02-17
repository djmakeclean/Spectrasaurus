#include "Bank.h"
#include <cmath>

Bank::Bank()
{
    reset();
}

void Bank::reset()
{
    delayL.reset();
    delayR.reset();
    panL.reset();
    panR.reset();
    feedbackL.reset();
    feedbackR.reset();

    // Dynamics: PreGain and MaxClip default to 0 dB (Y=1.0), MinGate to -60 dB (Y=0.0)
    preGainL.reset(1.0f);
    preGainR.reset(1.0f);
    minGateL.reset(0.0f);
    minGateR.reset(0.0f);
    maxClipL.reset(1.0f);
    maxClipR.reset(1.0f);

    // Shift/Multiply: default at Y=0.5 (no shift / 1.0x multiply)
    shiftL.reset(0.5f);
    shiftR.reset(0.5f);
    multiplyL.reset(0.5f);
    multiplyR.reset(0.5f);
}

PiecewiseFunction& Bank::getCurve(CurveType type)
{
    switch (type)
    {
        case CurveType::DelayL:     return delayL;
        case CurveType::DelayR:     return delayR;
        case CurveType::PanL:       return panL;
        case CurveType::PanR:       return panR;
        case CurveType::FeedbackL:  return feedbackL;
        case CurveType::FeedbackR:  return feedbackR;
        case CurveType::PreGainL:   return preGainL;
        case CurveType::PreGainR:   return preGainR;
        case CurveType::MinGateL:   return minGateL;
        case CurveType::MinGateR:   return minGateR;
        case CurveType::MaxClipL:   return maxClipL;
        case CurveType::MaxClipR:   return maxClipR;
        case CurveType::ShiftL:     return shiftL;
        case CurveType::ShiftR:     return shiftR;
        case CurveType::MultiplyL:  return multiplyL;
        case CurveType::MultiplyR:  return multiplyR;
        default:                    return delayL;
    }
}

const PiecewiseFunction& Bank::getCurve(CurveType type) const
{
    switch (type)
    {
        case CurveType::DelayL:     return delayL;
        case CurveType::DelayR:     return delayR;
        case CurveType::PanL:       return panL;
        case CurveType::PanR:       return panR;
        case CurveType::FeedbackL:  return feedbackL;
        case CurveType::FeedbackR:  return feedbackR;
        case CurveType::PreGainL:   return preGainL;
        case CurveType::PreGainR:   return preGainR;
        case CurveType::MinGateL:   return minGateL;
        case CurveType::MinGateR:   return minGateR;
        case CurveType::MaxClipL:   return maxClipL;
        case CurveType::MaxClipR:   return maxClipR;
        case CurveType::ShiftL:     return shiftL;
        case CurveType::ShiftR:     return shiftR;
        case CurveType::MultiplyL:  return multiplyL;
        case CurveType::MultiplyR:  return multiplyR;
        default:                    return delayL;
    }
}

float Bank::binToNormalizedFreq(int binIndex, float sampleRate) const
{
    // Frequency of this bin
    float freq = (binIndex * sampleRate) / static_cast<float>(fftSize);

    // Nyquist frequency
    float nyquist = sampleRate / 2.0f;

    // Map frequency to log scale [20 Hz to Nyquist]
    const float minFreq = 20.0f;
    float maxFreq = nyquist;

    if (freq < minFreq)
        return 0.0f;
    if (freq > maxFreq)
        return 1.0f;

    // Logarithmic mapping
    float logMin = std::log10(minFreq);
    float logMax = std::log10(maxFreq);
    float logFreq = std::log10(freq);

    return (logFreq - logMin) / (logMax - logMin);
}

float Bank::evaluateDelay(CurveType curveType, int binIndex, float sampleRate) const
{
    const PiecewiseFunction& curve = getCurve(curveType);

    // Get normalized frequency for this bin
    float normalizedFreq = binToNormalizedFreq(binIndex, sampleRate);

    // Evaluate the curve at this frequency
    float normalizedDelay = curve.evaluate(normalizedFreq);

    // Pick the correct per-channel max
    float maxMs = (curveType == CurveType::DelayL) ? delayMaxTimeMsL : delayMaxTimeMsR;

    // Convert to actual delay time
    bool logScale = (curveType == CurveType::DelayL) ? delayLogScaleL : delayLogScaleR;
    float delayMs;
    if (logScale)
    {
        delayMs = std::pow(maxMs, normalizedDelay);
    }
    else
    {
        delayMs = normalizedDelay * maxMs;
    }

    // Convert to samples
    return (delayMs / 1000.0f) * sampleRate;
}

float Bank::evaluatePan(CurveType curveType, int binIndex) const
{
    const PiecewiseFunction& curve = getCurve(curveType);
    float normalizedFreq = binToNormalizedFreq(binIndex, 48000.0f);
    return curve.evaluate(normalizedFreq);
}

float Bank::evaluateFeedback(CurveType curveType, int binIndex) const
{
    const PiecewiseFunction& curve = getCurve(curveType);
    float normalizedFreq = binToNormalizedFreq(binIndex, 48000.0f);
    float y = curve.evaluate(normalizedFreq);

    if (y <= 0.0f)
        return 0.0f;

    float dB = (y * 66.0f) - 60.0f;
    return std::pow(10.0f, dB / 20.0f);
}

float Bank::evaluateCurveNormalized(CurveType curveType, int binIndex, float sampleRate) const
{
    const PiecewiseFunction& curve = getCurve(curveType);
    float normalizedFreq = binToNormalizedFreq(binIndex, sampleRate);
    return curve.evaluate(normalizedFreq);
}

void Bank::rebuildLUTIfNeeded(int numBins, float sampleRate)
{
    if (numBins > kLUTMaxBins)
        numBins = kLUTMaxBins;

    bool fullRebuild = (std::abs(sampleRate - lutSampleRate) > 0.1f || numBins != lutNumBins);

    if (fullRebuild)
    {
        lutSampleRate = sampleRate;
        lutNumBins = numBins;

        const float minFreq = 20.0f;
        float nyquist = sampleRate / 2.0f;
        lutLogMin = std::log10(minFreq);
        float logMax = std::log10(nyquist);
        float logRange = logMax - lutLogMin;
        lutLogRangeInv = (logRange > 0.0f) ? (1.0f / logRange) : 1.0f;
    }

    float binFreqStep = sampleRate / static_cast<float>(fftSize);

    for (int c = 0; c < 16; ++c)
    {
        auto& curve = getCurve(static_cast<CurveType>(c));
        if (!fullRebuild && curve.version == lutCurveVersions[c])
            continue;

        lutCurveVersions[c] = curve.version;

        for (int bin = 0; bin < numBins; ++bin)
        {
            float freq = bin * binFreqStep;
            float normalizedFreq;
            if (freq < 20.0f)
                normalizedFreq = 0.0f;
            else
                normalizedFreq = (std::log10(freq) - lutLogMin) * lutLogRangeInv;

            normalizedFreq = std::clamp(normalizedFreq, 0.0f, 1.0f);
            curveLUT[c][bin] = curve.evaluate(normalizedFreq);
        }
    }
}

juce::var Bank::toVar() const
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty("delayMaxTimeMsL", delayMaxTimeMsL);
    obj->setProperty("delayMaxTimeMsR", delayMaxTimeMsR);
    obj->setProperty("delayLogScaleL", delayLogScaleL);
    obj->setProperty("delayLogScaleR", delayLogScaleR);
    obj->setProperty("gainDB", gainDB);
    obj->setProperty("softClipThresholdDB", softClipThresholdDB);
    obj->setProperty("panValue", static_cast<double>(panValue));

    auto makeCurveObj = [](const PiecewiseFunction& curve)
    {
        auto* curveObj = new juce::DynamicObject();
        curveObj->setProperty("points", curve.toVar());
        return juce::var(curveObj);
    };

    obj->setProperty("delayL", makeCurveObj(delayL));
    obj->setProperty("delayR", makeCurveObj(delayR));
    obj->setProperty("panL", makeCurveObj(panL));
    obj->setProperty("panR", makeCurveObj(panR));
    obj->setProperty("feedbackL", makeCurveObj(feedbackL));
    obj->setProperty("feedbackR", makeCurveObj(feedbackR));
    obj->setProperty("preGainL", makeCurveObj(preGainL));
    obj->setProperty("preGainR", makeCurveObj(preGainR));
    obj->setProperty("minGateL", makeCurveObj(minGateL));
    obj->setProperty("minGateR", makeCurveObj(minGateR));
    obj->setProperty("maxClipL", makeCurveObj(maxClipL));
    obj->setProperty("maxClipR", makeCurveObj(maxClipR));

    obj->setProperty("shiftL", makeCurveObj(shiftL));
    obj->setProperty("shiftR", makeCurveObj(shiftR));
    obj->setProperty("multiplyL", makeCurveObj(multiplyL));
    obj->setProperty("multiplyR", makeCurveObj(multiplyR));
    obj->setProperty("shiftBeforeMultiply", shiftBeforeMultiply);

    return juce::var(obj);
}

void Bank::fromVar(const juce::var& v)
{
    if (auto* obj = v.getDynamicObject())
    {
        if (obj->hasProperty("delayMaxTimeMsL"))
        {
            delayMaxTimeMsL = static_cast<float>(static_cast<double>(obj->getProperty("delayMaxTimeMsL")));
            delayMaxTimeMsR = static_cast<float>(static_cast<double>(obj->getProperty("delayMaxTimeMsR")));
        }
        else if (obj->hasProperty("delayMaxTimeMs"))
        {
            float val = static_cast<float>(static_cast<double>(obj->getProperty("delayMaxTimeMs")));
            delayMaxTimeMsL = val;
            delayMaxTimeMsR = val;
        }
        if (obj->hasProperty("delayLogScaleL"))
        {
            delayLogScaleL = static_cast<bool>(obj->getProperty("delayLogScaleL"));
            delayLogScaleR = static_cast<bool>(obj->getProperty("delayLogScaleR"));
        }
        else if (obj->hasProperty("delayLogScale"))
        {
            bool val = static_cast<bool>(obj->getProperty("delayLogScale"));
            delayLogScaleL = val;
            delayLogScaleR = val;
        }
        gainDB = static_cast<float>(static_cast<double>(obj->getProperty("gainDB")));
        softClipThresholdDB = static_cast<float>(static_cast<double>(obj->getProperty("softClipThresholdDB")));

        if (obj->hasProperty("panValue"))
            panValue = static_cast<float>(static_cast<double>(obj->getProperty("panValue")));

        auto loadCurve = [](PiecewiseFunction& curve, const juce::var& curveVar)
        {
            if (auto* curveObj = curveVar.getDynamicObject())
                curve.fromVar(curveObj->getProperty("points"));
        };

        loadCurve(delayL, obj->getProperty("delayL"));
        loadCurve(delayR, obj->getProperty("delayR"));
        loadCurve(panL, obj->getProperty("panL"));
        loadCurve(panR, obj->getProperty("panR"));
        loadCurve(feedbackL, obj->getProperty("feedbackL"));
        loadCurve(feedbackR, obj->getProperty("feedbackR"));

        // Dynamics curves (backward compatible - won't exist in old presets)
        if (obj->hasProperty("preGainL"))
        {
            loadCurve(preGainL, obj->getProperty("preGainL"));
            loadCurve(preGainR, obj->getProperty("preGainR"));
            loadCurve(minGateL, obj->getProperty("minGateL"));
            loadCurve(minGateR, obj->getProperty("minGateR"));
            loadCurve(maxClipL, obj->getProperty("maxClipL"));
            loadCurve(maxClipR, obj->getProperty("maxClipR"));
        }

        // Shift/multiply curves (backward compatible)
        if (obj->hasProperty("shiftL"))
        {
            loadCurve(shiftL, obj->getProperty("shiftL"));
            loadCurve(shiftR, obj->getProperty("shiftR"));
            loadCurve(multiplyL, obj->getProperty("multiplyL"));
            loadCurve(multiplyR, obj->getProperty("multiplyR"));
            if (obj->hasProperty("shiftBeforeMultiply"))
                shiftBeforeMultiply = static_cast<bool>(obj->getProperty("shiftBeforeMultiply"));
        }
    }
}
