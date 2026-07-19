#include "PluginEditor.h"
#include <BinaryData.h>

//==============================================================================
static const juce::Colour creamCol { 0xffe8dcc0 };
static const juce::Colour ledOn    { 0xffe03a2a };
static const juce::Colour ledOff   { 0xff4a1f1a };

OverlayLookAndFeel::OverlayLookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff17130f));
    setColour (juce::ComboBox::textColourId, creamCol);
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0x00000000));   // 배경 판이 이미 있음
    setColour (juce::ComboBox::arrowColourId, juce::Colour (0xffc9a86a));
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff17130f));
    setColour (juce::PopupMenu::textColourId, creamCol);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0x40c9a86a));
    setColour (juce::PopupMenu::highlightedTextColourId, creamCol);
    setColour (juce::BubbleComponent::backgroundColourId, juce::Colour (0xff17130f));
    setColour (juce::BubbleComponent::outlineColourId, juce::Colour (0xffc9a86a));
}

void OverlayLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                           float sliderPos, float startAngle, float endAngle,
                                           juce::Slider&)
{
    // 배경 이미지의 노브 위에 얹는 크림색 포인터
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    auto centre = bounds.getCentre();
    auto R      = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
    auto angle  = startAngle + sliderPos * (endAngle - startAngle);

    g.setColour (creamCol);
    g.drawLine ({ centre.getPointOnCircumference (R * 0.28f, angle),
                  centre.getPointOnCircumference (R * 0.68f, angle) }, 3.0f);

    // 포인터 끝의 도트 (배경 노브의 장식과 어울리게)
    auto tip = centre.getPointOnCircumference (R * 0.68f, angle);
    g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (tip));
}

void OverlayLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height,
                                       bool, int, int, int, int, juce::ComboBox&)
{
    // 배경 플레이트에 남은 글자를 확실히 덮는 불투명 배경
    auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
    g.setColour (juce::Colour (0xff17140f));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (juce::Colour (0x30c9a86a));
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);

    juce::Path p;
    const float ax = (float) width - 20.0f, ay = (float) height * 0.5f;
    p.addTriangle (ax, ay - 3.0f, ax + 10.0f, ay - 3.0f, ax + 5.0f, ay + 4.0f);
    g.setColour (juce::Colour (0xffc9a86a));
    g.fillPath (p);
}

juce::Font OverlayLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (juce::FontOptions (16.0f)).withExtraKerningFactor (0.08f);
}

//==============================================================================
void ImageKnob::paint (juce::Graphics& g)
{
    if (! src.isValid())
        return;

    auto dest = getLocalBounds().toFloat();
    const auto rp   = getRotaryParameters();
    const float prop  = (float) valueToProportionOfLength (getValue());
    const float angle = rp.startAngleRadians + prop * (rp.endAngleRadians - rp.startAngleRadians);

    // 원형 클리핑: 노브 몸통만 회전 (사각 크롭 모서리의 배경까지 도는 것 방지)
    juce::Path clip;
    clip.addEllipse (dest.reduced (1.0f));
    g.saveState();
    g.reduceClipRegion (clip);

    const float scale = dest.getWidth() / (float) src.getWidth();
    auto t = juce::AffineTransform::translation ((float) -src.getWidth() * 0.5f,
                                                 (float) -src.getHeight() * 0.5f)
                 .rotated (angle)
                 .scaled (scale)
                 .followedBy (juce::AffineTransform::translation (dest.getCentreX(),
                                                                  dest.getCentreY()));
    g.drawImageTransformed (src, t);
    g.restoreState();
}

//==============================================================================
void NeedleOverlay::setLevel (float linearRms)
{
    targetDb = juce::Decibels::gainToDecibels (linearRms, -60.0f) + 18.0f;   // 0 VU = -18 dBFS
}

void NeedleOverlay::tick()
{
    const float coeff = targetDb > displayDb ? 0.45f : 0.12f;
    displayDb += (targetDb - displayDb) * coeff;
    repaint();
}

void NeedleOverlay::paint (juce::Graphics& g)
{
    auto face = getLocalBounds().toFloat();
    const juce::Point<float> pivot (face.getCentreX(), face.getBottom() + face.getHeight() * 0.18f);
    const float radius = face.getHeight() * 1.02f;

    auto dbToAngle = [] (float db)
    {
        return juce::jmap (juce::jlimit (-20.0f, 3.0f, db), -20.0f, 3.0f, -0.72f, 0.72f);
    };

    // 빈티지 바늘 (다크 브라운, 밑부분 굵게)
    const auto a   = dbToAngle (displayDb);
    const auto tip = pivot.getPointOnCircumference (radius, a);
    g.setColour (juce::Colour (0xff2b241a));
    g.drawLine ({ pivot, tip }, 2.2f);
}

//==============================================================================
//  Editor
//==============================================================================
MyGainEditor::MyGainEditor (MyGainProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);

    bg = juce::ImageCache::getFromMemory (BinaryData::background_png,
                                          BinaryData::background_pngSize);

    for (auto* s : { &drive, &odd, &even, &mix, &output, &tone })
    {
        s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s->setPopupDisplayEnabled (true, true, this);
        addAndMakeVisible (*s);
    }

    // 배경에서 노브 몸통을 잘라내 회전용 이미지로 (좌표는 이미지 비율 기준)
    if (bg.isValid())
    {
        auto cropKnob = [this] (float cx, float cy, float r)   // r = 이미지 가로폭 기준 반지름
        {
            const float W = (float) bg.getWidth(), H = (float) bg.getHeight();
            return bg.getClippedImage ({ juce::roundToInt (cx * W - r * W),
                                         juce::roundToInt (cy * H - r * W),
                                         juce::roundToInt (2.0f * r * W),
                                         juce::roundToInt (2.0f * r * W) });
        };
        // r=0.056: 노브 스커트의 흰 도트 장식까지 회전 원에 포함 (눈금 직전까지)
        drive .setKnobImage (cropKnob (0.147f, 0.345f, 0.056f));
        odd   .setKnobImage (cropKnob (0.333f, 0.345f, 0.056f));
        even  .setKnobImage (cropKnob (0.512f, 0.345f, 0.056f));
        mix   .setKnobImage (cropKnob (0.690f, 0.345f, 0.056f));
        output.setKnobImage (cropKnob (0.865f, 0.345f, 0.056f));
        tone  .setKnobImage (cropKnob (0.5854f, 0.8773f, 0.038f));
    }

    mode.addItemList ({ "SATURATE", "DISTORT", "FUZZ" }, 1);
    mode.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (mode);

    // 토글: 배경의 스위치/LED 위 투명 히트영역. 클릭 시 LED 다시 그리기
    for (auto* b : { &analogBtn, &osBtn })
    {
        b->onClick = [this] { repaint(); };
        addAndMakeVisible (*b);
    }

    addAndMakeVisible (inNeedle);
    addAndMakeVisible (outNeedle);

    auto& s = proc.apvts;
    driveA  = std::make_unique<SA> (s, "drive",  drive);
    oddA    = std::make_unique<SA> (s, "odd",    odd);
    evenA   = std::make_unique<SA> (s, "even",   even);
    mixA    = std::make_unique<SA> (s, "mix",    mix);
    outputA = std::make_unique<SA> (s, "output", output);
    toneA   = std::make_unique<SA> (s, "tone",   tone);
    analogA = std::make_unique<BA> (s, "analog", analogBtn);
    osA     = std::make_unique<BA> (s, "oversample", osBtn);
    modeA   = std::make_unique<CA> (s, "mode",   mode);

    startTimerHz (30);

    // 배경 이미지 비율 유지 (1517:1029 ≈ 1.475)
    setSize (1012, 686);
}

MyGainEditor::~MyGainEditor()
{
    setLookAndFeel (nullptr);
}

void MyGainEditor::timerCallback()
{
    inNeedle .setLevel (proc.getInputLevel());
    outNeedle.setLevel (proc.getOutputLevel());
    inNeedle .tick();
    outNeedle.tick();
}

// 이미지 기준 비율 좌표 → 실제 픽셀
juce::Rectangle<int> MyGainEditor::frac (float x, float y, float w, float h) const
{
    return { juce::roundToInt (x * (float) getWidth()),
             juce::roundToInt (y * (float) getHeight()),
             juce::roundToInt (w * (float) getWidth()),
             juce::roundToInt (h * (float) getHeight()) };
}

//==============================================================================
void MyGainEditor::paint (juce::Graphics& g)
{
    // 배경 이미지 (전체 UI)
    if (bg.isValid())
        g.drawImage (bg, getLocalBounds().toFloat());
    else
    {
        g.fillAll (juce::Colour (0xff141110));
        g.setColour (creamCol);
        g.drawText ("Assets/background.png 가 없습니다 — 리빌드 필요",
                    getLocalBounds(), juce::Justification::centred);
    }

    // LED 상태 (배경의 LED를 완전히 덮어 그림 — 크기도 배경 LED와 동일하게)
    auto drawLed = [&g, this] (float cx, float cy, float dia, bool on)
    {
        auto r = juce::Rectangle<float> (dia * (float) getWidth(),
                                         dia * (float) getWidth())
                     .withCentre ({ cx * (float) getWidth(), cy * (float) getHeight() });
        if (on)
        {
            g.setColour (ledOn.withAlpha (0.35f));
            g.fillEllipse (r.expanded (r.getWidth() * 0.4f));   // 글로우
        }
        g.setColour (on ? ledOn : ledOff);
        g.fillEllipse (r);
        g.setColour (juce::Colours::white.withAlpha (on ? 0.55f : 0.08f));
        g.fillEllipse (r.reduced (r.getWidth() * 0.32f).translated (-r.getWidth() * 0.08f, -r.getWidth() * 0.10f));
    };

    drawLed (0.7425f, 0.0920f, 0.0260f, osBtn.getToggleState());       // OVERSAMPLE 4X
    drawLed (0.3995f, 0.8880f, 0.0200f, analogBtn.getToggleState());   // ANALOG
}

//==============================================================================
void MyGainEditor::resized()
{
    // ── 좌표는 전부 배경 이미지 기준 비율 (이미지 바꾸면 여기만 수정) ──

    // 노브: 크롭 이미지와 같은 원 (중심 cx,cy / 반지름 r은 가로폭 기준 비율)
    auto knob = [this] (juce::Slider& s, float cx, float cy, float r)
    {
        const float W = (float) getWidth(), H = (float) getHeight();
        const float rw = r * W;
        s.setBounds (juce::roundToInt (cx * W - rw), juce::roundToInt (cy * H - rw),
                     juce::roundToInt (2.0f * rw),   juce::roundToInt (2.0f * rw));
    };
    knob (drive,  0.147f, 0.345f, 0.056f);
    knob (odd,    0.333f, 0.345f, 0.056f);
    knob (even,   0.512f, 0.345f, 0.056f);
    knob (mix,    0.690f, 0.345f, 0.056f);
    knob (output, 0.865f, 0.345f, 0.056f);
    knob (tone,   0.5854f, 0.8773f, 0.038f);

    // VU 바늘 오버레이 (배경 미터 판 영역)
    inNeedle .setBounds (frac (0.170f, 0.590f, 0.246f, 0.140f));
    outNeedle.setBounds (frac (0.585f, 0.590f, 0.250f, 0.140f));

    // MODE 콤보 (배경의 플레이트 안쪽)
    mode.setBounds (frac (0.095f, 0.868f, 0.165f, 0.048f));

    // 토글 히트영역 (배경의 스위치+LED+라벨 전체)
    analogBtn.setBounds (frac (0.345f, 0.862f, 0.145f, 0.062f));
    osBtn    .setBounds (frac (0.725f, 0.068f, 0.175f, 0.052f));
}
