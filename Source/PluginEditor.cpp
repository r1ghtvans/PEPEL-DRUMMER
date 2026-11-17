#include "PluginEditor.h"

//==============================================================================
// форматирование значений
static juce::String fmtDb (double v)    { return v <= -59.9 ? juce::String ("-inf dB") : juce::String (v, 1) + " dB"; }
static juce::String fmtGain (double v)  { return (v > 0.04 ? juce::String ("+") : juce::String()) + juce::String (v, 1) + " dB"; }
static juce::String fmtPan (double v)
{
    const int p = (int) std::lround (std::abs (v) * 100.0);
    return p == 0 ? juce::String ("C") : (v < 0 ? juce::String ("L ") : juce::String ("R ")) + juce::String (p) + "%";
}
static juce::String fmtSt (double v)    { return (v > 0.004 ? juce::String ("+") : juce::String()) + juce::String (v, 1) + " st"; }
static juce::String fmtPct (double v)   { return juce::String ((int) std::lround (v * 100.0)) + "%"; }
static juce::String fmtChoke (double v) { const int g = (int) std::lround (v); return g == 0 ? juce::String ("off") : "G" + juce::String (g); }
static juce::String fmtSwing (double v) { return juce::String ((int) std::lround (v)) + "%"; }
static juce::String fmtLen (double v)   { return juce::String ((int) std::lround (v)); }

//==============================================================================
// Сцена шапки: панельки на горизонте
namespace scene
{
    struct Block { int x, w, h; };

    constexpr int horizon = 118;

    // низкие дальние дома и основные панельки
    inline const Block farBlocks[]  = { {0,58,20}, {60,40,28}, {102,64,18}, {170,50,24}, {224,46,32}, {274,56,20} };
    inline const Block mainBlocks[] = { {380,64,50}, {448,52,38}, {504,96,60}, {604,52,44}, {660,70,54}, {734,66,40} };

    template <typename F>
    inline void forEachWindow (const Block& b, F&& f)
    {
        const int cols = (b.w - 6) / 8, rows = (b.h - 8) / 10;
        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < cols; ++c)
                f (b.x + 4 + c * 8, horizon - b.h + 5 + r * 10);
    }

    inline void drawBranch (juce::Graphics& g, juce::Random& rnd, float x, float y, float angle, float len, int depth)
    {
        if (depth == 0 || len < 3.0f)
            return;

        const float x2 = x + std::cos (angle) * len, y2 = y + std::sin (angle) * len;
        g.drawLine (x, y, x2, y2, depth > 3 ? 2.0f : 1.0f);

        drawBranch (g, rnd, x2, y2, angle - 0.45f - rnd.nextFloat() * 0.3f, len * 0.72f, depth - 1);
        drawBranch (g, rnd, x2, y2, angle + 0.45f + rnd.nextFloat() * 0.3f, len * 0.72f, depth - 1);

        if (depth > 2 && rnd.nextBool())
            drawBranch (g, rnd, x2, y2, angle + (rnd.nextFloat() - 0.5f) * 0.3f, len * 0.6f, depth - 2);
    }
}

//==============================================================================
// Пэд = окно панельки: гаснет, когда пуст; светится, когда по нему бьют; выбранное окно красное
class DrumRackEditor::PadButton : public juce::Component,
                                  public juce::FileDragAndDropTarget
{
public:
    PadButton (DrumRackEditor& o, int i) : owner (o), index (i) {}

    float flash = 0.0f;
    bool  isSelected = false;

    void paint (juce::Graphics& g) override
    {
        const bool hasSample = owner.proc.getSample (index) != nullptr;
        auto r = getLocalBounds().withTrimmedRight (3).withTrimmedBottom (3).reduced (1);

        // стена
        g.setColour (juce::Colour (0xff0E1438));
        g.fillRect (r);
        g.setColour (pal::frame);
        g.drawRect (r, 1);

        // свечение вокруг окна
        if (flash > 0.05f)
        {
            g.setColour (pal::lamp.withAlpha (flash * 0.22f));
            g.fillRect (r);
        }

        // стекло
        auto glass = r.reduced (6);
        const juce::Colour base = isSelected ? pal::redDim
                                             : (hasSample ? juce::Colour (0xff1C2B78) : juce::Colour (0xff0A0E2C));
        g.setColour (base.interpolatedWith (pal::lamp, flash));
        g.fillRect (glass);

        // переплёт
        const int barY = glass.getY() + glass.getHeight() * 2 / 5;
        g.setColour (pal::steel.withAlpha (0.45f));
        g.fillRect (glass.getCentreX() - 1, glass.getY(), 2, glass.getHeight());
        g.fillRect (glass.getX(), barY, glass.getWidth(), 2);

        // рама
        g.setColour (isSelected ? pal::red : pal::steel);
        g.drawRect (glass, 2);

        const auto textCol = flash > 0.5f ? pal::night : pal::text;
        const int note = owner.proc.padNote[index].load();

        g.setColour (textCol);
        g.setFont (pixelfont::title (8.0f));
        g.drawText (juce::String (index + 1).paddedLeft ('0', 2), glass.getX() + 5, glass.getY() + 1, 24, barY - glass.getY() - 1,
                    juce::Justification::centredLeft, false);

        g.setColour (textCol.withAlpha (0.7f));
        g.setFont (pixelfont::vhs (20.0f));
        g.drawText (juce::MidiMessage::getMidiNoteName (note, true, true, 5), glass.getX() + 28, glass.getY() + 1,
                    glass.getWidth() - 33, barY - glass.getY() - 1, juce::Justification::centredRight, false);

        const auto name = owner.proc.getSampleName (index);
        g.setColour (name.isEmpty() ? pal::textDim.withAlpha (0.7f) : textCol);
        g.setFont (pixelfont::vhs (20.0f));
        g.drawFittedText (name.isEmpty() ? "empty" : name,
                          juce::Rectangle<int> (glass.getX() + 4, barY + 3, glass.getWidth() - 8, glass.getBottom() - barY - 6),
                          juce::Justification::centred, 2);
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        owner.selectPad (index);
        owner.proc.previewPad (index, 0.8f);
    }

    void mouseDoubleClick (const juce::MouseEvent&) override { owner.browseForSample(); }

    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        for (auto& f : files)
            if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;ogg"))
                return true;
        return false;
    }

    void filesDropped (const juce::StringArray& files, int, int) override
    {
        for (auto& f : files)
            if (owner.proc.loadSample (index, juce::File (f)))
            {
                owner.selectPad (index);
                break;
            }
        repaint();
    }

private:
    DrumRackEditor& owner;
    int index;
};

//==============================================================================
// Сетка шагов: 16 строк (пэды) x 16 шагов
class DrumRackEditor::StepGrid : public juce::Component,
                                 public juce::FileDragAndDropTarget
{
public:
    explicit StepGrid (DrumRackEditor& o) : owner (o), proc (o.proc) {}

    int playStep = -1;

    void paint (juce::Graphics& g) override
    {
        const int pattern = currentPattern();
        const int len = juce::jlimit (1, 16, (int) std::lround (proc.apvts.getRawParameterValue ("seqlen")->load()));
        const int rh = rowHeight();
        const int cw = cellWidth();

        g.setFont (pixelfont::vhs (18.0f));
        g.setColour (pal::textDim);
        for (int beat = 0; beat < 4; ++beat)
            g.drawText (juce::String (beat + 1), cellX (beat * 4) + 2, 0, 30, rulerH, juce::Justification::centredLeft, false);

        for (int row = 0; row < 16; ++row)
        {
            const int y = rulerH + row * rh;
            const auto rowCol = pal::rowColour (row);
            const auto name = proc.getSampleName (row);

            if (row == owner.selected)
            {
                g.setColour (pal::red.withAlpha (0.28f));
                g.fillRect (0, y, labelW - 6, rh - 1);
            }

            g.setColour (rowCol);
            g.fillRect (3, y + rh / 2 - 3, 6, 6);

            g.setColour (pal::text);
            g.setFont (pixelfont::title (8.0f));
            g.drawText (juce::String (row + 1).paddedLeft ('0', 2), 13, y, 20, rh, juce::Justification::centredLeft, false);

            g.setColour (name.isEmpty() ? pal::textDim.withAlpha (0.6f) : pal::textDim.brighter (0.3f));
            g.setFont (pixelfont::vhs (17.0f));
            g.drawText (name.isEmpty() ? "-" : name, 38, y, labelW - 48, rh, juce::Justification::centredLeft, true);

            for (int col = 0; col < 16; ++col)
            {
                const juce::Rectangle<int> cell (cellX (col) + 1, y + 1, cw - 2, rh - 2);
                const bool on = proc.getStep (pattern, row, col);
                const bool inRange = col < len;

                auto fill = (col / 4) % 2 == 0 ? pal::navy2.brighter (0.35f) : pal::navy2.brighter (0.1f);
                if (on)
                    fill = rowCol;

                g.setColour (inRange ? fill : fill.withAlpha (0.35f));
                g.fillRect (cell);

                if (on && inRange)
                {
                    g.setColour (juce::Colours::white.withAlpha (0.4f));
                    g.fillRect (cell.getX(), cell.getY(), cell.getWidth(), 2);
                }

                if (col == playStep)
                {
                    g.setColour (juce::Colours::white.withAlpha (0.16f));
                    g.fillRect (cell);
                    g.setColour (pal::red);
                    g.drawRect (cell, 2);
                }
            }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto pt = e.getPosition();
        int row = 0, col = 0;

        if (pt.x < labelW && pt.y >= rulerH)
        {
            const int pad = juce::jlimit (0, 15, (pt.y - rulerH) / rowHeight());
            owner.selectPad (pad);
            proc.previewPad (pad, 0.8f);
            return;
        }

        if (cellAt (pt, row, col))
        {
            const int pattern = currentPattern();
            paintValue = ! proc.getStep (pattern, row, col);
            proc.setStep (pattern, row, col, paintValue);
            lastRow = row;
            lastCol = col;
            dragging = true;
            repaint();
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! dragging)
            return;

        int row = 0, col = 0;
        if (cellAt (e.getPosition(), row, col) && (row != lastRow || col != lastCol))
        {
            proc.setStep (currentPattern(), row, col, paintValue);
            lastRow = row;
            lastCol = col;
            repaint();
        }
    }

    void mouseUp (const juce::MouseEvent&) override { dragging = false; }

    // MIDI-файл можно бросить прямо на сетку: ноты превратятся в шаги
    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        for (auto& f : files)
            if (juce::File (f).hasFileExtension ("mid;midi"))
                return true;
        return false;
    }

    void filesDropped (const juce::StringArray& files, int, int) override
    {
        for (auto& f : files)
            if (juce::File (f).hasFileExtension ("mid;midi"))
            {
                owner.importMidi (juce::File (f));
                break;
            }
    }

private:
    static constexpr int labelW = 112;
    static constexpr int rulerH = 16;
    static constexpr int gapW = 6;

    int currentPattern() const
    {
        return juce::jlimit (0, DrumRackProcessor::numPatterns - 1,
                             (int) std::lround (proc.apvts.getRawParameterValue ("pattern")->load()));
    }

    int rowHeight() const { return juce::jmax (8, (getHeight() - rulerH) / 16); }
    int cellWidth() const { return (getWidth() - labelW - 3 * gapW) / 16; }
    int cellX (int col) const { return labelW + col * cellWidth() + (col / 4) * gapW; }

    bool cellAt (juce::Point<int> pt, int& row, int& col) const
    {
        if (pt.y < rulerH || pt.x < labelW)
            return false;

        row = juce::jlimit (0, 15, (pt.y - rulerH) / rowHeight());

        for (int c = 15; c >= 0; --c)
            if (pt.x >= cellX (c))
            {
                col = c;
                return true;
            }
        return false;
    }

    DrumRackEditor& owner;
    DrumRackProcessor& proc;
    bool dragging = false;
    bool paintValue = true;
    int lastRow = -1, lastCol = -1;
};

//==============================================================================
// Кнопка: зажми и тяни в Piano roll или плейлист FL, чтобы вставить паттерн как MIDI
class DrumRackEditor::MidiDragButton : public PixelButton
{
public:
    explicit MidiDragButton (DrumRackEditor& o) : PixelButton ("DRAG .MID", pal::lampSoft), owner (o) {}

    void mouseDown (const juce::MouseEvent& e) override { started = false; PixelButton::mouseDown (e); }
    void mouseUp (const juce::MouseEvent& e) override   { started = false; PixelButton::mouseUp (e); }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (started || e.getDistanceFromDragStart() < 8)
            return;

        started = true;

        const int pattern = juce::jlimit (0, DrumRackProcessor::numPatterns - 1,
                                          (int) std::lround (owner.proc.apvts.getRawParameterValue ("pattern")->load()));

        auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("PEPEL DRUMMER");
        dir.createDirectory();
        auto file = dir.getChildFile ("PEPEL DRUMMER pattern " + juce::String::charToString ((juce::juce_wchar) ('A' + pattern)) + ".mid");

        if (owner.proc.writePatternMidi (pattern, file))
        {
            owner.setStatus ("pattern " + juce::String::charToString ((juce::juce_wchar) ('A' + pattern)) + " > drop into FL");
            juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
        }
    }

private:
    DrumRackEditor& owner;
    bool started = false;
};

//==============================================================================
DrumRackEditor::DrumRackEditor (DrumRackProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      volRow ("Volume", fmtDb), panRow ("Pan", fmtPan), pitchRow ("Pitch", fmtSt),
      startRow ("Start", fmtPct), endRow ("End", fmtPct), chokeRow ("Choke", fmtChoke),
      driveRow ("Drive", fmtGain), ceilRow ("Ceiling", fmtDb),
      patternChoice ("Pattern", DrumRackProcessor::patternChoices(), pal::lampSoft),
      gridChoice ("Grid", DrumRackProcessor::gridChoices(), pal::lampSoft),
      swingRow ("Swing", fmtSwing), lenRow ("Length", fmtLen),
      repeatChoice ("Repeat", DrumRackProcessor::repeatChoices(), pal::lampSoft)
{
    setLookAndFeel (&laf);
    setOpaque (true);
    setResizable (false, false);

    // --- вкладки ---
    addAndMakeVisible (padsTab);
    addAndMakeVisible (seqTab);
    padsTab.onClick = [this] { showPage (0); };
    seqTab.onClick  = [this] { showPage (1); };

    // --- страница PADS ---
    for (int i = 0; i < DrumRackProcessor::numPads; ++i)
    {
        pads[i] = std::make_unique<PadButton> (*this, i);
        addAndMakeVisible (*pads[i]);
        padsPage.push_back (pads[i].get());
        lastHit[i] = proc.hitCounter[i].load();
    }

    loadBtn.onClick  = [this] { browseForSample(); };
    clearBtn.onClick = [this] { proc.clearSample (selected); waveData = nullptr; repaint(); };
    learnBtn.onClick = [this]
    {
        proc.learnPad.store (selected);
        proc.learning.store (learnBtn.isOn());
    };
    monoBtn.onClick = [this]
    {
        if (monoAttachment != nullptr)
            monoAttachment->setValueAsCompleteGesture (monoBtn.isOn() ? 1.0f : 0.0f);
    };

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &loadBtn, &clearBtn, &learnBtn,
             &volRow, &panRow, &pitchRow, &startRow, &endRow, &chokeRow,
             &driveRow, &ceilRow, &monoBtn })
    {
        addAndMakeVisible (c);
        padsPage.push_back (c);
    }

    // --- страница SEQUENCER ---
    grid = std::make_unique<StepGrid> (*this);
    dragBtn = std::make_unique<MidiDragButton> (*this);

    patternChoice.attach (proc.apvts, "pattern");
    gridChoice.attach (proc.apvts, "grid");
    swingRow.attach (proc.apvts, "swing");
    lenRow.attach (proc.apvts, "seqlen");
    repeatChoice.attach (proc.apvts, "exportreps");

    if (auto* seqParam = proc.apvts.getParameter ("seqon"))
    {
        seqOnAttachment = std::make_unique<juce::ParameterAttachment> (*seqParam, [this] (float v) { seqOnBtn.setOn (v >= 0.5f); });
        seqOnAttachment->sendInitialUpdate();
    }

    seqOnBtn.onClick = [this]
    {
        if (seqOnAttachment != nullptr)
            seqOnAttachment->setValueAsCompleteGesture (seqOnBtn.isOn() ? 1.0f : 0.0f);
    };

    clearPatBtn.onClick = [this]
    {
        proc.clearPattern ((int) std::lround (proc.apvts.getRawParameterValue ("pattern")->load()));
        grid->repaint();
    };

    saveMidiBtn.onClick = [this] { saveMidiFile(); };
    importBtn.onClick = [this] { browseForMidi(); };

    for (juce::Component* c : std::initializer_list<juce::Component*> {
             &patternChoice, &gridChoice, &swingRow, &lenRow, &repeatChoice, &seqOnBtn, &clearPatBtn,
             &importBtn, &saveMidiBtn, dragBtn.get(), grid.get() })
    {
        addAndMakeVisible (c);
        seqPage.push_back (c);
    }

    selectPad (0);
    showPage (0);
    setSize (windowW, windowH);
    startTimerHz (15);   // намеренно «рваный» ретро-темп
}

DrumRackEditor::~DrumRackEditor()
{
    stopTimer();
    monoAttachment.reset();
    seqOnAttachment.reset();
    setLookAndFeel (nullptr);
}

//==============================================================================
void DrumRackEditor::showPage (int newPage)
{
    page = newPage;

    for (auto* c : padsPage) c->setVisible (page == 0);
    for (auto* c : seqPage)  c->setVisible (page == 1);

    padsTab.setOn (page == 0);
    seqTab.setOn (page == 1);
    repaint();
}

void DrumRackEditor::selectPad (int index)
{
    selected = juce::jlimit (0, DrumRackProcessor::numPads - 1, index);

    if (proc.learning.load() && proc.learnPad.load() != selected)
    {
        proc.learning.store (false);
        learnBtn.setOn (false);
    }

    const auto id = juce::String (selected);
    volRow.attach   (proc.apvts, "vol" + id);
    panRow.attach   (proc.apvts, "pan" + id);
    pitchRow.attach (proc.apvts, "pitch" + id);
    startRow.attach (proc.apvts, "start" + id);
    endRow.attach   (proc.apvts, "end" + id);
    chokeRow.attach (proc.apvts, "choke" + id);
    driveRow.attach (proc.apvts, "drive" + id);
    ceilRow.attach  (proc.apvts, "ceil" + id);

    monoAttachment.reset();
    if (auto* monoParam = proc.apvts.getParameter ("mono" + id))
    {
        monoAttachment = std::make_unique<juce::ParameterAttachment> (*monoParam, [this] (float v) { monoBtn.setOn (v >= 0.5f); });
        monoAttachment->sendInitialUpdate();
    }

    for (int i = 0; i < DrumRackProcessor::numPads; ++i)
        pads[i]->isSelected = (i == selected);

    waveData = nullptr;   // перерисовать волну для нового пэда
    repaint();
}

void DrumRackEditor::browseForSample()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose a sample", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.ogg");

    juce::Component::SafePointer<DrumRackEditor> safeThis (this);
    const int pad = selected;

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safeThis, pad] (const juce::FileChooser& fc)
                          {
                              auto file = fc.getResult();
                              if (safeThis != nullptr && file.existsAsFile())
                              {
                                  safeThis->proc.loadSample (pad, file);
                                  safeThis->repaint();
                              }
                          });
}

void DrumRackEditor::saveMidiFile()
{
    const int pattern = juce::jlimit (0, DrumRackProcessor::numPatterns - 1,
                                      (int) std::lround (proc.apvts.getRawParameterValue ("pattern")->load()));

    chooser = std::make_unique<juce::FileChooser> ("Save pattern as MIDI",
                                                   juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                                       .getChildFile ("PEPEL DRUMMER pattern " + juce::String::charToString ((juce::juce_wchar) ('A' + pattern)) + ".mid"),
                                                   "*.mid");

    juce::Component::SafePointer<DrumRackEditor> safeThis (this);

    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safeThis, pattern] (const juce::FileChooser& fc)
                          {
                              auto file = fc.getResult();
                              if (safeThis != nullptr && file != juce::File())
                                  if (safeThis->proc.writePatternMidi (pattern, file.withFileExtension ("mid")))
                                      safeThis->setStatus ("saved " + file.withFileExtension ("mid").getFileName());
                          });
}

void DrumRackEditor::setStatus (const juce::String& text)
{
    statusText = text;
    statusFrames = 60;    // около 4 секунд
    repaint();
}

void DrumRackEditor::importMidi (const juce::File& file)
{
    const int pattern = juce::jlimit (0, DrumRackProcessor::numPatterns - 1,
                                      (int) std::lround (proc.apvts.getRawParameterValue ("pattern")->load()));
    const int n = proc.importPatternMidi (pattern, file);

    if (n > 0)       setStatus ("imported " + juce::String (n) + " notes");
    else if (n == 0) setStatus ("no notes on pad keys 36-51");
    else             setStatus ("cannot read midi file");

    grid->repaint();
}

void DrumRackEditor::browseForMidi()
{
    chooser = std::make_unique<juce::FileChooser> ("Import MIDI into the pattern", juce::File(), "*.mid;*.midi");

    juce::Component::SafePointer<DrumRackEditor> safeThis (this);

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safeThis] (const juce::FileChooser& fc)
                          {
                              auto file = fc.getResult();
                              if (safeThis != nullptr && file.existsAsFile())
                                  safeThis->importMidi (file);
                          });
}

//==============================================================================
// анимация (15 Гц)
void DrumRackEditor::timerCallback()
{
    advanceFrame();
    repaint();
}

void DrumRackEditor::advanceFrame()
{
    ++frame;

    if (statusFrames > 0)
        --statusFrames;

    static const float speeds[3] = { 0.35f, 0.6f, 0.25f };
    for (int i = 0; i < 3; ++i)
    {
        cloudX[i] += speeds[i] * 2.0f;
        if (cloudX[i] > (float) windowW)
            cloudX[i] = -60.0f;
    }

    for (int i = 0; i < DrumRackProcessor::numPads; ++i)
    {
        const int hits = proc.hitCounter[i].load();
        if (hits != lastHit[i])
        {
            lastHit[i] = hits;
            pads[i]->flash = 1.0f;
        }
        else
        {
            pads[i]->flash = pads[i]->flash > 0.08f ? pads[i]->flash * 0.55f : 0.0f;
        }
    }

    grid->playStep = proc.currentStep.load();

    const bool learning = proc.learning.load();
    if (learnBtn.isOn() != learning)
        learnBtn.setOn (learning);

    // волна выбранного сэмпла
    if (proc.getSample (selected) != waveData)
        rebuildWavePeaks();

    // редкие «сбои»: примерно раз в 4 секунды на 2-4 кадра
    if (glitchFrames > 0)
    {
        --glitchFrames;
        glitchY = rng.nextInt (windowH - 60);
        glitchH = 10 + rng.nextInt (24);
    }
    else if (--framesToGlitch <= 0)
    {
        glitchFrames = 2 + rng.nextInt (3);
        framesToGlitch = 45 + rng.nextInt (30);
        glitchY = rng.nextInt (windowH - 60);
        glitchH = 10 + rng.nextInt (24);
    }
}

void DrumRackEditor::prepareSnapshot (int pageIndex, int frames, int glitch)
{
    showPage (pageIndex);

    for (int i = 0; i < frames; ++i)
        advanceFrame();

    glitchFrames = glitch;
    glitchY = 150;
    glitchH = 22;
}

void DrumRackEditor::rebuildWavePeaks()
{
    waveData = proc.getSample (selected);
    wavePeaks.clear();

    if (waveData == nullptr)
        return;

    const int cols = wavePanel.getWidth() - 24 - 2;
    const int len = waveData->buffer.getNumSamples();
    if (cols <= 0 || len <= 0)
        return;

    const float* src = waveData->buffer.getReadPointer (0);
    wavePeaks.resize ((size_t) cols * 2);

    for (int c = 0; c < cols; ++c)
    {
        const int i0 = (int) ((juce::int64) c * len / cols);
        const int i1 = juce::jmax (i0 + 1, (int) ((juce::int64) (c + 1) * len / cols));
        float mn = 0.0f, mx = 0.0f;

        for (int i = i0; i < i1 && i < len; ++i)
        {
            mn = juce::jmin (mn, src[i]);
            mx = juce::jmax (mx, src[i]);
        }

        wavePeaks[(size_t) c * 2]     = mn;
        wavePeaks[(size_t) c * 2 + 1] = mx;
    }
}

//==============================================================================
void DrumRackEditor::rebuildBackground()
{
    background = juce::Image (juce::Image::RGB, windowW, windowH, false);
    juce::Graphics g (background);

    // небо и тёмный двор: полосы по 6 px, градиент квантован в 14 ступеней (как VHS)
    for (int y = 0; y < windowH; y += 6)
    {
        juce::Colour c;

        if (y < scene::horizon)
        {
            const float t = juce::jlimit (0.0f, 1.0f, (float) (y + 3) / (float) scene::horizon);
            const float q = juce::jmin (1.0f, std::floor (t * 14.0f) / 13.0f);
            c = juce::Colour (0xff04051A).interpolatedWith (juce::Colour (0xff1B2766), q);
        }
        else
        {
            const float t = juce::jlimit (0.0f, 1.0f, (float) (y - scene::horizon) / (float) (windowH - scene::horizon));
            const float q = juce::jmin (1.0f, std::floor (t * 14.0f) / 13.0f);
            c = juce::Colour (0xff0B1131).interpolatedWith (pal::night, q);
        }

        g.setColour (c);
        g.fillRect (0, y, windowW, 6);
    }

    // луна с ореолом
    {
        juce::ColourGradient halo (juce::Colour (0xffAEB9E8).withAlpha (0.28f), 440.0f, 30.0f,
                                   juce::Colour (0xffAEB9E8).withAlpha (0.0f), 470.0f, 30.0f, true);
        g.setGradientFill (halo);
        g.fillEllipse (410.0f, 0.0f, 60.0f, 60.0f);

        g.setColour (juce::Colour (0xffAEB9E8));
        g.fillEllipse (430.0f, 20.0f, 20.0f, 20.0f);
        g.setColour (juce::Colour (0xff8693C8));
        g.fillRect (436, 26, 3, 3);
        g.fillRect (442, 31, 4, 3);
        g.fillRect (440, 23, 2, 2);
    }

    // дальние дома и панельки
    for (const auto& b : scene::farBlocks)
    {
        g.setColour (juce::Colour (0xff0A0F33));
        g.fillRect (b.x, scene::horizon - b.h, b.w, b.h);
    }

    for (const auto& b : scene::mainBlocks)
    {
        g.setColour (juce::Colour (0xff060924));
        g.fillRect (b.x, scene::horizon - b.h, b.w, b.h);

        g.setColour (juce::Colour (0xff0C1236));
        scene::forEachWindow (b, [&g] (int wx, int wy) { g.fillRect (wx, wy, 4, 5); });
    }

    // радиовышка на крыше
    {
        g.setColour (juce::Colour (0xff03040C));
        g.drawLine (550.0f, 58.0f, 554.0f, 20.0f, 1.6f);
        g.drawLine (562.0f, 58.0f, 558.0f, 20.0f, 1.6f);
        for (int i = 0; i < 6; ++i)
        {
            const float y0 = 56.0f - (float) i * 6.5f, y1 = y0 - 6.5f;
            g.drawLine (551.0f + (float) i * 0.5f, y0, 561.0f - (float) i * 0.5f, y1, 1.0f);
            g.drawLine (561.0f - (float) i * 0.5f, y0, 551.0f + (float) i * 0.5f, y1, 1.0f);
        }
    }

    // деревья
    g.setColour (juce::Colour (0xff02030A));
    {
        juce::Random tr (7);
        scene::drawBranch (g, tr, 304.0f, (float) scene::horizon, -juce::MathConstants<float>::halfPi, 20.0f, 6);
        scene::drawBranch (g, tr, 742.0f, (float) scene::horizon, -juce::MathConstants<float>::halfPi, 17.0f, 6);
    }

    // фонарь
    g.setColour (juce::Colour (0xff02030A));
    g.fillRect (367, 62, 3, scene::horizon - 62);
    g.fillRect (367, 60, 14, 3);

    // колючая проволока над панелями и цепи по краям (фирменные элементы)
    drawBarbedWire (g, { -4.0f, 108.0f }, { 400.0f, 123.0f }, { 804.0f, 108.0f }, pal::steel);
    drawChain (g, { -8.0f, 26.0f }, { 50.0f, 20.0f }, { 104.0f, -6.0f }, pal::steel);
    drawChain (g, { 790.0f, -6.0f }, { 802.0f, 160.0f }, { 789.0f, 330.0f }, pal::steel);

    // пунктир слева и шахматная полоса снизу, как на референсе
    g.setColour (pal::steelDim.withAlpha (0.7f));
    for (int y = 132; y < 612; y += 14)
        g.fillRect (6, y, 4, 8);

    for (int i = 0; i < 124; ++i)
        for (int row = 0; row < 2; ++row)
            if (((i + row) & 1) == 0)
                g.fillRect (20 + i * 5, windowH - 14 + row * 5, 5, 5);
}

void DrumRackEditor::rebuildOverlays()
{
    // сканлайны: каждая 3-я строка, один раз в кэшированный Image
    scanlines = juce::Image (juce::Image::ARGB, windowW, windowH, true);
    {
        juce::Graphics g (scanlines);
        g.setColour (juce::Colours::black.withAlpha (0.26f));
        for (int y = 0; y < windowH; y += 3)
            g.fillRect (0, y, windowW, 1);
    }

    // виньетка по краям кадра
    vignette = juce::Image (juce::Image::ARGB, windowW, windowH, true);
    {
        juce::Graphics g (vignette);
        juce::ColourGradient vg (juce::Colours::transparentBlack, (float) windowW * 0.5f, (float) windowH * 0.5f,
                                 juce::Colours::black.withAlpha (0.55f), 0.0f, 0.0f, true);
        vg.addColour (0.62, juce::Colours::transparentBlack);
        g.setGradientFill (vg);
        g.fillRect (0, 0, windowW, windowH);
    }
}

void DrumRackEditor::resized()
{
    rebuildBackground();
    rebuildOverlays();

    // вкладки в шапке
    padsTab.setBounds (24, 74, 100, 33);
    seqTab.setBounds (130, 74, 140, 33);

    const int top = 118, colW = 370, left = 20, right = 410;

    // --- PADS ---
    padsPanel   = { left, top, colW, 500 };
    samplePanel = { right, top, colW, 235 };
    clipPanel   = { right, top + 245, colW, 150 };
    wavePanel   = { right, top + 405, colW, 95 };

    const int gx = padsPanel.getX() + 12, gy = padsPanel.getY() + 46;
    for (int i = 0; i < DrumRackProcessor::numPads; ++i)
    {
        const int col = i % 4;
        const int row = 3 - (i / 4);   // пэд 1 внизу слева, как на MPK Mini
        pads[i]->setBounds (gx + col * 86, gy + row * 84, 86, 84);
    }

    const int by = padsPanel.getY() + 448;
    loadBtn.setBounds  (gx, by, 108, 33);
    clearBtn.setBounds (gx + 116, by, 108, 33);
    learnBtn.setBounds (gx + 232, by, 108, 33);

    int y = samplePanel.getY() + 44;
    for (auto* row : { &volRow, &panRow, &pitchRow, &startRow, &endRow, &chokeRow })
    {
        row->setBounds (samplePanel.getX() + 12, y, samplePanel.getWidth() - 24, 30);
        y += 30;
    }

    driveRow.setBounds (clipPanel.getX() + 12, clipPanel.getY() + 44, clipPanel.getWidth() - 24, 30);
    ceilRow.setBounds  (clipPanel.getX() + 12, clipPanel.getY() + 74, clipPanel.getWidth() - 24, 30);
    monoBtn.setBounds  (clipPanel.getX() + 12, clipPanel.getY() + 106, 120, 33);

    // --- SEQUENCER ---
    seqCtrlPanel = { left, top, 760, 180 };
    seqGridPanel = { left, top + 190, 760, 310 };

    const int lx = left + 12, rx = left + 12 + 352 + 16, by2 = top + 138;
    patternChoice.setBounds (lx, top + 44, 352, 30);
    gridChoice.setBounds    (lx, top + 74, 352, 30);
    hintRect = { lx, top + 104, 352, 30 };

    swingRow.setBounds     (rx, top + 44, 344, 30);
    lenRow.setBounds       (rx, top + 74, 344, 30);
    repeatChoice.setBounds (rx, top + 104, 344, 30);

    seqOnBtn.setBounds    (lx, by2, 96, 33);
    clearPatBtn.setBounds (lx + 104, by2, 96, 33);
    importBtn.setBounds   (lx + 208, by2, 144, 33);
    dragBtn->setBounds    (rx, by2, 168, 33);
    saveMidiBtn.setBounds (rx + 176, by2, 168, 33);

    grid->setBounds (seqGridPanel.getX() + 12, seqGridPanel.getY() + 42, 736, 260);

    scopeRect = { 20, 626, 760, 46 };

    waveData = nullptr;
}

//==============================================================================
void DrumRackEditor::paintPanel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title, juce::Colour accent)
{
    drawPanelFrame (g, r, accent);

    // шапка панели: полоса, пунктир, красная звезда и название
    g.setColour (pal::navy2);
    g.fillRect (r.getX() + 4, r.getY() + 4, r.getWidth() - 8, 30);

    g.setColour (pal::steelDim);
    for (int x = r.getX() + 6; x < r.getRight() - 10; x += 8)
        g.fillRect (x, r.getY() + 36, 4, 1);

    drawStar (g, r.getX() + 19, r.getY() + 19, 7, accent);

    g.setColour (pal::lamp);
    g.setFont (pixelfont::title (13.0f));
    g.drawText (title, r.getX() + 36, r.getY() + 4, r.getWidth() - 50, 30, juce::Justification::centredLeft, false);
}

void DrumRackEditor::paintHeader (juce::Graphics& g)
{
    // тёмные облака ползут по небу
    const int cloudY[3] = { 6, 38, 18 };
    for (int i = 0; i < 3; ++i)
        drawSprite (g, sprites::cloud, sprites::cloudRows, (int) cloudX[i], cloudY[i], 4,
                    [] (char) { return pal::deepBlue.withAlpha (0.28f); });

    // звёзды мерцают: голубые и красные
    struct StarDef { int x, y, size; bool red; };
    static const StarDef stars[] = {
        { 404, 14, 9, false }, { 472, 58, 6, false }, { 590, 14, 11, false }, { 636, 44, 8, true },
        { 672, 12, 6, false }, { 786, 78, 9, true },  { 290, 20, 7, false },  { 14, 66, 8, true } };

    for (int i = 0; i < (int) (sizeof (stars) / sizeof (stars[0])); ++i)
    {
        const int phase = (frame + i * 3) % 10;
        if (phase == 9)
            continue;

        const int size = stars[i].size + (phase < 5 ? 0 : -2);
        drawStar (g, stars[i].x, stars[i].y, size, stars[i].red ? pal::red : pal::lamp);
    }

    // окна панелек: гаснут и зажигаются, редкие красные
    for (const auto& b : scene::mainBlocks)
        scene::forEachWindow (b, [this, &g] (int wx, int wy)
        {
            juce::uint32 h = (juce::uint32) (wx * 73856093) ^ (juce::uint32) (wy * 19349663)
                             ^ (juce::uint32) ((frame / 40 + wx / 8) * 83492791);
            h *= 2654435761u;
            const int v = (int) ((h >> 16) % 100);

            if (v >= 24)
                return;

            const auto c = v < 3 ? pal::red : (v < 12 ? pal::lamp : pal::lampSoft);
            g.setColour (c.withAlpha (0.22f));
            g.fillRect (wx - 1, wy - 1, 6, 7);
            g.setColour (c);
            g.fillRect (wx, wy, 4, 5);
        });

    // фонарь: пульсирующее свечение
    {
        const float pulse = 0.85f + 0.15f * std::sin ((float) frame * 0.4f);
        juce::ColourGradient glow (pal::lamp.withAlpha (0.55f * pulse), 380.0f, 62.0f,
                                   pal::lamp.withAlpha (0.0f), 428.0f, 62.0f, true);
        g.setGradientFill (glow);
        g.fillEllipse (332.0f, 14.0f, 96.0f, 96.0f);
        g.setColour (pal::white);
        g.fillRect (374, 59, 10, 3);
    }

    // красный огонь на вышке
    if ((frame / 7) % 2 == 0)
    {
        g.setColour (pal::red.withAlpha (0.35f));
        g.fillEllipse (548.0f, 10.0f, 16.0f, 16.0f);
        g.setColour (pal::red);
        g.fillRect (555, 17, 3, 3);
    }

    // заголовок с хроматической аберрацией (при сбое сдвиг больше)
    const int shift = glitchFrames > 0 ? 4 : 2;
    const juce::Rectangle<int> t (24, 22, 380, 34);
    g.setFont (pixelfont::title (28.0f));

    g.setColour (pal::glitchBlue.withAlpha (0.9f));
    g.drawText ("PEPEL DRUMMER", t.translated (shift, 0), juce::Justification::topLeft, false);
    g.setColour (pal::glitchRed.withAlpha (0.9f));
    g.drawText ("PEPEL DRUMMER", t.translated (-shift, 0), juce::Justification::topLeft, false);
    g.setColour (pal::white);
    g.drawText ("PEPEL DRUMMER", t, juce::Justification::topLeft, false);

    g.setColour (pal::textDim);
    g.setFont (pixelfont::vhs (22.0f));
    g.drawText ("16 pads / step seq / clipper", 26, 52, 300, 20, juce::Justification::topLeft, false);

    // иероглифы 灰愛 вверху справа
    const juce::String kanji (juce::CharPointer_UTF8 ("\xE7\x81\xB0\xE6\x84\x9B"));
    const juce::Rectangle<int> kr (688, 4, 100, 56);
    const int ks = glitchFrames > 0 ? 5 : 3;
    g.setFont (pixelfont::kanji (46.0f));

    g.setColour (pal::glitchBlue.withAlpha (0.8f));
    g.drawText (kanji, kr.translated (ks, 0), juce::Justification::centred, false);
    g.setColour (pal::glitchRed.withAlpha (0.8f));
    g.drawText (kanji, kr.translated (-ks, 0), juce::Justification::centred, false);
    g.setColour (pal::white);
    g.drawText (kanji, kr, juce::Justification::centred, false);
}

void DrumRackEditor::paintPadInfo (juce::Graphics& g)
{
    const auto name = proc.getSampleName (selected);
    const int note = proc.padNote[selected].load();
    const int x = padsPanel.getX() + 14;
    const int y = padsPanel.getY() + 46 + 336 + 4;

    g.setColour (pal::text);
    g.setFont (pixelfont::vhs (26.0f));
    g.drawText ("Pad " + juce::String (selected + 1).paddedLeft ('0', 2) + "  " + (name.isEmpty() ? juce::String ("- empty -") : name),
                x, y, padsPanel.getWidth() - 28, 28, juce::Justification::centredLeft, true);

    g.setColour (pal::textDim);
    g.setFont (pixelfont::vhs (24.0f));
    g.drawText (proc.learning.load() ? juce::String ("hit a pad on your controller...")
                                     : "MIDI note " + juce::String (note) + "  (" + juce::MidiMessage::getMidiNoteName (note, true, true, 5) + ")",
                x, y + 26, padsPanel.getWidth() - 28, 26, juce::Justification::centredLeft, true);
}

void DrumRackEditor::paintWave (juce::Graphics& g)
{
    const auto inner = wavePanel.withTrimmedTop (42).reduced (12, 0).withTrimmedBottom (10);

    g.setColour (pal::night);
    g.fillRect (inner);
    g.setColour (pal::frame);
    g.drawRect (inner, 1);

    const int midY = inner.getCentreY();
    g.setColour (pal::frame.withAlpha (0.6f));
    g.fillRect (inner.getX() + 1, midY, inner.getWidth() - 2, 1);

    const int cols = (int) wavePeaks.size() / 2;
    if (cols == 0)
    {
        g.setColour (pal::textDim);
        g.setFont (pixelfont::vhs (22.0f));
        g.drawText ("no sample", inner, juce::Justification::centred, false);
        return;
    }

    const int amp = inner.getHeight() / 2 - 3;
    g.setColour (pal::lampSoft);
    for (int c = 0; c < cols && c < inner.getWidth() - 2; ++c)
    {
        const int top = midY - (int) (wavePeaks[(size_t) c * 2 + 1] * (float) amp);
        const int bot = midY - (int) (wavePeaks[(size_t) c * 2] * (float) amp);
        g.fillRect (inner.getX() + 1 + c, top, 1, juce::jmax (1, bot - top + 1));
    }

    // метки Start и End
    const auto id = juce::String (selected);
    const float s = proc.apvts.getRawParameterValue ("start" + id)->load();
    const float e = proc.apvts.getRawParameterValue ("end" + id)->load();
    g.setColour (pal::red);
    g.fillRect (inner.getX() + 1 + (int) (s * (float) (inner.getWidth() - 4)), inner.getY() + 1, 2, inner.getHeight() - 2);
    g.fillRect (inner.getX() + 1 + (int) (e * (float) (inner.getWidth() - 4)), inner.getY() + 1, 2, inner.getHeight() - 2);
}

void DrumRackEditor::paintScope (juce::Graphics& g)
{
    // осциллограф: волна звука на выходе
    g.setColour (pal::night);
    g.fillRect (scopeRect);
    g.setColour (pal::frame);
    g.drawRect (scopeRect, 1);

    const int midY = scopeRect.getCentreY();
    g.setColour (pal::frame.withAlpha (0.5f));
    g.fillRect (scopeRect.getX() + 1, midY, scopeRect.getWidth() - 2, 1);
    for (int x = scopeRect.getX() + 40; x < scopeRect.getRight() - 4; x += 40)
        g.fillRect (x, scopeRect.getY() + 3, 1, scopeRect.getHeight() - 6);

    const int amp = scopeRect.getHeight() / 2 - 4;
    const int pos = proc.scopePos.load();

    for (int i = 0; i < DrumRackProcessor::scopeSize; ++i)
    {
        const int idx = (pos + i) % DrumRackProcessor::scopeSize;
        const float mn = juce::jlimit (-1.0f, 0.0f, proc.scopeMin[idx].load() * 1.25f);
        const float mx = juce::jlimit (0.0f, 1.0f, proc.scopeMax[idx].load() * 1.25f);

        const int x = scopeRect.getX() + 1 + i * 2;
        const int top = midY - (int) (mx * (float) amp);
        const int bot = midY - (int) (mn * (float) amp);
        const int h = juce::jmax (1, bot - top + 1);

        g.setColour (pal::lamp.withAlpha (0.16f));
        g.fillRect (x, top - 2, 2, h + 4);
        g.setColour (pal::lampSoft);
        g.fillRect (x, top, 2, h);

        if (mx >= 0.99f)          // клиппинг подсвечивается красным
        {
            g.setColour (pal::red);
            g.fillRect (x, top, 2, 2);
        }
    }

    drawStar (g, scopeRect.getX() + 10, midY, 6, pal::red);
    drawStar (g, scopeRect.getRight() - 10, midY, 6, pal::red);

    // подпись внизу справа
    const juce::String sig (juce::CharPointer_UTF8 ("by \xD0\x9F\xD0\x95\xD0\x9F\xD0\x95L"));
    const juce::Rectangle<int> sr (windowW - 20 - 160, windowH - 25, 160, 14);
    g.setFont (pixelfont::title (10.0f));
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawText (sig, sr.translated (1, 1), juce::Justification::centredRight, false);
    g.setColour (pal::steel);
    g.drawText (sig, sr, juce::Justification::centredRight, false);
}

void DrumRackEditor::paint (juce::Graphics& g)
{
    g.drawImageAt (background, 0, 0);

    paintHeader (g);

    if (page == 0)
    {
        paintPanel (g, padsPanel, "PADS", pal::red);
        paintPanel (g, samplePanel, "SAMPLE", pal::lampSoft);
        paintPanel (g, clipPanel, "CLIP / MONO", pal::red);
        paintPanel (g, wavePanel, "WAVE", pal::lampSoft);
        paintPadInfo (g);
        paintWave (g);
    }
    else
    {
        paintPanel (g, seqCtrlPanel, "SEQUENCER", pal::red);
        paintPanel (g, seqGridPanel, "STEPS", pal::lampSoft);

        g.setFont (pixelfont::vhs (20.0f));
        g.setColour (statusFrames > 0 ? pal::lamp : pal::textDim);
        g.drawText (statusFrames > 0 ? statusText : juce::String ("drag .mid to FL, then turn SEQ off"),
                    hintRect, juce::Justification::centredLeft, true);
    }

    paintScope (g);
}

void DrumRackEditor::paintOverChildren (juce::Graphics& g)
{
    // сканлайны (кэшированный Image) и виньетка
    g.drawImageAt (scanlines, 0, 0);
    g.drawImageAt (vignette, 0, 0);

    // плёночное зерно: случайные точки 2x1 за кадр
    juce::Random grain (frame * 7919 + 13);
    for (int i = 0; i < 36; ++i)
    {
        g.setColour (grain.nextBool() ? pal::white.withAlpha (0.4f) : pal::deepBlue.withAlpha (0.5f));
        g.fillRect (grain.nextInt (windowW), grain.nextInt (windowH), 2, 1);
    }

    // редкие сбои: полупрозрачная светлая полоса с красно-синими разрывами
    if (glitchFrames > 0)
    {
        g.setColour (pal::lamp.withAlpha (0.14f));
        g.fillRect (0, glitchY, windowW, glitchH);

        juce::Random tear (frame * 104729 + glitchY);
        for (int k = 0; k < 4; ++k)
        {
            const int w = 40 + tear.nextInt (170);
            const int x = tear.nextInt (windowW - w);
            const int y = glitchY + tear.nextInt (juce::jmax (1, glitchH - 4));
            const int h = 2 + tear.nextInt (4);

            g.setColour (pal::glitchBlue.withAlpha (0.55f));
            g.fillRect (x - 4, y, w, h);
            g.setColour (pal::glitchRed.withAlpha (0.55f));
            g.fillRect (x + 4, y, w, h);
        }
    }
}
