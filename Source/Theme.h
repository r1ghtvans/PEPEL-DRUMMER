#pragma once
#include <JuceHeader.h>
#include <BinaryData.h>
#include <functional>

// =============================================================================
//  Стиль: постсоветский ночной урбанизм + VHS 90-х
//  (тёмный двор, панельки, лампа-голубой свет, красные окна и звёзды,
//   цепи и колючая проволока)
// =============================================================================

namespace pal
{
    inline const juce::Colour night     { 0xff04050E };   // самый тёмный
    inline const juce::Colour navy      { 0xff090D26 };   // заливка панелей
    inline const juce::Colour navy2     { 0xff111845 };   // пустые сегменты, кнопки
    inline const juce::Colour frame     { 0xff2B3A82 };   // рамки панелей
    inline const juce::Colour steel     { 0xff8E9AC8 };   // цепи, проволока, рамки
    inline const juce::Colour steelDim  { 0xff4A5588 };
    inline const juce::Colour lamp      { 0xffCFE0FF };   // свет фонаря
    inline const juce::Colour lampSoft  { 0xff9FB8FF };
    inline const juce::Colour deepBlue  { 0xff2E44A8 };
    inline const juce::Colour red       { 0xffE0182D };   // красные окна и звёзды
    inline const juce::Colour redDim    { 0xff6B0F1F };
    inline const juce::Colour text      { 0xffC9D6FF };
    inline const juce::Colour textDim   { 0xff6F7DB8 };
    inline const juce::Colour white     { 0xffF2F6FF };

    inline const juce::Colour glitchRed  { 0xffFF2A3C };
    inline const juce::Colour glitchBlue { 0xff3C5CFF };

    // цвет строки секвенсора и номера пэда: в основном голубой свет, местами красные окна
    inline juce::Colour rowColour (int i)
    {
        static const juce::Colour colours[8] = { lamp, lampSoft, lamp, steel, red, lampSoft, lamp, steel };
        return colours[i & 7];
    }
}

// =============================================================================
//  Шрифты. Три слота: Assets/FontTitle.ttf, FontText.ttf, FontKanji.ttf.
//  Чтобы поставить свои шрифты, достаточно заменить эти файлы (имена те же).
// =============================================================================
namespace pixelfont
{
    inline juce::Typeface::Ptr titleFace()
    {
        static juce::Typeface::Ptr f = juce::Typeface::createSystemTypefaceFor (
            BinaryData::FontTitle_ttf, (size_t) BinaryData::FontTitle_ttfSize);
        return f;
    }

    inline juce::Typeface::Ptr textFace()
    {
        static juce::Typeface::Ptr f = juce::Typeface::createSystemTypefaceFor (
            BinaryData::FontText_ttf, (size_t) BinaryData::FontText_ttfSize);
        return f;
    }

    inline juce::Typeface::Ptr kanjiFace()
    {
        static juce::Typeface::Ptr f = juce::Typeface::createSystemTypefaceFor (
            BinaryData::FontKanji_ttf, (size_t) BinaryData::FontKanji_ttfSize);
        return f;
    }

    inline juce::Font vhs (float height)   { return juce::Font (juce::FontOptions (textFace()).withHeight (height)); }
    inline juce::Font title (float height) { return juce::Font (juce::FontOptions (titleFace()).withHeight (height)); }
    inline juce::Font kanji (float height) { return juce::Font (juce::FontOptions (kanjiFace()).withHeight (height)); }
}

// =============================================================================
//  Графика: звёзды, цепи, колючая проволока, спрайты
// =============================================================================

// Звезда-блёстка: тонкие лучи крестом со свечением
inline void drawStar (juce::Graphics& g, int cx, int cy, int size, juce::Colour c)
{
    if (size < 2)
        return;

    g.setColour (c.withAlpha (0.10f));
    g.fillEllipse ((float) (cx - size), (float) (cy - size), (float) size * 2.0f, (float) size * 2.0f);

    for (int i = -size; i <= size; ++i)
    {
        g.setColour (c.withAlpha (juce::jlimit (0.0f, 1.0f, 1.0f - (float) std::abs (i) / (float) size)));
        g.fillRect (cx, cy + i, 1, 1);
    }

    const int h = juce::jmax (1, (int) ((float) size * 0.7f));
    for (int i = -h; i <= h; ++i)
    {
        g.setColour (c.withAlpha (juce::jlimit (0.0f, 1.0f, 1.0f - (float) std::abs (i) / (float) h)));
        g.fillRect (cx + i, cy, 1, 1);
    }

    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.fillRect (cx - 1, cy - 1, 3, 3);
}

inline juce::Point<float> quadPoint (juce::Point<float> a, juce::Point<float> c, juce::Point<float> b, float t)
{
    const float u = 1.0f - t;
    return { u * u * a.x + 2.0f * u * t * c.x + t * t * b.x,
             u * u * a.y + 2.0f * u * t * c.y + t * t * b.y };
}

inline juce::Point<float> quadTangent (juce::Point<float> a, juce::Point<float> c, juce::Point<float> b, float t)
{
    const float u = 1.0f - t;
    return { 2.0f * u * (c.x - a.x) + 2.0f * t * (b.x - c.x),
             2.0f * u * (c.y - a.y) + 2.0f * t * (b.y - c.y) };
}

inline float quadLength (juce::Point<float> a, juce::Point<float> c, juce::Point<float> b)
{
    float length = 0.0f;
    auto prev = a;
    for (int i = 1; i <= 40; ++i)
    {
        const auto p = quadPoint (a, c, b, (float) i / 40.0f);
        length += prev.getDistanceFrom (p);
        prev = p;
    }
    return length;
}

// Цепь вдоль кривой: звенья чередуются плоско и ребром
inline void drawChain (juce::Graphics& g, juce::Point<float> a, juce::Point<float> c, juce::Point<float> b, juce::Colour col)
{
    const int count = juce::jmax (2, (int) (quadLength (a, c, b) / 9.0f));

    for (int k = 0; k <= count; ++k)
    {
        const float t = (float) k / (float) count;
        const auto p = quadPoint (a, c, b, t);
        const auto d = quadTangent (a, c, b, t);
        const float angle = std::atan2 (d.y, d.x);
        const bool flat = (k % 2) == 0;

        juce::Path link;
        const float w = 12.0f, h = flat ? 7.0f : 3.0f;
        link.addRoundedRectangle (-w * 0.5f, -h * 0.5f, w, h, h * 0.5f);

        const auto tf = juce::AffineTransform::rotation (angle).translated (p.x, p.y);
        g.setColour (pal::night.withAlpha (0.6f));
        g.strokePath (link, juce::PathStrokeType (3.2f), tf);
        g.setColour (col.withAlpha (flat ? 0.95f : 0.75f));
        g.strokePath (link, juce::PathStrokeType (1.7f), tf);
    }
}

// Колючая проволока: две жилы и колючки-крестики
inline void drawBarbedWire (juce::Graphics& g, juce::Point<float> a, juce::Point<float> c, juce::Point<float> b, juce::Colour col)
{
    juce::Path wire, strand;
    wire.startNewSubPath (a);

    for (int i = 0; i <= 80; ++i)
    {
        const float t = (float) i / 80.0f;
        const auto p = quadPoint (a, c, b, t);
        auto d = quadTangent (a, c, b, t);
        const float len = juce::jmax (0.001f, std::sqrt (d.x * d.x + d.y * d.y));
        const juce::Point<float> normal (-d.y / len, d.x / len);
        const auto q = p + normal * (2.4f * std::sin ((float) i * 0.9f));

        if (i == 0) strand.startNewSubPath (q); else strand.lineTo (q);
        wire.lineTo (p);
    }

    g.setColour (col.withAlpha (0.85f));
    g.strokePath (wire, juce::PathStrokeType (1.4f));
    g.setColour (col.withAlpha (0.55f));
    g.strokePath (strand, juce::PathStrokeType (1.0f));

    const int barbs = juce::jmax (2, (int) (quadLength (a, c, b) / 26.0f));
    for (int k = 0; k < barbs; ++k)
    {
        const float t = ((float) k + 0.5f) / (float) barbs;
        const auto p = quadPoint (a, c, b, t);
        const auto d = quadTangent (a, c, b, t);
        const float angle = std::atan2 (d.y, d.x);

        g.setColour (col);
        for (float da : { 0.9f, -0.9f })
        {
            const juce::Point<float> u (std::cos (angle + da), std::sin (angle + da));
            g.drawLine (p.x - u.x * 4.5f, p.y - u.y * 4.5f, p.x + u.x * 4.5f, p.y + u.y * 4.5f, 1.2f);
        }
    }
}

// Спрайты: массивы строк, '.' пусто
using SpriteRows = const char* const*;

inline void drawSprite (juce::Graphics& g, SpriteRows rows, int numRows, int x, int y, int scale,
                        const std::function<juce::Colour (char)>& colourOf)
{
    for (int row = 0; row < numRows; ++row)
    {
        const char* line = rows[row];
        for (int col = 0; line[col] != 0; ++col)
        {
            if (line[col] == '.')
                continue;

            g.setColour (colourOf (line[col]));
            g.fillRect (x + col * scale, y + row * scale, scale, scale);
        }
    }
}

namespace sprites
{
    inline constexpr int cloudRows = 6;
    inline const char* const cloud[] = {
        "....####......",
        "..########.##.",
        ".############.",
        "##############",
        ".############.",
        "..##########.." };
}

// Рамка панели: тёмная заливка, двойная рамка, красные метки в углах, шапка с пунктиром
inline void drawPanelFrame (juce::Graphics& g, juce::Rectangle<int> r, juce::Colour accent)
{
    g.setColour (pal::navy);
    g.fillRect (r);

    g.setColour (pal::frame);
    g.drawRect (r, 1);
    g.setColour (pal::frame.withAlpha (0.5f));
    g.drawRect (r.reduced (3), 1);

    const int L = 10;
    g.setColour (accent);
    g.fillRect (r.getX(), r.getY(), L, 2);                      g.fillRect (r.getX(), r.getY(), 2, L);
    g.fillRect (r.getRight() - L, r.getY(), L, 2);              g.fillRect (r.getRight() - 2, r.getY(), 2, L);
    g.fillRect (r.getX(), r.getBottom() - 2, L, 2);             g.fillRect (r.getX(), r.getBottom() - L, 2, L);
    g.fillRect (r.getRight() - L, r.getBottom() - 2, L, 2);     g.fillRect (r.getRight() - 2, r.getBottom() - L, 2, L);
}

// =============================================================================
//  LookAndFeel: сегментные слайдеры без ручек
// =============================================================================
class PixelLookAndFeel : public juce::LookAndFeel_V4
{
public:
    static constexpr int segW = 6, segH = 14, segGap = 2;

    juce::Slider::SliderLayout getSliderLayout (juce::Slider& s) override
    {
        juce::Slider::SliderLayout layout;
        layout.sliderBounds = s.getLocalBounds();
        return layout;
    }

    int getSliderThumbRadius (juce::Slider&) override { return 0; }

    // от глубокого синего к свету фонаря
    static juce::Colour segmentColour (float t) { return pal::deepBlue.interpolatedWith (pal::lamp, t); }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float, float, float, juce::Slider::SliderStyle, juce::Slider& s) override
    {
        const int pitch = segW + segGap;
        const int n = juce::jmax (1, (width + segGap) / pitch);
        const int top = y + (height - segH) / 2;
        const double prop = juce::jlimit (0.0, 1.0, s.valueToProportionOfLength (s.getValue()));
        const int filled = (int) std::lround (prop * n);

        for (int i = 0; i < n; ++i)
        {
            const int sx = x + i * pitch;

            if (i < filled)
            {
                g.setColour (segmentColour (n > 1 ? (float) i / (float) (n - 1) : 0.0f));
                g.fillRect (sx, top, segW, segH);
                g.setColour (juce::Colours::white.withAlpha (0.35f));      // блик сверху
                g.fillRect (sx, top, segW, 2);
            }
            else
            {
                g.setColour (pal::navy2);
                g.fillRect (sx, top, segW, segH);
            }
        }

        // светлый блок с красной рамкой на конце заполнения
        const int boundary = filled > 0 ? x + filled * pitch - segGap / 2 : x;
        const int mx = juce::jlimit (x, x + n * pitch - segGap - segW, boundary - segW / 2);
        g.setColour (pal::white);
        g.fillRect (mx, top - 2, segW, segH + 4);
        g.setColour (pal::red);
        g.drawRect (mx, top - 2, segW, segH + 4, 1);
    }
};

// =============================================================================
//  Виджеты
// =============================================================================

// Строка параметра: название, сегментный слайдер, значение
class ParamRow : public juce::Component
{
public:
    static constexpr int nameW = 88, valueW = 78;

    ParamRow (const juce::String& n, std::function<juce::String (double)> fmt)
        : name (n), format (std::move (fmt))
    {
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.onValueChange = [this] { repaint(); };
        addAndMakeVisible (slider);
    }

    ~ParamRow() override { attachment.reset(); }

    void attach (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID)
    {
        attachment.reset();
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramID, slider);   // двойной клик сбрасывает значение
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        g.setColour (pal::text);
        g.setFont (pixelfont::vhs (24.0f));
        g.drawText (name, r.removeFromLeft (nameW), juce::Justification::centredLeft, false);

        g.setColour (pal::textDim);
        g.drawText (format (slider.getValue()), r.removeFromRight (valueW), juce::Justification::centredRight, false);
    }

    void resized() override
    {
        auto r = getLocalBounds();
        r.removeFromLeft (nameW);
        r.removeFromRight (valueW);
        slider.setBounds (r.reduced (6, 0));
    }

    juce::Slider slider;

private:
    juce::String name;
    std::function<juce::String (double)> format;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// Кнопка (обычная или с фиксацией): рамка цвета акцента, при включении заливка
class PixelButton : public juce::Component
{
public:
    PixelButton (const juce::String& t, juce::Colour c, bool toggle = false)
        : text (t), colour (c), toggleMode (toggle) {}

    std::function<void()> onClick;

    void setOn (bool shouldBeOn)
    {
        if (on != shouldBeOn)
        {
            on = shouldBeOn;
            repaint();
        }
    }

    bool isOn() const { return on; }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().withTrimmedRight (3).withTrimmedBottom (3);
        const int off = down ? 2 : 0;
        r = r.translated (off, off);

        if (! down)
        {
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillRect (r.translated (3, 3));
        }

        g.setColour (on ? colour : pal::navy2);
        g.fillRect (r);
        g.setColour (colour);
        g.drawRect (r, 2);

        g.setColour (on ? pal::night : pal::text);
        g.setFont (pixelfont::vhs (24.0f));
        g.drawText (text, r, juce::Justification::centred, false);
    }

    void mouseDown (const juce::MouseEvent&) override { down = true; repaint(); }

    void mouseUp (const juce::MouseEvent& e) override
    {
        down = false;

        if (getLocalBounds().contains (e.getPosition()))
        {
            if (toggleMode)
                on = ! on;

            if (onClick)
                onClick();
        }
        repaint();
    }

protected:
    bool down = false;

private:
    juce::String text;
    juce::Colour colour;
    bool toggleMode, on = false;
};

// Выбор из списка: клик по левой трети назад, иначе вперёд (колесо мыши тоже работает)
class ChoiceRow : public juce::Component
{
public:
    ChoiceRow (const juce::String& n, const juce::StringArray& displayLabels, juce::Colour c = pal::lampSoft)
        : name (n), labels (displayLabels), colour (c) {}

    ~ChoiceRow() override { attachment.reset(); }

    void attach (juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID)
    {
        attachment.reset();
        param = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (paramID));

        if (param == nullptr)
            return;

        attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v)
        {
            index = juce::roundToInt (v);
            repaint();
        });
        attachment->sendInitialUpdate();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        g.setColour (pal::text);
        g.setFont (pixelfont::vhs (24.0f));
        g.drawText (name, r.removeFromLeft (ParamRow::nameW), juce::Justification::centredLeft, false);

        auto box = boxBounds();
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRect (box.translated (3, 3));
        g.setColour (pal::navy2);
        g.fillRect (box);
        g.setColour (colour);
        g.drawRect (box, 2);

        g.setColour (pal::text);
        g.drawText (labels[juce::jlimit (0, juce::jmax (0, labels.size() - 1), index)], box, juce::Justification::centred, false);

        g.setColour (colour);
        const int cy = box.getCentreY();
        for (int i = 0; i < 3; ++i)
        {
            g.fillRect (box.getX() + 10 + i, cy - 3 + i, 2, 7 - i * 2);              // <
            g.fillRect (box.getRight() - 12 - i, cy - 3 + i, 2, 7 - i * 2);          // >
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto box = boxBounds();
        step (e.x < box.getX() + box.getWidth() / 3 ? -1 : 1);
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        step (w.deltaY > 0 ? -1 : 1);
    }

private:
    juce::Rectangle<int> boxBounds() const
    {
        return getLocalBounds().withTrimmedLeft (ParamRow::nameW + 6).withTrimmedRight (3).withTrimmedBottom (3).reduced (0, 1);
    }

    void step (int dir)
    {
        if (param == nullptr || attachment == nullptr)
            return;

        const int n = param->choices.size();
        index = (index + dir + n) % n;
        attachment->setValueAsCompleteGesture ((float) index);
        repaint();
    }

    juce::String name;
    juce::StringArray labels;
    juce::Colour colour;
    int index = 0;
    juce::AudioParameterChoice* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};
