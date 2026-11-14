// Проверка DSP без DAW:  PepelDrummer_DspTest
#include <JuceHeader.h>
#include "../Source/PluginProcessor.h"
#include <cstdio>

struct TestPlayHead : juce::AudioPlayHead
{
    double ppq = 0.0, bpm = 120.0; bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo i; i.setBpm (bpm); i.setPpqPosition (ppq); i.setIsPlaying (playing); return i;
    }
};

static juce::File makeSample (const juce::String& name, float ampL, float ampR, int length = 1000)
{
    auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (name + ".wav");
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::FileOutputStream> os (f.createOutputStream());
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), 44100.0, 2, 16, {}, 0));
    os.release();
    juce::AudioBuffer<float> b (2, length);
    for (int i = 0; i < length; ++i) { b.setSample (0, i, ampL); b.setSample (1, i, ampR); }
    w->writeFromAudioSampleBuffer (b, 0, length);
    return f;
}

static void setParam (DrumRackProcessor& p, const juce::String& id, float value)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (value));
}

static std::vector<long> run (DrumRackProcessor& p, TestPlayHead& ph, int totalSamples)
{
    std::vector<long> onsets;
    juce::AudioBuffer<float> buf (2, 512);
    long done = 0; bool prevZero = true;
    while (done < totalSamples)
    {
        ph.ppq = (double) done * ph.bpm / 60.0 / 44100.0;
        juce::MidiBuffer m;
        p.processBlock (buf, m);
        for (int i = 0; i < 512; ++i)
        {
            const bool zero = std::abs (buf.getSample (0, i)) < 1e-6f;
            if (! zero && prevZero) onsets.push_back (done + i);
            prevZero = zero;
        }
        done += 512;
    }
    return onsets;
}

static void print (const char* name, const std::vector<long>& v)
{
    std::printf ("%s:", name); for (auto x : v) std::printf (" %ld", x); std::printf ("\n");
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    DrumRackProcessor p;
    TestPlayHead ph;
    p.setPlayHead (&ph);
    p.prepareToPlay (44100.0, 512);

    p.loadSample (0, makeSample ("t_a", 0.5f, 0.5f));
    p.loadSample (1, makeSample ("t_b", 0.25f, 0.25f));

    p.setStep (0, 0, 0, true); p.setStep (0, 0, 4, true);
    print ("test1 steps 0,4 (expect 0 ~22050)", run (p, ph, 30000));

    p.clearPattern (0); p.setStep (0, 1, 1, true);
    setParam (p, "swing", 75.0f);
    print ("test2 swing75 step1 (expect ~8269)", run (p, ph, 12000));

    setParam (p, "swing", 50.0f);
    print ("test3 no swing step1 (expect ~5512)", run (p, ph, 12000));

    ph.playing = false;
    print ("test4 stopped (expect none)", run (p, ph, 12000));
    ph.playing = true;

    p.clearPattern (0); p.setStep (0, 0, 0, true);
    setParam (p, "seqlen", 4.0f);
    print ("test5 len4 (expect 0 ~22050 ~44100)", run (p, ph, 50000));
    setParam (p, "seqlen", 16.0f);

    p.clearPattern (0); p.setStep (0, 1, 1, true);
    setParam (p, "grid", 2.0f);   // 1/32
    print ("test6 grid 1/32 step1 (expect ~2756)", run (p, ph, 6000));
    setParam (p, "grid", 1.0f);

    // клиппер: вход 0.5, Drive +12 dB, Ceiling -6 dB -> потолок ~0.501
    {
        p.prepareToPlay (44100.0, 512);
        setParam (p, "seqon", 0.0f);
        setParam (p, "drive0", 12.0f);
        setParam (p, "ceil0", -6.0f);
        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer on; on.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 127), 0);
        p.processBlock (buf, on);
        std::printf ("test7 clip peak (expect ~0.501): %.3f\n", buf.getMagnitude (0, 0, 512));
        setParam (p, "drive0", 0.0f);
        setParam (p, "ceil0", 0.0f);
    }

    // mono: L=0.8, R=0.2 -> с Mono оба канала 0.5, без Mono разные
    {
        p.loadSample (2, makeSample ("t_c", 0.8f, 0.2f));
        auto peaks = [&p] (bool mono)
        {
            setParam (p, "mono2", mono ? 1.0f : 0.0f);
            p.prepareToPlay (44100.0, 512);
            juce::AudioBuffer<float> buf (2, 512);
            juce::MidiBuffer on; on.addEvent (juce::MidiMessage::noteOn (1, 38, (juce::uint8) 127), 0);
            p.processBlock (buf, on);
            return std::make_pair (buf.getSample (0, 300), buf.getSample (1, 300));
        };
        auto a = peaks (false), b = peaks (true);
        std::printf ("test8 mono off L/R (expect 0.800/0.200): %.3f/%.3f\n", a.first, a.second);
        std::printf ("test8 mono on  L/R (expect 0.500/0.500): %.3f/%.3f\n", b.first, b.second);
    }

    // экспорт паттерна в MIDI: шаг 1 со свингом 75% на 1/16 -> тик 360
    {
        p.clearPattern (0);
        p.setStep (0, 0, 0, true);
        p.setStep (0, 1, 1, true);
        p.setStep (0, 3, 2, true);
        setParam (p, "swing", 75.0f);
        setParam (p, "seqlen", 16.0f);
        auto f = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("t_pattern.mid");
        const bool ok = p.writePatternMidi (0, f);

        juce::MidiFile mf;
        juce::FileInputStream in (f);
        const bool read = mf.readFrom (in);
        std::printf ("test9 midi written/read (expect 1/1): %d/%d, ticks/qn=%d\n", (int) ok, (int) read, mf.getTimeFormat());
        if (read && mf.getNumTracks() > 0)
        {
            auto* t = mf.getTrack (0);
            for (int i = 0; i < t->getNumEvents(); ++i)
            {
                auto& m = t->getEventPointer (i)->message;
                if (m.isNoteOn())
                    std::printf ("   note %d at tick %.0f (expect 36@0, 37@360, 39@480)\n", m.getNoteNumber(), m.getTimeStamp());
            }
        }
    }

    // осциллограф получил данные
    {
        float mx = 0.0f;
        for (int i = 0; i < DrumRackProcessor::scopeSize; ++i)
            mx = juce::jmax (mx, p.scopeMax[i].load());
        std::printf ("test10 scope max (expect > 0): %.3f\n", mx);
    }
    return 0;
}
