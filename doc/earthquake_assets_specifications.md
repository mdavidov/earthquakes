# Earthquake Alert System - Asset Specifications

## Required Image Files

### Application Icons

#### earthquake_app.png (Main Application Icon)

- **Size**: 256x256px (with smaller variants: 16x16, 32x32, 48x48, 64x64, 128x128)
- **Format**: PNG with transparency
- **Description**: Circular seismograph design with concentric rings representing seismic waves
- **Colors**: Deep blue background (#1e3a5f), bright orange/red waves (#ff6b35)
- **Style**: Modern, flat design with subtle depth

#### earthquake_large.png (Splash Screen Icon)

- **Size**: 128x128px
- **Format**: PNG with transparency
- **Description**: Larger version of main icon for splash screen
- **Additional Elements**: Subtle glow effect around waves

#### earthquake_alert.png (System Tray Icon)

- **Size**: 16x16px, 22x22px, 32x32px
- **Format**: PNG with transparency
- **Description**: Simplified version of main icon, optimized for small sizes
- **Style**: High contrast for visibility in system tray

### Splash Screen Background

#### splash_background.png

- **Size**: 600x400px
- **Format**: PNG
- **Description**: Dark gradient background (navy to dark blue) with subtle earthquake wave patterns
- **Elements**: 
  - Gradient from #2c3e50 (top) to #1a252f (bottom)
  - Faint concentric circles representing seismic waves
  - World map silhouette in very dark blue (#0f1419)

### UI Icons (24x24px each)

#### icons/refresh.png

- Circular arrow icon in white/light gray

#### icons/settings.png

- Gear/cog icon in white/light gray

#### icons/fullscreen.png

- Expand/maximize icon in white/light gray

#### icons/zoom_in.png

- Plus sign with magnifying glass

#### icons/zoom_out.png

- Minus sign with magnifying glass

#### icons/legend.png

- List/legend icon with colored squares

#### icons/filter.png

- Funnel icon for filtering

#### icons/export.png

- Download/save arrow icon

#### icons/alert_on.png

- Bell icon (solid) for enabled alerts

#### icons/alert_off.png

- Bell icon with slash for disabled alerts

### Magnitude Icons (Color-coded circles, 32x32px each)

- **magnitude/mag_minor.png** - Green circle (#4CAF50)
- **magnitude/mag_light.png** - Yellow circle (#FFEB3B)
- **magnitude/mag_moderate.png** - Orange circle (#FF9800)
- **magnitude/mag_strong.png** - Red circle (#F44336)
- **magnitude/mag_major.png** - Dark red circle (#D32F2F)
- **magnitude/mag_great.png** - Purple circle (#9C27B0)

## Required Sound Files

### Alert Sounds (WAV format, 16-bit, 44.1kHz)

#### alert.wav

- **Duration**: 2-3 seconds
- **Type**: Standard alert tone
- **Description**: Clear, attention-getting beep sequence (3 ascending tones)
- **Volume**: Moderate, not jarring
- **Pattern**: Beep-beep-beep with slight pause between

#### warning.wav

- **Duration**: 3-4 seconds
- **Type**: More urgent warning sound
- **Description**: Rapid alternating tones (high-low-high-low pattern)
- **Volume**: Louder than alert.wav
- **Pattern**: Urgent but not panic-inducing

#### emergency.wav

- **Duration**: 4-5 seconds
- **Type**: Critical emergency alert
- **Description**: Continuous warbling siren tone
- **Volume**: Highest volume level
- **Pattern**: Continuous rising/falling tone to grab immediate attention

#### beep.wav

- **Duration**: 0.5 seconds
- **Type**: Simple notification
- **Description**: Single clear beep tone
- **Volume**: Soft, for minor notifications

#### chime.wav

- **Duration**: 1-2 seconds
- **Type**: Pleasant notification
- **Description**: Gentle bell-like chime (similar to system notification)
- **Volume**: Soft to moderate

## Asset Creation Guidelines

### Image Creation Tools

- **Professional**: Adobe Illustrator, Photoshop
- **Free alternatives**: GIMP, Inkscape, Canva
- **Icon-specific**: IconJar, Icon8, Flaticon

### Sound Creation/Sources

- **Creation tools**: Audacity (free), Adobe Audition
- **Free sound libraries**: 
  - Freesound.org
  - Zapsplat.com
  - BBC Sound Effects Library
- **Generated tones**: Audacity's tone generator for simple beeps

### Design Principles

#### Icons

- Use consistent color palette across all icons
- Maintain high contrast for visibility
- Follow platform conventions (Windows, macOS, Linux)
- Include scalable vector versions (SVG) when possible

#### Sounds

- Keep file sizes small (under 100KB each)
- Use uncompressed WAV for best quality
- Test at different volume levels
- Ensure sounds are distinguishable from each other

## File Organization Structure

```text
resources/
├── icons/
│   ├── earthquake_app.png (and variants)
│   ├── earthquake_large.png
│   ├── earthquake_alert.png
│   ├── splash_background.png
│   ├── ui/
│   │   ├── refresh.png
│   │   ├── settings.png
│   │   └── [other UI icons]
│   └── magnitude/
│       ├── mag_minor.png
│       └── [other magnitude icons]
├── sounds/
│   ├── alert.wav
│   ├── warning.wav
│   ├── emergency.wav
│   ├── beep.wav
│   └── chime.wav
└── splash/
    └── splash_background.png
```

## Qt Resource File (resources.qrc)

```xml
<RCC>
    <qresource prefix="/icons">
        <file>icons/earthquake_app.png</file>
        <file>icons/earthquake_large.png</file>
        <file>icons/earthquake_alert.png</file>
        <file>icons/ui/refresh.png</file>
        <file>icons/ui/settings.png</file>
        <!-- Add all other icon files -->
    </qresource>
    <qresource prefix="/sounds">
        <file>sounds/alert.wav</file>
        <file>sounds/warning.wav</file>
        <file>sounds/emergency.wav</file>
        <file>sounds/beep.wav</file>
        <file>sounds/chime.wav</file>
    </qresource>
    <qresource prefix="/splash">
        <file>splash/splash_background.png</file>
    </qresource>
</RCC>
```

## Implementation Notes

### Loading Assets in Code

```cpp
// Icons
QIcon appIcon(":/icons/earthquake_app.png");
QPixmap splashImage(":/splash/splash_background.png");

// Sounds
QSoundEffect alertSound;
alertSound.setSource(QUrl("qrc:/sounds/alert.wav"));
```

### Alternative Asset Sources

If you prefer not to create custom assets:

1. **Icons**: Use system icons or icon libraries like Font Awesome
1. **Sounds**: Use system notification sounds as fallbacks
1. **Images**: Generate programmatically using QPainter for simple designs

### Accessibility Considerations

- Provide visual alternatives for sound alerts
- Ensure sufficient contrast in all icons
- Include tooltips describing icon functions
- Allow users to customize alert sounds and volumes

## Budget-Friendly Alternatives

### Free Icon Sets

- Feather Icons
- Material Design Icons
- Font Awesome (free tier)

### Free Sound Resources

- System notification sounds
- Generated tones using audio software
- Creative Commons sound libraries

This specification provides everything needed to create a professional-looking and functional asset set for your Earthquake Alert System.
