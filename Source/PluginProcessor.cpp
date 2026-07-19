#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
MyGainProcessor::MyGainProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    driveParam  = apvts.getRawParameterValue ("drive");
    oddParam    = apvts.getRawParameterValue ("odd");
    evenParam   = apvts.getRawParameterValue ("even");
    outputParam = apvts.getRawParameterValue ("output");
    osParam     = apvts.getRawParameterValue ("oversample");
    modeParam   = apvts.getRawParameterValue ("mode");
    mixParam    = apvts.getRawParameterValue ("mix");
    analogParam = apvts.getRawParameterValue ("analog");
    toneParam   = apvts.getRawParameterValue ("tone");
}

juce::AudioProcessorValueTreeState::ParameterLayout MyGainProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "drive", 1 }, "Drive",
        juce::NormalisableRange<float> (1.0f, 20.0f, 0.01f, 0.5f), 4.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "odd", 1 }, "Odd Harmonics",
        juce::NormalisableRange<float> (0.0f, 2.0f, 0.001f), 1.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "even", 1 }, "Even Harmonics",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "output", 1 }, "Output",
        juce::NormalisableRange<float> (-24.0f, 6.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "oversample", 1 }, "Oversample (4x)", true));

    // 왜곡 커브 종류
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "mode", 1 }, "Mode",
        juce::StringArray { "Saturate", "Distort", "Fuzz" }, 0));

    // Dry/Wet 믹스 (병렬 새추레이션용)
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "mix", 1 }, "Mix",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));

    // 아날로그 섹션: 동적 바이어스 + Tone
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "analog", 1 }, "Analog", false));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "tone", 1 }, "Tone",
        juce::NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f));

    return layout;
}

//==============================================================================
void MyGainProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        (size_t) getTotalNumOutputChannels(), 2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR);
    oversampling->initProcessing ((size_t) samplesPerBlock);

    lastOsState = osParam->load() > 0.5f;
    setLatencySamples (lastOsState ? (int) oversampling->getLatencyInSamples() : 0);

    const double osRate = sampleRate * 4.0;

    for (auto* sm : { &driveSm, &oddSm, &evenSm, &outSm })
        sm->reset (osRate, 0.05);
    for (auto* sm : { &mixSm, &toneSm })
        sm->reset (sampleRate, 0.05);

    driveSm.setCurrentAndTargetValue (driveParam->load());
    oddSm  .setCurrentAndTargetValue (oddParam->load());
    evenSm .setCurrentAndTargetValue (evenParam->load());
    outSm  .setCurrentAndTargetValue (juce::Decibels::decibelsToGain (outputParam->load()));
    mixSm  .setCurrentAndTargetValue (mixParam->load());
    toneSm .setCurrentAndTargetValue (toneParam->load());

    // 샘플레이트에 맞춘 계수 계산 (하드코딩 0.995의 함정 방지)
    dcCoeff   = (float) std::exp (-juce::MathConstants<double>::twoPi * 20.0 / osRate);   // ~20Hz
    envDecay  = (float) std::exp (-1.0 / (0.05 * osRate));                                // 50ms 릴리즈
    toneCoeff = 1.0f - (float) std::exp (-juce::MathConstants<double>::twoPi * 1800.0 / sampleRate); // 틸트 중심 ~1.8kHz

    const auto numCh = (size_t) getTotalNumOutputChannels();
    dcInE .assign (numCh, 0.0f);  dcOutE.assign (numCh, 0.0f);
    dcInF .assign (numCh, 0.0f);  dcOutF.assign (numCh, 0.0f);
    envFollow.assign (numCh, 0.0f);
    toneLp   .assign (numCh, 0.0f);

    dryBuffer.setSize ((int) numCh, samplesPerBlock);

    // Dry 지연선 (위상 정렬)
    dryDelay.prepare ({ sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) numCh });
    dryDelay.reset();
    dryDelay.setDelay (lastOsState ? oversampling->getLatencyInSamples() : 0.0f);
}

bool MyGainProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainOutputChannelSet() == layouts.getMainInputChannelSet();
}

//==============================================================================
void MyGainProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                    juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals noDenormals;

    for (int i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    driveSm.setTargetValue (driveParam->load());
    oddSm  .setTargetValue (oddParam->load());
    evenSm .setTargetValue (evenParam->load());
    outSm  .setTargetValue (juce::Decibels::decibelsToGain (outputParam->load()));
    mixSm  .setTargetValue (mixParam->load());
    toneSm .setTargetValue (toneParam->load());

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // ── Mix용 dry 신호 보관 ───────────────────────────────
    if (dryBuffer.getNumSamples() < numSamples || dryBuffer.getNumChannels() < numChannels)
        dryBuffer.setSize (numChannels, numSamples, false, false, true); // 안전장치
    for (int ch = 0; ch < numChannels; ++ch)
        dryBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    // 입력 레벨 측정 (VU)
    {
        float lvl = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            lvl = juce::jmax (lvl, buffer.getRMSLevel (ch, 0, numSamples));
        inputLevel.store (lvl);
    }

    // ── 오버샘플링 분기 ───────────────────────────────────
    const bool osOn = osParam->load() > 0.5f;
    if (osOn != lastOsState)
    {
        setLatencySamples (osOn ? (int) oversampling->getLatencyInSamples() : 0);
        dryDelay.setDelay (osOn ? oversampling->getLatencyInSamples() : 0.0f);
        lastOsState = osOn;
    }

    juce::dsp::AudioBlock<float> block (buffer);
    if (osOn)
    {
        auto osBlock = oversampling->processSamplesUp (block);
        processDistortion (osBlock);
        oversampling->processSamplesDown (block);
    }
    else
    {
        processDistortion (block);
    }

    // ── 아날로그 Tone 틸트 + Dry/Wet 믹스 (기본 레이트) ────
    const bool analogOn = analogParam->load() > 0.5f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float mix  = mixSm.getNextValue();
        const float tone = toneSm.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = buffer.getWritePointer (ch);
            float wet   = data[sample];

            if (analogOn)
            {
                // 틸트 EQ: 저역/고역을 시소처럼. tone<0 어둡게, >0 밝게
                float& lp = toneLp[(size_t) ch];
                lp += toneCoeff * (wet - lp);
                wet = lp + (wet - lp) * (1.0f + tone);
            }

            // dry를 wet과 같은 만큼 지연시켜 콤필터링 방지 (위상 정렬)
            dryDelay.pushSample (ch, dryBuffer.getReadPointer (ch)[sample]);
            const float dryAligned = dryDelay.popSample (ch);
            data[sample] = dryAligned * (1.0f - mix) + wet * mix;
        }
    }

    // 출력 레벨 측정 (VU)
    {
        float lvl = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            lvl = juce::jmax (lvl, buffer.getRMSLevel (ch, 0, numSamples));
        outputLevel.store (lvl);
    }
}

//==============================================================================
void MyGainProcessor::processDistortion (juce::dsp::AudioBlock<float>& osBlock)
{
    const int  numSamples  = (int) osBlock.getNumSamples();
    const int  numChannels = (int) osBlock.getNumChannels();
    const int  mode        = (int) modeParam->load();
    const bool analogOn    = analogParam->load() > 0.5f;

    // Fuzz의 고정 바이어스가 만드는 DC를 무음 기준으로 보상
    const float fuzzRest = juce::jlimit (-0.85f, 1.0f, 0.3f) * 0.9f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float drive = driveSm.getNextValue();
        const float odd   = oddSm.getNextValue();
        const float even  = evenSm.getNextValue();
        const float out   = outSm.getNextValue();

        const float oddMix  = juce::jmin (odd, 1.0f);
        const float oddPush = juce::jmax (odd, 1.0f);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = osBlock.getChannelPointer ((size_t) ch);
            const float x = data[sample] * drive;

            // ── 아날로그: 동적 바이어스 ────────────────────
            // 신호가 커지면 동작점이 밀렸다가 50ms에 걸쳐 복귀
            // → 비대칭(짝수 배음)이 연주 강도에 따라 살아 움직임
            float bias = 0.0f;
            if (analogOn)
            {
                float& env = envFollow[(size_t) ch];
                env  = juce::jmax (std::abs (x), env * envDecay);
                bias = env * 0.25f;
            }

            const float in = x * oddPush;

            // ── 커브 선택 ─────────────────────────────────
            float shaped = 0.0f;
            switch (mode)
            {
                case 0: // Saturate: 부드러운 tanh
                    shaped = std::tanh (in + bias) - std::tanh (bias);
                    break;

                case 1: // Distort: 하드 클리핑
                {
                    const float t = (in + bias) * 1.4f;
                    shaped = juce::jlimit (-1.0f, 1.0f, t)
                           - juce::jlimit (-1.0f, 1.0f, bias * 1.4f);
                    break;
                }

                case 2: // Fuzz: 극단 게인 + 비대칭 클립 (사각파에 가깝게)
                {
                    const float t = (in + bias) * 8.0f + 0.3f;
                    shaped = juce::jlimit (-0.85f, 1.0f, t) * 0.9f - fuzzRest;
                    break;
                }
            }

            // ── 짝수 배음 경로 (정류 + DC 제거) ────────────
            const float rectified = std::abs (x);
            const float dcE = rectified - dcInE[(size_t) ch] + dcCoeff * dcOutE[(size_t) ch];
            dcInE[(size_t) ch]  = rectified;
            dcOutE[(size_t) ch] = dcE;
            const float evenPath = std::tanh (dcE);

            // ── 합산 + 최종 DC 제거 ────────────────────────
            const float summed = shaped * oddMix + evenPath * even;
            const float dcF = summed - dcInF[(size_t) ch] + dcCoeff * dcOutF[(size_t) ch];
            dcInF[(size_t) ch]  = summed;
            dcOutF[(size_t) ch] = dcF;

            data[sample] = dcF * out;
        }
    }
}

//==============================================================================
juce::AudioProcessorEditor* MyGainProcessor::createEditor()
{
    return new MyGainEditor (*this);
}

void MyGainProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MyGainProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MyGainProcessor();
}
