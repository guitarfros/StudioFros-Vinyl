#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <vector>

class VinylProcessor : public juce::AudioProcessor
{
public:
    VinylProcessor();
    ~VinylProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
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

    juce::AudioProcessorParameter* getBypassParameter() const override
    {
        return apvts.getParameter ("bypass");
    }

    //==========================================================================
    // Presets (botóns SAVE / SAVE AS / DELETE / INIT)
    static juce::File getPresetDir();
    juce::StringArray getUserPresetNames() const;
    static juce::StringArray getFactoryPresetNames();

    bool saveUserPreset (const juce::String& name);
    bool loadUserPreset (const juce::String& name);
    bool deleteUserPreset (const juce::String& name);
    void loadFactoryPreset (int index);
    void initDefaults();

    juce::String getCurrentPresetName() const;

    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;

    // Medidores e LEDs (os le o editor)
    std::atomic<float> inPeak { 0.0f };
    std::atomic<float> outPeak { 0.0f };
    std::atomic<bool> crackleFlag { false };

    // Nivel dos VU en unidades VU (0 VU = kVuReferenceDb dBFS RMS), con balística de VU
    std::atomic<float> inVu { -40.0f };
    std::atomic<float> outVu { -40.0f };
    static constexpr float kVuReferenceDb = -18.0f; // cambia isto para recalibrar o 0 VU

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void setParam (const char* id, float value);
    void setCurrentPresetName (const juce::String& name);

    std::atomic<float> *pInput, *pWarmth, *pCrackle, *pDust, *pHiss, *pWow, *pFlutter,
        *pWarp, *pAge, *pTone, *pWidth, *pMix, *pOutput, *pSpeed, *pSurface,
        *pMonoBass, *pRumble, *pPower, *pBypass;

    double currentSr = 44100.0;
    int baseDelaySamples = 220;

    std::array<std::vector<float>, 2> delayLine;
    int writePos = 0;
    float wowPhase1 = 0.f, wowPhase2 = 0.f, flutPhase1 = 0.f, flutPhase2 = 0.f, warpPhase = 0.f;

    std::array<float, 2> lpAge1 {}, lpAge2 {}, lpWarm {}, lpTone {}, lpBass {}, lpHiss {}, clickState {};
    float lpRumble = 0.f;

    juce::Random rng;
    float inPow = 0.f, outPow = 0.f; // potencia media suavizada para os VU
    juce::SmoothedValue<float> inGainSm, outGainSm, mixSm, powerSm;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VinylProcessor)
};
