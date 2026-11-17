#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "Theme.h"

class DrumRackEditor : public juce::AudioProcessorEditor,
                       private juce::Timer
{
public:
    explicit DrumRackEditor (DrumRackProcessor&);
    ~DrumRackEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    // для инструмента снимков интерфейса (без цикла событий)
    void prepareSnapshot (int pageIndex, int frames, int glitchFrames);

    static constexpr int windowW = 800;
    static constexpr int windowH = 690;

private:
    class PadButton;
    class StepGrid;
    class MidiDragButton;
    friend class PadButton;
    friend class StepGrid;

    void timerCallback() override;
    void advanceFrame();
    void selectPad (int index);
    void browseForSample();
    void saveMidiFile();
    void browseForMidi();
    void importMidi (const juce::File& file);
    void setStatus (const juce::String& text);
    void showPage (int newPage);
    void rebuildBackground();
    void rebuildOverlays();
    void rebuildWavePeaks();

    void paintHeader (juce::Graphics&);
    void paintPanel (juce::Graphics&, juce::Rectangle<int> r, const juce::String& title, juce::Colour accent);
    void paintPadInfo (juce::Graphics&);
    void paintWave (juce::Graphics&);
    void paintScope (juce::Graphics&);

    PixelLookAndFeel laf;   // объявлен первым: переживает все компоненты

    DrumRackProcessor& proc;
    int selected = 0;
    int page = 0;
    int frame = 0;

    // вкладки
    PixelButton padsTab { "PADS", pal::lampSoft };
    PixelButton seqTab  { "SEQUENCER", pal::lampSoft };

    // страница PADS
    std::unique_ptr<PadButton> pads[DrumRackProcessor::numPads];
    int lastHit[DrumRackProcessor::numPads] {};

    PixelButton loadBtn  { "LOAD", pal::lampSoft };
    PixelButton clearBtn { "CLEAR", pal::red };
    PixelButton learnBtn { "LEARN", pal::lampSoft, true };

    ParamRow volRow, panRow, pitchRow, startRow, endRow, chokeRow;
    ParamRow driveRow, ceilRow;
    PixelButton monoBtn { "MONO", pal::lampSoft, true };
    std::unique_ptr<juce::ParameterAttachment> monoAttachment;

    // страница SEQUENCER
    ChoiceRow patternChoice, gridChoice;
    ParamRow swingRow, lenRow;
    ChoiceRow repeatChoice;
    PixelButton importBtn { "IMPORT .MID", pal::steel };
    juce::String statusText;
    int statusFrames = 0;
    PixelButton seqOnBtn { "SEQ ON", pal::lampSoft, true };
    PixelButton clearPatBtn { "CLEAR", pal::red };
    PixelButton saveMidiBtn { "SAVE .MID", pal::steel };
    std::unique_ptr<MidiDragButton> dragBtn;
    std::unique_ptr<StepGrid> grid;
    std::unique_ptr<juce::ParameterAttachment> seqOnAttachment;

    std::vector<juce::Component*> padsPage, seqPage;
    std::unique_ptr<juce::FileChooser> chooser;

    // области
    juce::Rectangle<int> padsPanel, samplePanel, clipPanel, wavePanel, seqCtrlPanel, seqGridPanel, scopeRect, hintRect;

    // кэш картинок
    juce::Image background, scanlines, vignette;

    // волна выбранного сэмпла
    std::shared_ptr<const SampleData> waveData;
    std::vector<float> wavePeaks;   // пары min/max на пиксель

    // анимация
    juce::Random rng;
    float cloudX[3] { 60.0f, 330.0f, 590.0f };
    int glitchFrames = 0;
    int framesToGlitch = 55;
    int glitchY = 40, glitchH = 18;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumRackEditor)
};
