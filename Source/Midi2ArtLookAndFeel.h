/*
 ==============================================================================

   Midi2ArtLookAndFeel.h
   Custom LookAndFeel for MIDI2Art plugin (knobs & basic controls)

 ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

// Small, header-only LookAndFeel to avoid touching project files.
class Midi2ArtLookAndFeel : public juce::LookAndFeel_V4
{
public:
    Midi2ArtLookAndFeel()
    {
        // Global base colours (can still be overridden per component)
        const auto accentBlue  = juce::Colour (0xff1fa0ff);   // dunkleres Cyan
        const auto knobOutline = juce::Colour (0xff151a28);   // sehr dunkles Blau-Grau

        setColour (juce::Slider::rotarySliderFillColourId, accentBlue);
        setColour (juce::Slider::rotarySliderOutlineColourId, knobOutline);
        setColour (juce::TextButton::buttonColourId, juce::Colour (0x80333b55));
        setColour (juce::TextButton::buttonOnColourId, accentBlue.withAlpha (0.8f));
        setColour (juce::TextButton::textColourOffId, juce::Colours::white);
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::TextEditor::textColourId, juce::Colours::white);
        setColour (juce::TextEditor::highlightColourId, accentBlue.withAlpha (0.4f));
        setColour (juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::textColourId, juce::Colours::white);
        setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
        setColour (juce::ComboBox::arrowColourId, accentBlue);
    }

    ~Midi2ArtLookAndFeel() override = default;

private:
    // Helper: Cut-corner Path (45° "abgebissen") - wird von Buttons und TextEditor verwendet
    static juce::Path makeCutCornerPath (juce::Rectangle<float> r, float cut)
    {
        juce::Path p;
        auto x  = r.getX();
        auto y  = r.getY();
        auto x2 = r.getRight();
        auto y2 = r.getBottom();

        cut = juce::jmin (cut, r.getWidth() * 0.25f, r.getHeight() * 0.25f);

        p.startNewSubPath (x + cut, y);
        p.lineTo (x2 - cut, y);
        p.lineTo (x2,      y + cut);
        p.lineTo (x2,      y2 - cut);
        p.lineTo (x2 - cut, y2);
        p.lineTo (x + cut,  y2);
        p.lineTo (x,        y2 - cut);
        p.lineTo (x,        y + cut);
        p.closeSubPath();
        return p;
    }

public:
    void drawRotarySlider (juce::Graphics& g,
                           int x, int y, int width, int height,
                           float sliderPosProportional,
                           float rotaryStartAngle,
                           float rotaryEndAngle,
                           juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (6.0f);

        auto radius   = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        auto centre   = bounds.getCentre();
        auto rx       = centre.x - radius;
        auto ry       = centre.y - radius;
        auto rw       = radius * 2.0f;
        auto knobArea = juce::Rectangle<float> (rx, ry, rw, rw);

        auto outlineColour = slider.findColour (juce::Slider::rotarySliderOutlineColourId);
        auto fillColour    = slider.findColour (juce::Slider::rotarySliderFillColourId);

        // --- Outer ring (dunkel, nicht gefüllt) ---
        float outerThickness = juce::jmax (1.5f, radius * 0.10f);
        g.setColour (outlineColour.withAlpha (0.85f));
        g.drawEllipse (knobArea.reduced (outerThickness * 0.5f), outerThickness);

        // --- Soft "Glow"-Ring mit Transparenz nach außen ---
        {
            juce::Path glowRing;
            glowRing.addEllipse (knobArea.expanded (outerThickness * 0.6f));

            juce::Colour innerC = fillColour.withAlpha (0.22f);
            juce::Colour outerC = fillColour.withAlpha (0.0f);

            juce::ColourGradient glowGrad (innerC,
                                           centre.x, centre.y,
                                           outerC,
                                           centre.x, centre.y + radius * 1.4f,
                                           true);
            g.setGradientFill (glowGrad);
            g.fillPath (glowRing);
        }

        // --- Value arc (progress) ---
        auto toAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        juce::Path valueArc;
        float arcThickness = juce::jmax (2.0f, radius * 0.11f);
        valueArc.addCentredArc (centre.x, centre.y,
                                radius - arcThickness * 0.5f,
                                radius - arcThickness * 0.5f,
                                0.0f,
                                rotaryStartAngle,
                                toAngle,
                                true);

        g.setColour (fillColour.withAlpha (0.9f));
        g.strokePath (valueArc, juce::PathStrokeType (arcThickness,
                                                      juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));

        // --- Pointer ---
        auto pointerLength    = radius * 0.55f;
        auto pointerThickness = juce::jmax (1.5f, radius * 0.06f);

        juce::Path pointer;
        pointer.addRoundedRectangle (-pointerThickness * 0.5f,
                                     -pointerLength,
                                     pointerThickness,
                                     pointerLength,
                                     pointerThickness * 0.5f);

        g.setColour (fillColour.brighter (0.25f).withAlpha (0.85f));
        g.fillPath (pointer, juce::AffineTransform::rotation (toAngle).translated (centre.x, centre.y));
    }

    void drawButtonBackground (juce::Graphics& g,
                               juce::Button& button,
                               const juce::Colour& /*backgroundColour*/,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override
    {
        // Zeichnung fast über die ganze Button-Fläche, leicht nach unten verschoben
        // -> optisch gleich viel Abstand über/unter dem Text (Font-Baseline sitzt minimal tiefer)
        auto bounds = button.getLocalBounds().toFloat().reduced (1.0f, 1.0f);
        bounds.translate (0.0f, 1.0f);

        const bool isOn   = button.getToggleState();
        const bool isOver = shouldDrawButtonAsHighlighted;
        const bool isDown = shouldDrawButtonAsDown;

        // An Rotary-Knob orientiert
        auto outlineColour = findColour (juce::Slider::rotarySliderOutlineColourId);
        auto glowColour    = button.findColour (juce::TextButton::buttonOnColourId);

        float outlineThickness = juce::jmax (1.5f, bounds.getHeight() * 0.08f);
        float cutSize          = bounds.getHeight() * 0.35f;

        // Transparenter Grundkörper
        juce::Path basePath = makeCutCornerPath (bounds, cutSize);
        g.setColour (outlineColour.withAlpha (0.3f));
        g.fillPath (basePath);

        // Outline
        juce::Path outlinePath = makeCutCornerPath (bounds.reduced (outlineThickness * 0.5f), cutSize);
        g.setColour (outlineColour.withAlpha (0.9f));
        g.strokePath (outlinePath, juce::PathStrokeType (outlineThickness));

        // Glow-Intensität nach Zustand
        float glowAlpha = 0.0f;
        if (isOn)
            glowAlpha = 0.35f;
        else if (isOver || isDown)
            glowAlpha = 0.18f;

        if (glowAlpha > 0.0f)
        {
            // Glow in derselben Cut-Corner-Form wie der Button selbst
            auto glowArea = bounds.expanded (1.5f, 1.5f);
            juce::Path glowPath = makeCutCornerPath (glowArea, cutSize);

            juce::Colour inner = glowColour.withAlpha (glowAlpha);
            juce::Colour outer = glowColour.withAlpha (0.0f);

            juce::ColourGradient grad (inner,
                                       glowArea.getCentreX(), glowArea.getCentreY(),
                                       outer,
                                       glowArea.getCentreX(), glowArea.getBottom(),
                                       false);
            g.setGradientFill (grad);
            g.fillPath (glowPath);
        }
    }

    void drawLinearSlider (juce::Graphics& g,
                           int x, int y, int width, int height,
                           float sliderPos,
                           float minSliderPos,
                           float maxSliderPos,
                           const juce::Slider::SliderStyle style,
                           juce::Slider& slider) override
    {
        auto trackBounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height)
                               .reduced (2.0f, (float) height * 0.35f);

        auto trackColour = slider.findColour (juce::Slider::trackColourId)
                               .withMultipliedAlpha (0.8f);
        auto thumbColour = slider.findColour (juce::Slider::thumbColourId)
                               .withMultipliedAlpha (0.95f);

        if (! trackColour.isOpaque())
            trackColour = slider.findColour (juce::Slider::rotarySliderOutlineColourId).withAlpha (0.9f);
        if (! thumbColour.isOpaque())
            thumbColour = slider.findColour (juce::Slider::rotarySliderFillColourId).brighter (0.05f);

        auto cornerRadius = trackBounds.getHeight() * 0.5f;

        // Track background (leicht vom Hintergrund abgehoben)
        g.setColour (trackColour.darker (0.4f));
        g.fillRoundedRectangle (trackBounds, cornerRadius);

        // Dünnes Outline um den gesamten Track
        g.setColour (trackColour.brighter (0.35f).withAlpha (0.9f));
        g.drawRoundedRectangle (trackBounds, cornerRadius, 1.0f);

        // Track fill (links bis SliderPos) mit leichtem Glow
        juce::Rectangle<float> filled = trackBounds;
        auto clampedPos = juce::jlimit (trackBounds.getX(),
                                        trackBounds.getRight(),
                                        sliderPos);
        filled.setRight (clampedPos);

        juce::ColourGradient grad (thumbColour.withAlpha (0.7f),
                                   filled.getX(), filled.getCentreY(),
                                   thumbColour.withAlpha (0.1f),
                                   filled.getRight(), filled.getCentreY(),
                                   false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (filled, cornerRadius);

        // Thumb (kleiner abgerundeter "Handle")
        const float thumbRadius = trackBounds.getHeight() * 0.9f;
        juce::Rectangle<float> thumbRect (0, 0, thumbRadius, thumbRadius);
        const float thumbHalf = thumbRadius * 0.5f;
        auto thumbCenterX = juce::jlimit (trackBounds.getX() + thumbHalf,
                                          trackBounds.getRight() - thumbHalf,
                                          sliderPos);
        thumbRect.setCentre (thumbCenterX, trackBounds.getCentreY());

        g.setColour (thumbColour.withAlpha (0.95f));
        g.fillRoundedRectangle (thumbRect, thumbRadius * 0.4f);
    }

    void fillTextEditorBackground (juce::Graphics& g,
                                    int width, int height,
                                    juce::TextEditor& textEditor) override
    {
        // Gleicher Stil wie Buttons: Cut-Corner-Form
        auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height)
                          .reduced (1.0f, 1.0f);

        auto outlineColour = findColour (juce::Slider::rotarySliderOutlineColourId);
        float cutSize = bounds.getHeight() * 0.35f;

        // Transparenter Grundkörper (wie bei Buttons)
        juce::Path basePath = makeCutCornerPath (bounds, cutSize);
        g.setColour (outlineColour.withAlpha (0.3f));
        g.fillPath (basePath);
    }

    void drawTextEditorOutline (juce::Graphics& g,
                                 int width, int height,
                                 juce::TextEditor& textEditor) override
    {
        // Gleicher Stil wie Buttons: Cut-Corner-Form
        auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height)
                          .reduced (1.0f, 1.0f);

        auto outlineColour = findColour (juce::Slider::rotarySliderOutlineColourId);
        auto fillColour    = findColour (juce::Slider::rotarySliderFillColourId);
        float outlineThickness = juce::jmax (1.5f, bounds.getHeight() * 0.08f);
        float cutSize          = bounds.getHeight() * 0.35f;

        // Outline (wie bei Buttons)
        juce::Path outlinePath = makeCutCornerPath (bounds.reduced (outlineThickness * 0.5f), cutSize);
        g.setColour (outlineColour.withAlpha (0.9f));
        g.strokePath (outlinePath, juce::PathStrokeType (outlineThickness));

        // Glow bei Focus (wie bei Buttons)
        if (textEditor.hasKeyboardFocus (true))
        {
            auto glowArea = bounds.expanded (1.5f, 1.5f);
            juce::Path glowPath = makeCutCornerPath (glowArea, cutSize);

            juce::Colour inner = fillColour.withAlpha (0.35f);
            juce::Colour outer = fillColour.withAlpha (0.0f);

            juce::ColourGradient grad (inner,
                                       glowArea.getCentreX(), glowArea.getCentreY(),
                                       outer,
                                       glowArea.getCentreX(), glowArea.getBottom(),
                                       false);
            g.setGradientFill (grad);
            g.fillPath (glowPath);
        }
    }

    void drawComboBox (juce::Graphics& g,
                        int width, int height,
                        bool isButtonDown,
                        int buttonX, int buttonY,
                        int buttonW, int buttonH,
                        juce::ComboBox& box) override
    {
        // Gleicher Stil wie TextEditor: Cut-Corner-Form
        auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height)
                          .reduced (1.0f, 1.0f);

        auto outlineColour = findColour (juce::Slider::rotarySliderOutlineColourId);
        float outlineThickness = juce::jmax (1.5f, bounds.getHeight() * 0.08f);
        float cutSize          = bounds.getHeight() * 0.35f;

        // Transparenter Grundkörper
        juce::Path basePath = makeCutCornerPath (bounds, cutSize);
        g.setColour (outlineColour.withAlpha (0.3f));
        g.fillPath (basePath);

        // Outline
        juce::Path outlinePath = makeCutCornerPath (bounds.reduced (outlineThickness * 0.5f), cutSize);
        g.setColour (outlineColour.withAlpha (0.9f));
        g.strokePath (outlinePath, juce::PathStrokeType (outlineThickness));

        // Leichter Glow bei Hover/Focus
        if (box.hasKeyboardFocus (true) || isButtonDown)
        {
            auto fillColour = findColour (juce::Slider::rotarySliderFillColourId);
            auto glowArea   = bounds.expanded (1.5f, 1.5f);
            juce::Path glowPath = makeCutCornerPath (glowArea, cutSize);

            juce::Colour inner = fillColour.withAlpha (0.25f);
            juce::Colour outer = fillColour.withAlpha (0.0f);

            juce::ColourGradient grad (inner,
                                       glowArea.getCentreX(), glowArea.getCentreY(),
                                       outer,
                                       glowArea.getCentreX(), glowArea.getBottom(),
                                       false);
            g.setGradientFill (grad);
            g.fillPath (glowPath);
        }
    }

    void positionComboBoxText (juce::ComboBox& box,
                                 juce::Label& label) override
    {
        // Standard-Positionierung, aber mit etwas mehr Padding links
        label.setBounds (8, 0, box.getWidth() - 30, box.getHeight());
    }

    void drawPopupMenuBackground (juce::Graphics& g, int width, int height) override
    {
        // Dunkler Hintergrund für Dropdown-Menü
        auto outlineColour = findColour (juce::Slider::rotarySliderOutlineColourId);
        g.setColour (outlineColour.darker (0.8f).withAlpha (0.95f));
        g.fillRoundedRectangle (0.0f, 0.0f, (float) width, (float) height, 4.0f);
        
        // Dünnes Outline
        g.setColour (outlineColour.brighter (0.35f).withAlpha (0.9f));
        g.drawRoundedRectangle (0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f, 4.0f, 1.0f);
    }

    void drawPopupMenuItem (juce::Graphics& g,
                             const juce::Rectangle<int>& area,
                             bool isSeparator,
                             bool isActive,
                             bool isHighlighted,
                             bool isTicked,
                             bool hasSubMenu,
                             const juce::String& text,
                             const juce::String& shortcutKeyText,
                             const juce::Drawable* icon,
                             const juce::Colour* textColour) override
    {
        auto textCol = textColour != nullptr ? *textColour : findColour (juce::ComboBox::textColourId);
        
        if (isSeparator)
        {
            auto r = area.reduced (5, 0);
            g.setColour (textCol.withAlpha (0.3f));
            g.fillRect (r.removeFromTop (1));
        }
        else
        {
            auto r = area.reduced (1);
            
            // Highlight bei Hover
            if (isHighlighted)
            {
                auto fillColour = findColour (juce::Slider::rotarySliderFillColourId);
                g.setColour (fillColour.withAlpha (0.3f));
                g.fillRoundedRectangle (r.toFloat(), 2.0f);
            }
            
            // Text
            g.setColour (isActive ? textCol : textCol.withAlpha (0.7f));
            g.setFont (juce::Font ((float) r.getHeight() * 0.6f));
            g.drawText (text, r.reduced (8, 0), juce::Justification::centredLeft, true);
            
            // Sleaker: Nur Highlight für aktives Item, kein Checkmark
            if (isTicked)
            {
                auto fillColour = findColour (juce::Slider::rotarySliderFillColourId);
                // Dünner Akzent-Strich links
                g.setColour (fillColour.withAlpha (0.8f));
                g.fillRect (r.removeFromLeft (3));
            }
        }
    }
};


