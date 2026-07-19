#pragma once

#include "PluginProcessor.h"

//==============================================================================
// 이미지 기반 UI: 배경 PNG 위에 "움직이는 것들"만 코드로 그린다
//  - 노브: 배경에 그려진 노브 위에 회전하는 포인터만
//  - 미터: 배경의 VU 판 위에 바늘만
//  - LED/토글: 상태 표시만
//==============================================================================
class OverlayLookAndFeel : public juce::LookAndFeel_V4
{
public:
    OverlayLookAndFeel();

    // 포인터만 그리는 로터리 (이미지 크롭이 없을 때의 폴백)
    void drawRotarySlider (juce::Graphics&, int, int, int, int,
                           float, float, float, juce::Slider&) override;

    // 토글은 히트영역만 (그림은 배경 + 에디터의 LED가 담당)
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override {}

    // 콤보박스: 투명 배경 + 골드 화살표만 (플레이트는 배경 이미지에 있음)
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int,
                       juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
};

//==============================================================================
// 배경에서 잘라낸 노브 이미지를 통째로 회전시키는 노브
// 전제: 배경 이미지의 노브 포인터가 12시 방향을 향하고 있을 것
class ImageKnob : public juce::Slider
{
public:
    void setKnobImage (juce::Image img) { src = std::move (img); }
    void paint (juce::Graphics&) override;

private:
    juce::Image src;
};

//==============================================================================
// 배경 미터 판 위에 얹는 투명 바늘
class NeedleOverlay : public juce::Component
{
public:
    NeedleOverlay() { setInterceptsMouseClicks (false, false); }

    void setLevel (float linearRms);
    void tick();
    void paint (juce::Graphics&) override;

private:
    float displayDb = -20.0f, targetDb = -20.0f;
};

//==============================================================================
class MyGainEditor : public juce::AudioProcessorEditor,
                     private juce::Timer
{
public:
    explicit MyGainEditor (MyGainProcessor&);
    ~MyGainEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    juce::Rectangle<int> frac (float x, float y, float w, float h) const;

    MyGainProcessor& proc;
    OverlayLookAndFeel lnf;
    juce::Image bg;

    ImageKnob drive, odd, even, mix, output, tone;
    juce::ComboBox mode;
    juce::ToggleButton analogBtn, osBtn;
    NeedleOverlay inNeedle, outNeedle;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<SA> driveA, oddA, evenA, mixA, outputA, toneA;
    std::unique_ptr<BA> analogA, osA;
    std::unique_ptr<CA> modeA;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MyGainEditor)
};
