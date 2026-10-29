#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

namespace
{
    // Presets de fábrica. speed: 0=33, 1=45, 2=78 | surface: 0=Clean, 1=Used, 2=Worn
    // width: 0.5 = neutro, 0 = mono | tone en dB | os demais controis de 0 a 1
    struct FactoryPreset
    {
        const char* name;
        int speed, surface;
        float warmth, crackle, dust, hiss, wow, flutter, warp, age, tone, width, mix;
        bool monoBass, rumble;
    };

    const FactoryPreset kFactory[] = {
        // nome              vel sup  warm  crack dust  hiss  wow   flut  warp  age   tone  width mix  mono  rumble
        { "Mint 33",           0, 0, 0.25f, 0.08f, 0.10f, 0.05f, 0.08f, 0.05f, 0.03f, 0.10f,  0.0f, 0.50f, 1.0f, false, false },
        { "Dusty 45",          1, 1, 0.40f, 0.30f, 0.35f, 0.20f, 0.20f, 0.15f, 0.12f, 0.40f,  0.0f, 0.50f, 1.0f, true,  false },
        { "Worn 78",           2, 2, 0.60f, 0.60f, 0.60f, 0.45f, 0.40f, 0.30f, 0.30f, 0.80f,  0.0f, 0.10f, 1.0f, true,  true  },
        { "Clean Reissue",     0, 0, 0.20f, 0.03f, 0.05f, 0.03f, 0.03f, 0.02f, 0.02f, 0.05f,  0.5f, 0.50f, 0.9f, true,  false },
        { "Gentle Glue",       0, 0, 0.30f, 0.00f, 0.02f, 0.02f, 0.02f, 0.02f, 0.00f, 0.10f,  0.0f, 0.50f, 0.6f, true,  false },
        { "Warm Jazz LP",      0, 1, 0.50f, 0.12f, 0.15f, 0.08f, 0.10f, 0.06f, 0.05f, 0.25f, -1.5f, 0.45f, 1.0f, true,  false },
        { "Vintage Mono 33",   0, 1, 0.45f, 0.15f, 0.20f, 0.15f, 0.10f, 0.08f, 0.06f, 0.35f, -1.0f, 0.00f, 1.0f, true,  false },
        { "Soul Single 45",    1, 1, 0.45f, 0.20f, 0.25f, 0.12f, 0.12f, 0.10f, 0.08f, 0.30f,  1.0f, 0.35f, 1.0f, true,  false },
        { "Late Night Hiss",   0, 1, 0.35f, 0.05f, 0.10f, 0.60f, 0.08f, 0.05f, 0.05f, 0.40f, -2.0f, 0.50f, 1.0f, false, false },
        { "Crackle Only",      0, 1, 0.05f, 0.50f, 0.50f, 0.05f, 0.00f, 0.00f, 0.00f, 0.00f,  0.0f, 0.50f, 1.0f, false, false },
        { "Warped Record",     0, 1, 0.35f, 0.20f, 0.20f, 0.12f, 0.35f, 0.15f, 0.80f, 0.30f,  0.0f, 0.50f, 1.0f, false, false },
        { "Slow Wobble",       0, 1, 0.30f, 0.15f, 0.15f, 0.10f, 0.80f, 0.30f, 0.40f, 0.35f,  0.0f, 0.50f, 1.0f, false, false },
        { "Thrift Store Find", 1, 2, 0.40f, 0.50f, 0.55f, 0.25f, 0.25f, 0.20f, 0.30f, 0.50f, -1.0f, 0.50f, 1.0f, false, false },
        { "Dusty Attic",       1, 2, 0.50f, 0.55f, 0.75f, 0.30f, 0.20f, 0.15f, 0.35f, 0.60f, -2.5f, 0.40f, 1.0f, true,  true  },
        { "Basement Dub",      0, 2, 0.60f, 0.35f, 0.40f, 0.30f, 0.30f, 0.15f, 0.20f, 0.55f, -3.0f, 0.30f, 1.0f, true,  true  },
        { "Lo-Fi Bedroom",     0, 2, 0.55f, 0.30f, 0.35f, 0.35f, 0.45f, 0.25f, 0.25f, 0.70f, -4.0f, 0.25f, 0.85f, false, false },
        { "Old Jukebox",       1, 2, 0.65f, 0.45f, 0.40f, 0.40f, 0.30f, 0.30f, 0.20f, 0.65f, -2.0f, 0.05f, 1.0f, true,  false },
        { "Shellac Radio",     2, 1, 0.55f, 0.40f, 0.35f, 0.35f, 0.25f, 0.30f, 0.15f, 0.80f, -4.0f, 0.00f, 1.0f, true,  false },
        { "Gramophone 78",     2, 2, 0.70f, 0.70f, 0.65f, 0.50f, 0.35f, 0.35f, 0.30f, 0.90f, -5.0f, 0.00f, 1.0f, true,  true  },
    };
    constexpr int kNumFactory = (int) (sizeof (kFactory) / sizeof (kFactory[0]));

    constexpr float kBaseDelaySec = 0.005f; // atraso central da modulación (5 ms)

    float onePoleCoeff (float fc, float sr)
    {
        return 1.0f - std::exp (-juce::MathConstants<float>::twoPi * fc / sr);
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout VinylProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto addDb = [&] (const char* id, const char* name)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name, NormalisableRange<float> (-12.f, 12.f, 0.1f), 0.f));
    };
    auto add01 = [&] (const char* id, const char* name, float def)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (
            ParameterID { id, 1 }, name, NormalisableRange<float> (0.f, 1.f, 0.001f), def));
    };

    addDb ("input", "Input");
    add01 ("warmth", "Warmth", 0.3f);
    add01 ("crackle", "Crackle", 0.25f);
    add01 ("dust", "Dust", 0.25f);
    add01 ("hiss", "Hiss", 0.15f);
    add01 ("wow", "Wow", 0.2f);
    add01 ("flutter", "Flutter", 0.15f);
    add01 ("warp", "Warp", 0.1f);
    add01 ("age", "Age", 0.3f);
    p.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { "tone", 1 }, "Tone", NormalisableRange<float> (-6.f, 6.f, 0.1f), 0.f));
    add01 ("width", "Width", 0.5f);
    add01 ("mix", "Mix", 1.0f);
    addDb ("output", "Output");

    p.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { "speed", 1 }, "Speed", StringArray { "33", "45", "78" }, 0));
    p.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { "surface", 1 }, "Surface", StringArray { "Clean", "Used", "Worn" }, 1));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "mono_bass", 1 }, "Mono Bass", false));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "rumble", 1 }, "Rumble", false));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "power", 1 }, "Power", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { "bypass", 1 }, "Bypass", false));

    return { p.begin(), p.end() };
}

//==============================================================================
VinylProcessor::VinylProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pInput = get ("input");     pWarmth = get ("warmth");   pCrackle = get ("crackle");
    pDust = get ("dust");       pHiss = get ("hiss");       pWow = get ("wow");
    pFlutter = get ("flutter"); pWarp = get ("warp");       pAge = get ("age");
    pTone = get ("tone");       pWidth = get ("width");     pMix = get ("mix");
    pOutput = get ("output");   pSpeed = get ("speed");     pSurface = get ("surface");
    pMonoBass = get ("mono_bass"); pRumble = get ("rumble"); pPower = get ("power");
    pBypass = get ("bypass");
}

bool VinylProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void VinylProcessor::prepareToPlay (double sampleRate, int)
{
    currentSr = sampleRate;
    baseDelaySamples = (int) std::round (kBaseDelaySec * (float) sampleRate);

    const size_t size = (size_t) (sampleRate * 0.02) + 8;
    for (auto& d : delayLine)
        d.assign (size, 0.0f);
    writePos = 0;

    wowPhase1 = wowPhase2 = flutPhase1 = flutPhase2 = warpPhase = 0.f;
    lpAge1.fill (0.f); lpAge2.fill (0.f); lpWarm.fill (0.f); lpTone.fill (0.f);
    lpBass.fill (0.f); lpHiss.fill (0.f); clickState.fill (0.f);
    lpRumble = 0.f;
    inPow = outPow = 0.f;

    inGainSm.reset (sampleRate, 0.02);
    outGainSm.reset (sampleRate, 0.02);
    mixSm.reset (sampleRate, 0.02);
    powerSm.reset (sampleRate, 0.15);
    inGainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pInput->load()));
    outGainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    mixSm.setCurrentAndTargetValue (pMix->load());
    powerSm.setCurrentAndTargetValue (pPower->load() > 0.5f ? 1.f : 0.f);

    setLatencySamples (baseDelaySamples);
}

//==============================================================================
void VinylProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (buffer.getNumChannels() < 2)
        return;

    if (pBypass->load() > 0.5f)
    {
        inPow = outPow = 0.f;
        inVu.store (-40.f);
        outVu.store (-40.f);
        return;
    }

    const int numSamples = buffer.getNumSamples();
    float* ch[2] = { buffer.getWritePointer (0), buffer.getWritePointer (1) };
    const float sr = (float) currentSr;
    const float twoPi = juce::MathConstants<float>::twoPi;

    inGainSm.setTargetValue (juce::Decibels::decibelsToGain (pInput->load()));
    outGainSm.setTargetValue (juce::Decibels::decibelsToGain (pOutput->load()));
    mixSm.setTargetValue (pMix->load());
    powerSm.setTargetValue (pPower->load() > 0.5f ? 1.f : 0.f);

    const float warmth  = pWarmth->load();
    const float crackle = pCrackle->load();
    const float dust    = pDust->load();
    const float hiss    = pHiss->load();
    const float wow     = pWow->load();
    const float flutter = pFlutter->load();
    const float warp    = pWarp->load();
    const float age     = pAge->load();
    const float width   = pWidth->load() * 2.0f; // 0..2, 1 = neutro
    const bool monoBass = pMonoBass->load() > 0.5f;
    const bool rumbleOn = pRumble->load() > 0.5f;

    static const float rpms[3]      = { 33.333f, 45.0f, 78.0f };
    static const float surfaceMul[3] = { 0.3f, 1.0f, 2.5f };
    const float rpm  = rpms[juce::jlimit (0, 2, (int) pSpeed->load())];
    const float surf = surfaceMul[juce::jlimit (0, 2, (int) pSurface->load())];
    const float spd  = rpm / 33.333f;

    // Coeficientes dos filtros
    const float ageCut = 18000.f * std::pow (3500.f / 18000.f, age);
    const float aAge    = onePoleCoeff (ageCut, sr);
    const float aWarm   = onePoleCoeff (200.f, sr);
    const float aTone   = onePoleCoeff (3000.f, sr);
    const float aBass   = onePoleCoeff (150.f, sr);
    const float aHiss   = onePoleCoeff (7000.f, sr);
    const float aRumble = onePoleCoeff (35.f, sr);
    const float toneK   = juce::Decibels::decibelsToGain (pTone->load()) - 1.f;
    const float drive   = 1.f + 3.f * warmth;

    // Ruído
    const float crackleProb = crackle * 25.f * surf * (1.f + age) / sr;
    const float dustProb    = dust * 400.f * surf / sr;
    const float hissAmp     = hiss * 0.03f * (0.5f + 0.5f * surf) * (1.f + age);

    const size_t dlSize = delayLine[0].size();
    float peak = 0.f, outPk = 0.f;
    double sumIn = 0.0, sumOut = 0.0;

    for (int n = 0; n < numSamples; ++n)
    {
        const float inG  = inGainSm.getNextValue();
        const float outG = outGainSm.getNextValue();
        const float mix  = mixSm.getNextValue();
        const float pwr  = powerSm.getNextValue();

        // --- Modulación de pitch (wow / flutter / warp), común a L e R ---
        wowPhase1  += twoPi * 0.55f * spd / sr;
        wowPhase2  += twoPi * 1.30f * spd / sr;
        flutPhase1 += twoPi * 6.30f * spd / sr;
        flutPhase2 += twoPi * 11.7f * spd / sr;
        warpPhase  += twoPi * (rpm / 60.f) / sr; // unha volta do disco
        if (wowPhase1  > twoPi) wowPhase1  -= twoPi;
        if (wowPhase2  > twoPi) wowPhase2  -= twoPi;
        if (flutPhase1 > twoPi) flutPhase1 -= twoPi;
        if (flutPhase2 > twoPi) flutPhase2 -= twoPi;
        if (warpPhase  > twoPi) warpPhase  -= twoPi;

        const float modMs = wow * 1.5f * (0.7f * std::sin (wowPhase1) + 0.3f * std::sin (wowPhase2))
                          + flutter * 0.12f * (0.6f * std::sin (flutPhase1) + 0.4f * std::sin (flutPhase2))
                          + warp * 2.0f * std::sin (warpPhase);
        const float delaySamples = (float) baseDelaySamples + modMs * 0.001f * sr;

        float wet[2], dry[2];

        for (int c = 0; c < 2; ++c)
        {
            float x = ch[c][n] * inG;
            peak = juce::jmax (peak, std::abs (x));
            sumIn += (double) x * (double) x;

            auto& dl = delayLine[(size_t) c];
            dl[(size_t) writePos] = x;

            // Toma seca (atraso fixo, para manter a fase co húmido)
            dry[c] = dl[(size_t) ((writePos + (int) dlSize - baseDelaySamples) % (int) dlSize)];

            // Toma modulada (interpolación linear)
            float rp = (float) writePos - delaySamples;
            while (rp < 0.f) rp += (float) dlSize;
            const int i0 = (int) rp;
            const float fr = rp - (float) i0;
            const int i1 = (i0 + 1) % (int) dlSize;
            float y = dl[(size_t) i0] * (1.f - fr) + dl[(size_t) i1] * fr;

            // Warmth: saturación suave + reforzo de graves
            y = std::tanh (drive * y) / drive;
            lpWarm[(size_t) c] += aWarm * (y - lpWarm[(size_t) c]);
            y += warmth * 0.6f * lpWarm[(size_t) c];

            // Age: paso baixo de 12 dB/oct
            lpAge1[(size_t) c] += aAge * (y - lpAge1[(size_t) c]);
            lpAge2[(size_t) c] += aAge * (lpAge1[(size_t) c] - lpAge2[(size_t) c]);
            y = lpAge2[(size_t) c];

            // Tone: reforzo/corte de agudos
            lpTone[(size_t) c] += aTone * (y - lpTone[(size_t) c]);
            y += toneK * (y - lpTone[(size_t) c]);

            wet[c] = y;
        }

        // Mono Bass: graves en mono (por debaixo duns 150 Hz)
        if (monoBass)
        {
            lpBass[0] += aBass * (wet[0] - lpBass[0]);
            lpBass[1] += aBass * (wet[1] - lpBass[1]);
            const float monoLow = 0.5f * (lpBass[0] + lpBass[1]);
            wet[0] = (wet[0] - lpBass[0]) + monoLow;
            wet[1] = (wet[1] - lpBass[1]) + monoLow;
        }

        // Width (mid/side)
        {
            const float m = 0.5f * (wet[0] + wet[1]);
            const float s = 0.5f * (wet[0] - wet[1]) * width;
            wet[0] = m + s;
            wet[1] = m - s;
        }

        // Ruído de superficie: hiss, po e chasquidos
        float noise[2] = { 0.f, 0.f };
        for (int c = 0; c < 2; ++c)
        {
            const float w = rng.nextFloat() * 2.f - 1.f;
            lpHiss[(size_t) c] += aHiss * (w - lpHiss[(size_t) c]);
            noise[c] = lpHiss[(size_t) c] * hissAmp;
        }

        auto trigger = [&] (float prob, float amp, bool isCrackle)
        {
            if (rng.nextFloat() < prob)
            {
                const float r = rng.nextFloat();
                const float a = amp * r * r * (rng.nextBool() ? 1.f : -1.f);
                clickState[0] += a;
                clickState[1] += a * (0.5f + 0.5f * rng.nextFloat());
                if (isCrackle)
                    crackleFlag.store (true);
            }
        };
        trigger (crackleProb, 0.10f + 0.30f * crackle, true);
        trigger (dustProb, 0.02f + 0.05f * dust, false);

        if (rumbleOn)
        {
            const float w = rng.nextFloat() * 2.f - 1.f;
            lpRumble += aRumble * (w - lpRumble);
            noise[0] += lpRumble * 0.5f;
            noise[1] += lpRumble * 0.5f;
        }

        for (int c = 0; c < 2; ++c)
        {
            noise[c] += clickState[(size_t) c];
            clickState[(size_t) c] *= -0.6f; // chasquido curto, de ton agudo
            if (std::abs (clickState[(size_t) c]) < 1.0e-6f)
                clickState[(size_t) c] = 0.f;

            const float out = dry[c] * (1.f - mix) + wet[c] * mix + noise[c] * pwr;
            ch[c][n] = out * outG;
            outPk = juce::jmax (outPk, std::abs (ch[c][n]));
            sumOut += (double) ch[c][n] * (double) ch[c][n];
        }

        writePos = (writePos + 1) % (int) dlSize;
    }

    // Pico de entrada para o LED de clip
    float old = inPeak.load();
    while (peak > old && ! inPeak.compare_exchange_weak (old, peak)) {}
    old = outPeak.load();
    while (outPk > old && ! outPeak.compare_exchange_weak (old, outPk)) {}

    // VU: potencia media (RMS) con integración ~120 ms, calibrada a kVuReferenceDb
    {
        const float norm = (float) (numSamples * 2);
        const float msIn = (float) (sumIn / norm), msOut = (float) (sumOut / norm);
        const float coef = 1.0f - std::exp (-(float) numSamples / (0.12f * sr));
        inPow += (msIn - inPow) * coef;
        outPow += (msOut - outPow) * coef;
        inVu.store (10.0f * std::log10 (juce::jmax (inPow, 1.0e-10f)) - kVuReferenceDb);
        outVu.store (10.0f * std::log10 (juce::jmax (outPow, 1.0e-10f)) - kVuReferenceDb);
    }
}

//==============================================================================
// Presets
juce::File VinylProcessor::getPresetDir()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("GuitarFros")
                   .getChildFile ("StudioFros Vinyl")
                   .getChildFile ("Presets");
    dir.createDirectory();
    return dir;
}

juce::StringArray VinylProcessor::getUserPresetNames() const
{
    juce::StringArray names;
    for (auto& f : getPresetDir().findChildFiles (juce::File::findFiles, false, "*.xml"))
        names.add (f.getFileNameWithoutExtension());
    names.sortNatural();
    return names;
}

juce::StringArray VinylProcessor::getFactoryPresetNames()
{
    juce::StringArray names;
    for (const auto& p : kFactory)
        names.add (p.name);
    return names;
}

juce::String VinylProcessor::getCurrentPresetName() const
{
    return apvts.state.getProperty ("presetName").toString();
}

void VinylProcessor::setCurrentPresetName (const juce::String& name)
{
    apvts.state.setProperty ("presetName", name, nullptr);
}

bool VinylProcessor::saveUserPreset (const juce::String& rawName)
{
    const auto name = juce::File::createLegalFileName (rawName.trim());
    if (name.isEmpty())
        return false;

    setCurrentPresetName (name);
    if (auto xml = apvts.copyState().createXml())
        if (xml->writeTo (getPresetDir().getChildFile (name + ".xml")))
            return true;
    return false;
}

bool VinylProcessor::loadUserPreset (const juce::String& name)
{
    auto file = getPresetDir().getChildFile (name + ".xml");
    if (auto xml = juce::XmlDocument::parse (file))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            setCurrentPresetName (name);
            return true;
        }
    return false;
}

bool VinylProcessor::deleteUserPreset (const juce::String& name)
{
    const bool ok = getPresetDir().getChildFile (name + ".xml").deleteFile();
    if (ok && getCurrentPresetName() == name)
        setCurrentPresetName ({});
    return ok;
}

void VinylProcessor::setParam (const char* id, float value)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void VinylProcessor::initDefaults()
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            rp->setValueNotifyingHost (rp->getDefaultValue());
    setCurrentPresetName ({});
}

void VinylProcessor::loadFactoryPreset (int index)
{
    if (index < 0 || index >= kNumFactory)
        return;

    const auto& p = kFactory[index];
    initDefaults();
    setParam ("speed", (float) p.speed);
    setParam ("surface", (float) p.surface);
    setParam ("warmth", p.warmth);
    setParam ("crackle", p.crackle);
    setParam ("dust", p.dust);
    setParam ("hiss", p.hiss);
    setParam ("wow", p.wow);
    setParam ("flutter", p.flutter);
    setParam ("warp", p.warp);
    setParam ("age", p.age);
    setParam ("tone", p.tone);
    setParam ("width", p.width);
    setParam ("mix", p.mix);
    setParam ("mono_bass", p.monoBass ? 1.f : 0.f);
    setParam ("rumble", p.rumble ? 1.f : 0.f);
    setCurrentPresetName (p.name);
}

//==============================================================================
juce::AudioProcessorEditor* VinylProcessor::createEditor()
{
    return new VinylEditor (*this);
}

void VinylProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void VinylProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VinylProcessor();
}
