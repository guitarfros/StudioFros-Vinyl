#include "PluginEditor.h"
#include <BinaryData.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <utility>
#include <vector>

//==============================================================================
// Coordenadas medidas sobre a imaxe orixinal (1920 x 1280 px)
namespace layout
{
    constexpr float kImgW = 1920.f, kImgH = 1280.f;

    // VU xerados para este fondo. O pivote de cada agulla queda agochado baixo a tarxeta.
    // A resposta é a dun VU real: o ángulo vai coa tensión (ver vuToAngleDeg).
    struct VuMeter { float pivotX, pivotY, needleLen, faceL, faceR, faceTop, faceBottom; };
    constexpr VuMeter kVuIn  {  785.f, 274.f, 148.f,  656.f,  914.f, 66.f, 214.f };
    constexpr VuMeter kVuOut { 1141.f, 274.f, 148.f, 1012.f, 1270.f, 66.f, 214.f };

    // LEDs (centros medidos na imaxe)
    constexpr float kLedPowerX = 1366.f,   kLedPowerY = 132.f;
    constexpr float kLedBypassX = 1474.4f, kLedBypassY = 132.8f;
    constexpr float kLedClipX = 1582.f,    kLedClipY = 132.f;
    constexpr float kLedCrackleX = 1691.4f, kLedCrackleY = 132.6f;
    constexpr float kActiveX = 1654.5f, kActiveY = 1021.8f;

    // Barra de abaixo: selector de presets (frechas + visor)
    struct HotRect { float cx, cy, w, h; };
    constexpr HotRect kPresetPrev { 229.5f, 1169.5f, 109.f, 70.f };
    constexpr HotRect kPresetWin  { 493.0f, 1169.5f, 402.f, 70.f };
    constexpr HotRect kPresetNext { 756.5f, 1169.5f, 109.f, 70.f };
    const juce::Rectangle<float> presetText { 328.f, 1149.f, 330.f, 41.f }; // onde se escribe o nome
}

//==============================================================================
namespace
{
    constexpr float kPi = juce::MathConstants<float>::pi;

    constexpr float kTwoPi = juce::MathConstants<float>::twoPi;

    // Textura do knob (xira co knob): baquelita con moleteado, tapa de aceiro
    // cepillado, punteiro de pintura descascada, rabuñaduras, sucidade e ferruxe.
    juce::Image makeKnobTexture (int S, juce::int64 seed)
    {
        using juce::Colour;
        using juce::ColourGradient;
        namespace Colours = juce::Colours;

        juce::Image img (juce::Image::ARGB, S, S, true);
        juce::Random rnd (seed);

        const float c = (float) S * 0.5f;
        const float r = c - 1.5f;
        const float px = (float) S / 192.f; // factor para grosores finos

        auto rf = [&] (float lo, float hi) { return lo + (hi - lo) * rnd.nextFloat(); };
        auto polar = [&] (float a, float rad) { return juce::Point<float> (c + rad * std::sin (a), c - rad * std::cos (a)); };
        auto inDisc = [&] (float maxR)
        {
            const float a = rf (0.f, kTwoPi);
            const float d = maxR * std::sqrt (rnd.nextFloat());
            return polar (a, d);
        };
        auto blob = [&] (juce::Graphics& g, juce::Point<float> p, float rad, Colour col)
        {
            g.setGradientFill (ColourGradient (col, p.x, p.y, col.withAlpha (0.f), p.x + rad, p.y, true));
            g.fillEllipse (p.x - rad, p.y - rad, rad * 2.f, rad * 2.f);
        };

        {
            juce::Graphics g (img);
            juce::Path disc;
            disc.addEllipse (c - r, c - r, r * 2.f, r * 2.f);
            g.reduceClipRegion (disc);

            // Saia de baquelita
            g.setGradientFill (ColourGradient (Colour (0xff3a3835), c - r * 0.3f, c - r * 0.35f,
                                               Colour (0xff0a0a0a), c + r * 0.4f, c + r * 0.5f, true));
            g.fillEllipse (c - r, c - r, r * 2.f, r * 2.f);

            // Moleteado: regos escuros e cristas con brillo
            const int N = 56;
            for (int i = 0; i < N; ++i)
            {
                const float a = kTwoPi * (float) i / (float) N;
                g.setColour (Colours::black.withAlpha (0.7f));
                g.drawLine (juce::Line<float> (polar (a, r * 0.80f), polar (a, r * 0.995f)), r * 0.04f);
                const float a2 = a + kTwoPi / (float) N * 0.5f;
                g.setColour (Colour (0xff8d897e).withAlpha (0.14f));
                g.drawLine (juce::Line<float> (polar (a2, r * 0.82f), polar (a2, r * 0.99f)), r * 0.03f);
            }
            // Cristas desgastadas (deixan ver o material de abaixo)
            for (int i = 0; i < 34; ++i)
            {
                const float a = rf (0.f, kTwoPi);
                g.setColour (Colour (0xffa29e92).withAlpha (rf (0.12f, 0.42f)));
                g.drawLine (juce::Line<float> (polar (a, r * rf (0.86f, 0.93f)), polar (a, r * 0.99f)), r * 0.028f);
            }
            // Aresta do bordo desgastada
            g.setColour (Colour (0xff7d7a72).withAlpha (0.45f));
            g.drawEllipse (c - r * 0.985f, c - r * 0.985f, r * 1.97f, r * 1.97f, r * 0.03f);

            // Ranura entre saia e tapa
            g.setColour (Colours::black.withAlpha (0.85f));
            g.drawEllipse (c - r * 0.67f, c - r * 0.67f, r * 1.34f, r * 1.34f, r * 0.05f);

            // Tapa de aceiro cepillado
            const float rc = r * 0.62f;
            g.setGradientFill (ColourGradient (Colour (0xffb0b0b3), c - rc * 0.7f, c - rc * 0.8f,
                                               Colour (0xff4b4b4f), c + rc * 0.7f, c + rc * 0.8f, false));
            g.fillEllipse (c - rc, c - rc, rc * 2.f, rc * 2.f);
            for (int i = 0; i < 170; ++i)
            {
                const float rr = rc * rf (0.05f, 1.f);
                g.setColour ((rnd.nextBool() ? Colours::white : Colours::black).withAlpha (rf (0.03f, 0.13f)));
                g.drawEllipse (c - rr, c - rr, rr * 2.f, rr * 2.f, rf (0.4f, 1.1f) * px);
            }
            g.setColour (Colours::black.withAlpha (0.65f));
            g.drawEllipse (c - rc, c - rc, rc * 2.f, rc * 2.f, r * 0.035f);
            g.setColour (Colours::white.withAlpha (0.22f));
            g.drawEllipse (c - rc * 0.93f, c - rc * 0.93f, rc * 1.86f, rc * 1.86f, r * 0.015f);
            blob (g, { c, c }, rc * 0.2f, Colours::black.withAlpha (0.35f)); // pequeno fuxo central

            // Punteiro de pintura crema, con zonas descascadas
            {
                const int segs = 26;
                for (int pass = 0; pass < 2; ++pass)
                {
                    for (int j = 0; j < segs; ++j)
                    {
                        const float t0 = 0.20f + 0.68f * (float) j / (float) segs;
                        const float t1 = 0.20f + 0.68f * (float) (j + 1) / (float) segs;
                        const float chip = rnd.nextFloat() < 0.16f ? 0.08f : rf (0.6f, 1.f);
                        if (pass == 0)
                        {
                            g.setColour (Colours::black.withAlpha (0.45f));
                            g.drawLine (juce::Line<float> (polar (0.f, r * t0).translated (r * 0.02f, r * 0.025f),
                                                           polar (0.f, r * t1).translated (r * 0.02f, r * 0.025f)),
                                        r * 0.1f);
                        }
                        else
                        {
                            g.setColour (Colour (0xffe8dcb0).withAlpha (chip));
                            g.drawLine (juce::Line<float> (polar (0.f, r * t0), polar (0.f, r * t1)), r * rf (0.07f, 0.09f));
                        }
                    }
                }
            }

            // Rabuñaduras
            for (int i = 0; i < 48; ++i)
            {
                const auto p0 = inDisc (r * 0.97f);
                const float a = rf (0.f, kTwoPi);
                const float len = r * rf (0.06f, 0.5f);
                const juce::Point<float> p1 (p0.x + len * std::sin (a), p0.y + len * std::cos (a));
                const bool light = rnd.nextFloat() < 0.6f;
                g.setColour ((light ? Colours::white : Colours::black).withAlpha (light ? rf (0.08f, 0.38f) : rf (0.2f, 0.5f)));
                g.drawLine (juce::Line<float> (p0, p1), rf (0.4f, 1.2f) * px);
                if (rnd.nextFloat() < 0.25f) // borde oposto do rego
                {
                    g.setColour ((light ? Colours::black : Colours::white).withAlpha (0.18f));
                    g.drawLine (juce::Line<float> (p0.translated (0.8f * px, 0.8f * px), p1.translated (0.8f * px, 0.8f * px)), 0.5f * px);
                }
            }
            // Arañazos circulares na tapa
            for (int i = 0; i < 9; ++i)
            {
                const float rad = rc * rf (0.15f, 0.95f);
                const float from = rf (0.f, kTwoPi);
                juce::Path arc;
                arc.addCentredArc (c, c, rad, rad, 0.f, from, from + rf (0.3f, 1.4f), true);
                g.setColour (Colours::white.withAlpha (rf (0.08f, 0.28f)));
                g.strokePath (arc, juce::PathStrokeType (rf (0.4f, 0.9f) * px));
            }

            // Sucidade e mantas de graxa
            for (int i = 0; i < 24; ++i)
                blob (g, inDisc (r), r * rf (0.08f, 0.28f),
                      (rnd.nextFloat() < 0.7f ? Colours::black : Colour (0xff4b3a1c)).withAlpha (rf (0.08f, 0.24f)));
            // Pegadas máis claras
            for (int i = 0; i < 3; ++i)
                blob (g, inDisc (r * 0.7f), r * rf (0.2f, 0.35f), Colours::white.withAlpha (0.06f));
            // Ferruxe
            for (int i = 0; i < 8; ++i)
                blob (g, polar (rf (0.f, kTwoPi), r * rf (0.68f, 0.97f)), r * rf (0.03f, 0.085f),
                      Colour (0xff8a3f1c).withAlpha (rf (0.25f, 0.5f)));
            // Pequenas picaduras
            for (int i = 0; i < 100; ++i)
            {
                const auto p = inDisc (r * 0.96f);
                const float d = rf (0.4f, 1.2f) * px;
                g.setColour (Colours::black.withAlpha (rf (0.3f, 0.65f)));
                g.fillEllipse (p.x - d, p.y - d, d * 2.f, d * 2.f);
            }
            // Lascas na aresta da tapa
            for (int i = 0; i < 9; ++i)
            {
                const auto p = polar (rf (0.f, kTwoPi), rc * rf (0.9f, 1.0f));
                const float d = rf (0.8f, 1.8f) * px;
                g.setColour (Colour (0xffd4d4d6).withAlpha (0.55f));
                g.fillEllipse (p.x - d, p.y - d, d * 2.f, d * 1.4f);
            }
        }

        // Gran fino sobre todos os píxeles
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::readWrite);
        for (int y = 0; y < S; ++y)
            for (int x = 0; x < S; ++x)
            {
                auto p = bd.getPixelColour (x, y);
                if (p.getAlpha() == 0)
                    continue;
                const float d = (rnd.nextFloat() - 0.5f) * 0.075f;
                bd.setPixelColour (x, y, Colour::fromFloatRGBA (juce::jlimit (0.f, 1.f, p.getFloatRed() + d),
                                                                juce::jlimit (0.f, 1.f, p.getFloatGreen() + d),
                                                                juce::jlimit (0.f, 1.f, p.getFloatBlue() + d),
                                                                p.getFloatAlpha()));
            }
        return img;
    }

    // Graos da agulla do VU (positivo = dereita). A deflexión é proporcional á tensión,
    // así que 0 VU cae ao 71 % do arco e o repouso queda á esquerda do -20.
    float vuToAngleDeg (float vu)
    {
        vu = juce::jlimit (-40.f, 3.f, vu);
        return -40.f + 80.f * std::pow (10.f, (vu - 3.f) / 20.f);
    }
}

//==============================================================================
const juce::Image& KnobLookAndFeel::textureFor (const juce::Slider& s)
{
    const auto id = s.getComponentID();
    auto it = textures.find (id);
    if (it == textures.end())
        it = textures.emplace (id, makeKnobTexture (192, id.hashCode64() + 12345)).first;
    return it->second;
}

void KnobLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float startAngle, float endAngle, juce::Slider& slider)
{
    namespace Colours = juce::Colours;
    using juce::ColourGradient;

    const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
    const auto c = b.getCentre();
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f / kPadding;
    const float angle = startAngle + sliderPos * (endAngle - startAngle);
    const auto& tex = textureFor (slider);

    // Sombra sobre o panel
    g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.7f), c.x + r * 0.08f, c.y + r * 0.12f,
                                       Colours::transparentBlack, c.x + r * 0.08f, c.y + r * 1.27f, true));
    g.fillEllipse (c.x - r * 1.27f, c.y - r * 1.22f, r * 2.54f, r * 2.54f);

    // Textura xirada
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    const float sc = (2.f * r) / (float) tex.getWidth();
    g.drawImageTransformed (tex, juce::AffineTransform::translation (-0.5f * (float) tex.getWidth(),
                                                                     -0.5f * (float) tex.getHeight())
                                     .scaled (sc).rotated (angle).translated (c.x, c.y));

    // Luz fixa por riba (non xira co knob)
    juce::ColourGradient ao (Colours::transparentBlack, c.x, c.y, Colours::black.withAlpha (0.5f), c.x, c.y + r, true);
    ao.addColour (0.7, Colours::transparentBlack);
    g.setGradientFill (ao);
    g.fillEllipse (c.x - r, c.y - r, r * 2.f, r * 2.f);

    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.16f), c.x - r * 0.55f, c.y - r * 0.6f,
                                       Colours::transparentWhite, c.x + r * 0.1f, c.y + r * 0.1f, false));
    g.fillEllipse (c.x - r, c.y - r, r * 2.f, r * 2.f);

    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.34f), c.x - r * 0.26f, c.y - r * 0.3f,
                                       Colours::transparentWhite, c.x - r * 0.26f + r * 0.32f, c.y - r * 0.3f, true));
    g.fillEllipse (c.x - r * 0.6f, c.y - r * 0.62f, r * 0.7f, r * 0.55f);

    juce::Path rim;
    rim.addCentredArc (c.x, c.y, r * 0.97f, r * 0.97f, 0.f, -2.3f, -0.6f, true);
    g.setColour (Colours::white.withAlpha (0.26f));
    g.strokePath (rim, juce::PathStrokeType (r * 0.035f));
}

//==============================================================================
SelectorSwitch::SelectorSwitch (juce::RangedAudioParameter& param, int positions)
    : numPositions (positions),
      attachment (param, [this] (float v)
      {
          index = juce::jlimit (0, numPositions - 1, (int) std::round (v));
          repaint();
      })
{
    imgUp     = juce::ImageCache::getFromMemory (BinaryData::switch_up_png, BinaryData::switch_up_pngSize);
    imgCenter = juce::ImageCache::getFromMemory (BinaryData::switch_center_png, BinaryData::switch_center_pngSize);
    imgDown   = juce::ImageCache::getFromMemory (BinaryData::switch_down_png, BinaryData::switch_down_pngSize);
    attachment.sendInitialUpdate();
}

void SelectorSwitch::paint (juce::Graphics& g)
{
    int pos = 0; // -1 arriba, 0 centro, +1 abaixo
    if (numPositions == 2)
        pos = index == 1 ? -1 : 1;
    else
        pos = index - 1;

    const juce::Image& img = pos < 0 ? imgUp : (pos > 0 ? imgDown : imgCenter);
    if (! img.isValid())
        return;

    const float s = (float) getWidth() / kSpriteW;

    // Sombra suave da placa sobre o panel
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (juce::Rectangle<float> (38.f * s, 7.f * s, 125.f * s, 162.f * s)
                                .translated (2.f * s, 4.f * s).expanded (1.5f * s), 4.f * s);

    g.setOpacity (1.0f); // drawImage usa a opacidade da cor actual: sen isto o switch sae escuro e translúcido
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (img, 0, 0, getWidth(), getHeight(), 0, 0, img.getWidth(), img.getHeight());
}

bool SelectorSwitch::hitTest (int x, int y)
{
    const float s = (float) getWidth() / kSpriteW;
    return (float) x >= 38.f * s && (float) x <= 162.f * s
        && (float) y >= 7.f * s && (float) y <= 168.f * s;
}

void SelectorSwitch::setIndex (int newIndex)
{
    if (newIndex != index)
        attachment.setValueAsCompleteGesture ((float) newIndex);
}

void SelectorSwitch::stepVisual (int dir)
{
    if (numPositions == 2)
        setIndex (dir < 0 ? 1 : 0);
    else
        setIndex (juce::jlimit (0, numPositions - 1, index + dir));
}

void SelectorSwitch::mouseDown (const juce::MouseEvent& e)
{
    if (numPositions == 2)
        setIndex (1 - index);
    else
        stepVisual (e.y < getHeight() / 2 ? -1 : 1);
}

void SelectorSwitch::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    stepVisual (w.deltaY > 0 ? -1 : 1);
}

//==============================================================================
void HotspotButton::paintButton (juce::Graphics& g, bool isMouseOver, bool isButtonDown)
{
    const auto b = getLocalBounds().toFloat().reduced (2.f);
    if (isButtonDown)
    {
        g.setColour (juce::Colours::black.withAlpha (0.38f));
        g.fillRoundedRectangle (b, 8.f);
    }
    else if (isMouseOver)
    {
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.fillRoundedRectangle (b, 8.f);
    }
}

//==============================================================================
VinylEditor::VinylEditor (VinylProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    bg = juce::ImageCache::getFromMemory (BinaryData::background_jpg, BinaryData::background_jpgSize);

    // --- Knobs: id, centro (x, y) e radio, en píxeles da imaxe ---
    struct KD { const char* id; float x, y, r; };
    static const KD kd[] = {
        { "input", 218.4f, 496.f, 64.f }, { "warmth", 413.8f, 496.f, 64.f }, { "crackle", 608.0f, 496.f, 64.f },
        { "dust", 801.6f, 496.f, 64.f },  { "hiss", 994.9f, 496.f, 64.f },   { "wow", 1188.8f, 496.f, 64.f },
        { "flutter", 1384.0f, 496.f, 64.f },
        { "warp", 227.0f, 840.4f, 68.f }, { "age", 449.2f, 840.4f, 68.f },   { "tone", 671.3f, 840.4f, 68.f },
        { "width", 893.3f, 840.4f, 68.f }, { "mix", 1115.8f, 840.4f, 68.f }, { "output", 1342.6f, 840.4f, 68.f }
    };

    juce::Slider::RotaryParameters rot;
    rot.startAngleRadians = 7.0f * kPi / 6.0f;   // -150 graos desde as 12 en punto
    rot.endAngleRadians   = 17.0f * kPi / 6.0f;  // +150 graos
    rot.stopAtEnd = true;

    for (auto& d : kd)
    {
        auto k = std::make_unique<Knob>();
        k->centre = { d.x, d.y };
        k->radius = d.r;
        k->slider.setComponentID (d.id);
        k->slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        k->slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        k->slider.setRotaryParameters (rot);
        k->slider.setMouseDragSensitivity (220);
        k->slider.setPopupDisplayEnabled (true, true, this);
        k->slider.setLookAndFeel (&knobLnf);
        k->att = std::make_unique<SliderAtt> (proc.apvts, d.id, k->slider);
        if (auto* par = proc.apvts.getParameter (d.id))
            k->slider.setDoubleClickReturnValue (true, par->convertFrom0to1 (par->getDefaultValue()));
        addAndMakeVisible (k->slider);
        knobs.push_back (std::move (k));
    }

    // --- Selectores da columna dereita (de arriba abaixo) ---
    struct SD { const char* id; float x, y; int positions; };
    const SD sd[] = {
        { "speed",     1725.5f, 389.5f, 3 },
        { "surface",   1723.5f, 499.5f, 3 },
        { "mono_bass", 1724.5f, 612.5f, 2 },
        { "rumble",    1722.5f, 729.5f, 2 },
        { "power",     1720.5f, 839.5f, 2 },
        { "bypass",    1720.5f, 953.5f, 2 }
    };

    for (auto& d : sd)
    {
        SwitchItem it;
        it.centre = { d.x, d.y };
        it.sw = std::make_unique<SelectorSwitch> (*proc.apvts.getParameter (d.id), d.positions);
        addAndMakeVisible (*it.sw);
        switches.push_back (std::move (it));
    }

    // --- Botóns de presets (placas de abaixo) ---
    const char* names[4] = { "SAVE", "SAVE AS", "DELETE", "INIT" };
    const float bx[4] = { 942.5f, 1115.f, 1288.f, 1459.5f };
    for (int i = 0; i < 4; ++i)
    {
        buttons[(size_t) i] = std::make_unique<HotspotButton> (names[i]);
        buttonCentres[(size_t) i] = { bx[i], 1169.5f };
        addAndMakeVisible (*buttons[(size_t) i]);
    }

    buttons[0]->onClick = [this] { doSave(); };
    buttons[1]->onClick = [this] { promptSaveAs(); };
    buttons[2]->onClick = [this] { doDelete(); };
    buttons[3]->onClick = [this] { proc.initDefaults(); repaint(); };

    // Selector de presets: frechas e visor central (clic = lista)
    auto makeHot = [this] (std::unique_ptr<HotspotButton>& b, const char* name)
    {
        b = std::make_unique<HotspotButton> (name);
        addAndMakeVisible (*b);
    };
    makeHot (presetPrev, "PREV");
    makeHot (presetNext, "NEXT");
    makeHot (presetWin, "PRESET");
    presetPrev->onClick = [this] { stepPreset (-1); };
    presetNext->onClick = [this] { stepPreset (1); };
    presetWin->onClick  = [this] { showPresetMenu(); };

    sizeLimits.setSizeLimits (480, 320, 1920, 1280);
    sizeLimits.setFixedAspectRatio (1.5);
    setConstrainer (&sizeLimits);
    setResizable (true, true);
    setSize (960, 640);

    startTimerHz (30);
}

VinylEditor::~VinylEditor()
{
    stopTimer();
    setConstrainer (nullptr);
    for (auto& k : knobs)
        k->slider.setLookAndFeel (nullptr);
}

void VinylEditor::resized()
{
    const float s = scale();
    if (bg.isValid())
        bgScaled = bg.rescaled (getWidth(), getHeight(), juce::Graphics::highResamplingQuality);

    for (auto& k : knobs)
    {
        const float r = k->radius * s * KnobLookAndFeel::kPadding;
        k->slider.setBounds (juce::Rectangle<float> (r * 2.f, r * 2.f).withCentre (k->centre * s).toNearestInt());
    }

    for (auto& it : switches)
    {
        const float k = SelectorSwitch::kSpriteScale * s;
        const auto topLeft = it.centre * s - juce::Point<float> (SelectorSwitch::kPlateCx * k,
                                                                 SelectorSwitch::kPlateCy * k);
        it.sw->setBounds (juce::Rectangle<float> (topLeft.x, topLeft.y,
                                                  SelectorSwitch::kSpriteW * k,
                                                  SelectorSwitch::kSpriteH * k).toNearestInt());
    }

    for (size_t i = 0; i < buttons.size(); ++i)
        buttons[i]->setBounds (juce::Rectangle<float> (150.f * s, 72.f * s)
                                   .withCentre (buttonCentres[i] * s).toNearestInt());

    auto setHot = [s] (HotspotButton& b, const layout::HotRect& r)
    {
        b.setBounds (juce::Rectangle<float> (r.w * s, r.h * s)
                         .withCentre (juce::Point<float> (r.cx * s, r.cy * s)).toNearestInt());
    };
    setHot (*presetPrev, layout::kPresetPrev);
    setHot (*presetWin,  layout::kPresetWin);
    setHot (*presetNext, layout::kPresetNext);
}

void VinylEditor::paint (juce::Graphics& g)
{
    if (bgScaled.isValid())
        g.drawImageAt (bgScaled, 0, 0);
    else
        g.fillAll (juce::Colours::black);

    const float s = scale();
    g.addTransform (juce::AffineTransform::scale (s)); // desde aquí debuxamos en píxeles da imaxe

    // --- Agullas dos VU ---
    auto needle = [&] (const layout::VuMeter& m, float vu)
    {
        juce::Graphics::ScopedSaveState ss (g);
        // A agulla só se ve dentro da tarxeta; por baixo métese baixo o lábio da ventá
        g.reduceClipRegion (juce::Rectangle<float> (m.faceL, m.faceTop, m.faceR - m.faceL, m.faceBottom - m.faceTop).toNearestInt());
        const float a = vuToAngleDeg (vu) * kPi / 180.f;
        const juce::Point<float> pivot (m.pivotX, m.pivotY);
        const juce::Point<float> dir (std::sin (a), -std::cos (a));
        const juce::Point<float> nrm (std::cos (a), std::sin (a)); // perpendicular
        const juce::Point<float> tip = pivot + dir * m.needleLen;

        // Agulla afilada: ancha na base e fina na punta
        auto makeNeedle = [&] (juce::Point<float> off, float baseW, float tipW)
        {
            juce::Path p;
            p.startNewSubPath (pivot + off + nrm * (baseW * 0.5f));
            p.lineTo (tip + off + nrm * (tipW * 0.5f));
            p.lineTo (tip + off - nrm * (tipW * 0.5f));
            p.lineTo (pivot + off - nrm * (baseW * 0.5f));
            p.closeSubPath();
            return p;
        };
        g.setColour (juce::Colours::black.withAlpha (0.07f)); // sombra suave en dúas capas
        g.fillPath (makeNeedle ({ 3.5f, 5.f }, 4.6f, 2.2f));
        g.setColour (juce::Colours::black.withAlpha (0.14f));
        g.fillPath (makeNeedle ({ 1.8f, 2.8f }, 3.4f, 1.6f));
        g.setColour (juce::Colour (0xff15130f));
        g.fillPath (makeNeedle ({ 0.f, 0.f }, 3.0f, 1.0f));
    };
    needle (layout::kVuIn, vuIn);
    needle (layout::kVuOut, vuOut);

    // --- LEDs: a imaxe ten todos acesos; os apagados tápanse e os acesos reciben brillo ---
    auto led = [&] (float x, float y, juce::Colour col, bool lit, float lens, float halo)
    {
        if (lit)
        {
            g.setGradientFill (juce::ColourGradient (col.withAlpha (0.45f), x, y, col.withAlpha (0.f), x, y + halo, true));
            g.fillEllipse (x - halo, y - halo, halo * 2.f, halo * 2.f);
        }
        else
        {
            const auto dark = col.darker (0.95f);
            g.setGradientFill (juce::ColourGradient (dark.withAlpha (0.97f), x, y, dark.withAlpha (0.f), x, y + halo, true));
            g.fillEllipse (x - halo, y - halo, halo * 2.f, halo * 2.f);
            g.setColour (dark.brighter (0.15f));
            g.fillEllipse (x - lens, y - lens, lens * 2.f, lens * 2.f);
            g.setColour (juce::Colours::white.withAlpha (0.12f));
            g.fillEllipse (x - lens * 0.55f, y - lens * 0.7f, lens * 0.7f, lens * 0.5f);
        }
    };

    const bool powerOn  = proc.apvts.getRawParameterValue ("power")->load() > 0.5f;
    const bool bypassOn = proc.apvts.getRawParameterValue ("bypass")->load() > 0.5f;

    led (layout::kLedPowerX,   layout::kLedPowerY,   juce::Colour (0xff3cff2a), powerOn,  14.f, 32.f);
    led (layout::kLedBypassX,  layout::kLedBypassY,  juce::Colour (0xffff2a2a), bypassOn, 14.f, 32.f);
    led (layout::kLedClipX,    layout::kLedClipY,    juce::Colour (0xffff2a2a), clipHold > 0, 14.f, 32.f);
    led (layout::kLedCrackleX, layout::kLedCrackleY, juce::Colour (0xffffe82a), crackleHold > 0, 14.f, 32.f);
    led (layout::kActiveX, layout::kActiveY, juce::Colour (0xffff2a2a), powerOn && ! bypassOn, 9.f, 22.f);

    // --- Nome do preset no visor da barra de abaixo ---
    auto name = proc.getCurrentPresetName();
    if (name.isEmpty())
        name = "Init";
    g.setFont (juce::Font (juce::FontOptions (30.f)));
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawText (name, layout::presetText.translated (1.5f, 2.f).toNearestInt(), juce::Justification::centred, true);
    g.setColour (juce::Colour (0xffe9e1c4).withAlpha (0.93f));
    g.drawText (name, layout::presetText.toNearestInt(), juce::Justification::centred, true);

    // Versión (para comprobar que Reaper carga a compilación nova)
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.setFont (juce::Font (juce::FontOptions (20.f)));
    g.drawText (juce::String ("v") + JucePlugin_VersionString,
                juce::Rectangle<int> (110, 1236, 200, 28), juce::Justification::centredLeft);
}

void VinylEditor::timerCallback()
{
    const float pkIn = proc.inPeak.exchange (0.0f);
    proc.outPeak.store (0.0f);

    // Os VU xa chegan calibrados e con balística de VU; aquí só se suaviza o movemento
    auto follow = [] (float& cur, float target)
    {
        target = juce::jlimit (-40.f, 3.f, target);
        cur += (target - cur) * 0.45f;
    };
    follow (vuIn, proc.inVu.load());
    follow (vuOut, proc.outVu.load());

    if (pkIn >= 0.99f) clipHold = 20; else if (clipHold > 0) --clipHold;
    if (proc.crackleFlag.exchange (false)) crackleHold = 3; else if (crackleHold > 0) --crackleHold;

    const float s = scale();
    auto area = [s] (float x, float y, float w, float h)
    {
        return juce::Rectangle<float> (x * s, y * s, w * s, h * s).getSmallestIntegerContainer();
    };
    repaint (area (645.f, 55.f, 285.f, 175.f));
    repaint (area (1001.f, 55.f, 285.f, 175.f));
    repaint (area (1330.f, 95.f, 400.f, 80.f));
    repaint (area (1630.f, 995.f, 60.f, 60.f));
}

//==============================================================================
void VinylEditor::showPresetMenu()
{
    juce::PopupMenu m;
    const auto factory = VinylProcessor::getFactoryPresetNames();
    const auto user = proc.getUserPresetNames();
    const auto current = proc.getCurrentPresetName();

    m.addSectionHeader ("Factory");
    for (int i = 0; i < factory.size(); ++i)
        m.addItem (1000 + i, factory[i], true, current == factory[i]);

    m.addSectionHeader ("User");
    if (user.isEmpty())
        m.addItem (9999, "(empty)", false, false);
    for (int i = 0; i < user.size(); ++i)
        m.addItem (2000 + i, user[i], true, current == user[i]);

    juce::Component::SafePointer<VinylEditor> safe (this);
    const auto target = localAreaToGlobal (presetWin->getBounds());

    m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (target),
                     [safe, user] (int result)
    {
        if (safe == nullptr || result == 0)
            return;
        if (result >= 2000 && result < 9000)
            safe->proc.loadUserPreset (user[result - 2000]);
        else if (result >= 1000 && result < 2000)
            safe->proc.loadFactoryPreset (result - 1000);
        safe->repaint();
    });
}

void VinylEditor::stepPreset (int dir)
{
    const auto factory = VinylProcessor::getFactoryPresetNames();
    const auto user = proc.getUserPresetNames();
    const int nf = factory.size(), n = nf + user.size();
    if (n == 0)
        return;

    const auto current = proc.getCurrentPresetName();
    int idx = -1;
    for (int i = 0; i < n; ++i)
        if ((i < nf ? factory[i] : user[i - nf]) == current)
        {
            idx = i;
            break;
        }

    const int next = idx < 0 ? (dir > 0 ? 0 : n - 1) : (idx + dir + n) % n;
    if (next < nf)
        proc.loadFactoryPreset (next);
    else
        proc.loadUserPreset (user[next - nf]);
    repaint();
}

void VinylEditor::promptSaveAs()
{
    auto* w = new juce::AlertWindow ("Save preset as", "Preset name:", juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", proc.getCurrentPresetName());
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<VinylEditor> safe (this);
    juce::Component::SafePointer<juce::AlertWindow> sw (w);

    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, sw] (int result)
    {
        if (result == 1 && safe != nullptr && sw != nullptr)
        {
            safe->proc.saveUserPreset (sw->getTextEditorContents ("name"));
            safe->repaint();
        }
    }), true);
}

void VinylEditor::doSave()
{
    const auto current = proc.getCurrentPresetName();
    if (current.isNotEmpty() && proc.getUserPresetNames().contains (current))
    {
        proc.saveUserPreset (current);
        repaint();
    }
    else
    {
        promptSaveAs();
    }
}

void VinylEditor::doDelete()
{
    const auto current = proc.getCurrentPresetName();
    if (current.isEmpty() || ! proc.getUserPresetNames().contains (current))
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Delete preset",
                                                "Select one of your presets first (click the name plate).");
        return;
    }

    auto* w = new juce::AlertWindow ("Delete preset", "Delete \"" + current + "\"?", juce::MessageBoxIconType::QuestionIcon);
    w->addButton ("Delete", 1);
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<VinylEditor> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, current] (int result)
    {
        if (result == 1 && safe != nullptr)
        {
            safe->proc.deleteUserPreset (current);
            safe->repaint();
        }
    }), true);
}
