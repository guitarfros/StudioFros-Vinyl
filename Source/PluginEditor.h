#pragma once
#include "PluginProcessor.h"
#include <array>
#include <map>
#include <memory>
#include <vector>

//==============================================================================
// Knobs gastados: a textura xérase por código (rabuñaduras, sucidade, ferruxe)
class KnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override;

    static constexpr float kPadding = 1.28f; // marxe arredor do knob para a sombra

private:
    const juce::Image& textureFor (const juce::Slider&);
    std::map<juce::String, juce::Image> textures;
};

//==============================================================================
// Interruptor de palanca feito con fotos (sprites con transparencia)
//  2 posicións: arriba = ON (índice 1), abaixo = OFF (índice 0)
//  3 posicións: arriba = índice 0, centro = índice 1, abaixo = índice 2
class SelectorSwitch : public juce::Component
{
public:
    SelectorSwitch (juce::RangedAudioParameter& param, int numPositions);

    // Medidas das imaxes orixinais (204 x 176 px) e como se colocan no panel
    static constexpr float kSpriteW = 204.f, kSpriteH = 176.f;
    static constexpr float kPlateCx = 100.f, kPlateCy = 87.5f; // centro da placa dentro da imaxe
    static constexpr float kSpriteScale = 0.62f;               // tamaño no panel

    void paint (juce::Graphics&) override;
    bool hitTest (int x, int y) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void stepVisual (int dir); // -1 = arriba, +1 = abaixo
    void setIndex (int newIndex);

    int numPositions = 2;
    int index = 0;
    juce::Image imgUp, imgCenter, imgDown;
    juce::ParameterAttachment attachment;
};

//==============================================================================
// Botón transparente sobre as placas SAVE / SAVE AS / DELETE / INIT
class HotspotButton : public juce::Button
{
public:
    explicit HotspotButton (const juce::String& name) : juce::Button (name) {}
    void paintButton (juce::Graphics&, bool isMouseOver, bool isButtonDown) override;
};

//==============================================================================
class VinylEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VinylEditor (VinylProcessor&);
    ~VinylEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void showPresetMenu();
    void stepPreset (int dir); // -1 = anterior, +1 = seguinte
    void promptSaveAs();
    void doSave();
    void doDelete();

    float scale() const { return (float) getWidth() / 1920.0f; }

    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;

    struct Knob
    {
        juce::Slider slider;
        std::unique_ptr<SliderAtt> att;
        juce::Point<float> centre;
        float radius = 60.f;
    };

    struct SwitchItem
    {
        std::unique_ptr<SelectorSwitch> sw;
        juce::Point<float> centre;
    };

    VinylProcessor& proc;
    KnobLookAndFeel knobLnf;
    juce::ComponentBoundsConstrainer sizeLimits;

    juce::Image bg, bgScaled;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<SwitchItem> switches;
    std::array<std::unique_ptr<HotspotButton>, 4> buttons; // SAVE, SAVE AS, DELETE, INIT
    std::array<juce::Point<float>, 4> buttonCentres;
    std::unique_ptr<HotspotButton> presetPrev, presetNext, presetWin; // frechas e visor de presets

    float vuIn = -20.f, vuOut = -20.f;
    int clipHold = 0, crackleHold = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VinylEditor)
};
