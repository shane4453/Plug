#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>

//==============================================================================
// SHANE DIST — 빈티지 스타일 디스토션
//
//  - 모드: Saturate(tanh) / Distort(하드클립) / Fuzz(극단 게인 + 비대칭)
//  - 홀수/짝수 배음 개별 컨트롤
//  - Mix (dry/wet 병렬 처리)
//  - 아날로그 섹션: 동적 바이어스(엔벨로프 추적) + Tone 틸트 필터
//  - 4x 오버샘플링 (끌 수 있음)
//==============================================================================
class MyGainProcessor : public juce::AudioProcessor
{
public:
    MyGainProcessor();
    ~MyGainProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Shane Dist"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // VU 미터용 레벨 (오디오 스레드가 쓰고 UI 스레드가 읽음 → atomic)
    float getInputLevel()  const { return inputLevel.load(); }
    float getOutputLevel() const { return outputLevel.load(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // 파라미터 (오디오 스레드에서 락 없이 읽기)
    std::atomic<float>* driveParam  = nullptr;
    std::atomic<float>* oddParam    = nullptr;
    std::atomic<float>* evenParam   = nullptr;
    std::atomic<float>* outputParam = nullptr;
    std::atomic<float>* osParam     = nullptr;
    std::atomic<float>* modeParam   = nullptr;   // 0=Sat 1=Dist 2=Fuzz
    std::atomic<float>* mixParam    = nullptr;
    std::atomic<float>* analogParam = nullptr;
    std::atomic<float>* toneParam   = nullptr;

    // 스무더: 위 4개는 오버샘플 레이트, 아래 2개는 기본 레이트에서 동작
    juce::SmoothedValue<float> driveSm, oddSm, evenSm, outSm;
    juce::SmoothedValue<float> mixSm, toneSm;

    // 채널별 상태
    std::vector<float> dcInE, dcOutE;    // 짝수 경로 DC 제거
    std::vector<float> dcInF, dcOutF;    // 최종 출력 DC 제거 (fuzz/바이어스 대비)
    std::vector<float> envFollow;        // 동적 바이어스용 엔벨로프
    std::vector<float> toneLp;           // Tone 틸트 필터 상태

    juce::AudioBuffer<float> dryBuffer;  // Mix용 원본 보관
    // Dry/Wet 위상 정렬: 오버샘플링 필터가 wet을 지연시키는 만큼 dry도 지연
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> dryDelay { 128 };

    // 샘플레이트 의존 계수 (prepareToPlay에서 계산)
    float dcCoeff = 0.999f, envDecay = 0.999f, toneCoeff = 0.2f;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    bool lastOsState = true;

    std::atomic<float> inputLevel { 0.0f }, outputLevel { 0.0f };

    void processDistortion (juce::dsp::AudioBlock<float>& block);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MyGainProcessor)
};
