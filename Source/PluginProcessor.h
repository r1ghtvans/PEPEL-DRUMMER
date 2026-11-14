#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <memory>

// Загруженный сэмпл (всегда 2 канала)
struct SampleData
{
    juce::AudioBuffer<float> buffer;
    double sampleRate = 44100.0;
};

class DrumRackProcessor : public juce::AudioProcessor
{
public:
    static constexpr int numPads = 16;
    static constexpr int numPatterns = 4;
    static constexpr int firstNote = 36;    // пэд 1 = нота 36 (в FL называется C3), пэд 16 = нота 51
    static constexpr int scopeSize = 380;   // столбцов осциллографа

    DrumRackProcessor();
    ~DrumRackProcessor() override = default;

    // --- AudioProcessor ---
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "PEPEL DRUMMER"; }
    bool acceptsMidi() const override { return true; }
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

    // --- вызывается из интерфейса (message thread) ---
    bool loadSample (int pad, const juce::File& file);
    void clearSample (int pad);
    juce::String getSampleName (int pad) const;
    std::shared_ptr<const SampleData> getSample (int pad) const { return std::atomic_load (&samples[pad]); }
    void previewPad (int pad, float velocity);   // клик мышью по пэду

    // Паттерн секвенсора в MIDI-файл (его можно перетащить в Piano roll или плейлист FL)
    bool writePatternMidi (int pattern, const juce::File& target) const;

    static juce::StringArray patternChoices() { return { "A", "B", "C", "D" }; }
    static juce::StringArray gridChoices()    { return { "1/8", "1/16", "1/32" }; }

    // --- секвенсор: 16 шагов, по биту на шаг ---
    bool getStep (int pattern, int pad, int step) const;
    void setStep (int pattern, int pad, int step, bool on);
    void clearPattern (int pattern);
    std::atomic<int> stepMask[numPatterns][numPads];
    std::atomic<int> currentStep { -1 };       // для подсветки, -1 когда транспорт стоит

    // --- для интерфейса ---
    juce::AudioProcessorValueTreeState apvts;
    std::atomic<int>   padNote[numPads];       // MIDI-нота каждого пэда
    std::atomic<int>   hitCounter[numPads];    // растёт при каждом ударе (подсветка)
    std::atomic<bool>  learning { false };     // режим MIDI Learn
    std::atomic<int>   learnPad { -1 };
    std::atomic<float> outputLevel { 0.0f };   // уровень выхода для анимации

    // осциллограф: кольцевой буфер (min/max на столбец)
    std::atomic<float> scopeMin[scopeSize];
    std::atomic<float> scopeMax[scopeSize];
    std::atomic<int>   scopePos { 0 };

private:
    struct Voice
    {
        bool   active = false;
        double pos = 0.0;
        float  velocity = 1.0f;
        bool   choking = false;
        float  chokeGain = 1.0f;
    };

    struct Pending
    {
        bool active = false;
        int pad = 0;
        float velocity = 1.0f;
        juce::int64 due = 0;
    };

    static BusesProperties makeBuses();
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    double currentStepLength() const;   // длина шага в долях четверти
    void handleMidi (const juce::MidiMessage& m);
    void startVoice (int pad, float velocity);
    void addPending (int pad, float velocity, juce::int64 due);
    void firePending (juce::int64 now);
    juce::int64 nextPendingDue() const;
    void scheduleSteps (double ppqStart, int numSamples, double ppqPerSample, juce::int64 blockStart);
    void renderRange (juce::AudioBuffer<float>& buffer, int startSample, int endSample);
    void updateScope (const juce::AudioBuffer<float>& buffer);

    Voice  voices[numPads];
    double currentSampleRate = 44100.0;

    std::shared_ptr<const SampleData> samples[numPads];   // доступ через std::atomic_load/store
    juce::String samplePaths[numPads];
    mutable juce::CriticalSection pathLock;

    std::atomic<float>* volP[numPads]   {};
    std::atomic<float>* panP[numPads]   {};
    std::atomic<float>* pitchP[numPads] {};
    std::atomic<float>* startP[numPads] {};
    std::atomic<float>* endP[numPads]   {};
    std::atomic<float>* chokeP[numPads] {};
    std::atomic<float>* driveP[numPads] {};
    std::atomic<float>* ceilP[numPads]  {};
    std::atomic<float>* monoP[numPads]  {};
    std::atomic<float>* seqOnP   = nullptr;
    std::atomic<float>* swingP   = nullptr;
    std::atomic<float>* patternP = nullptr;
    std::atomic<float>* seqLenP  = nullptr;
    std::atomic<float>* gridP    = nullptr;

    float smoothDrive[numPads] {};   // сглаженные значения клиппера (без щелчков)
    float smoothCeil[numPads]  {};

    static constexpr int maxPending = 256;
    Pending pending[maxPending];
    juce::int64 sampleCounter = 0;
    juce::int64 currentAbs = 0;
    double bpm = 120.0;
    bool wasPlaying = false;

    float scopeAccMin = 0.0f, scopeAccMax = 0.0f;
    int scopeCount = 0;

    std::atomic<float> guiTrigger[numPads];

    juce::AudioFormatManager formatManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrumRackProcessor)
};
