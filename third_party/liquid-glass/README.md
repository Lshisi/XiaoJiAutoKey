# LiquidGlass attribution

Source: https://github.com/OverShifted/LiquidGlass

Copyright (c) 2026 Sepehr Kalanaki. MIT licensed; see LICENSE.

The Qt 6 shaders in src/qml/shaders/glass-blur.frag and glass-refract.frag adapt
the separable blur, superellipse distance field and refraction model from
assets/shaders/Blur.glsl and assets/shaders/BatchRenderer2D.glsl.
This project uses Qt Shader Tools / QML, not the upstream OverEngine demo.
Noise and decorative glow are intentionally omitted. Background input is an
in-memory snapshot of this application's own Qt Quick view, never a desktop
or game capture. The MIT notice is embedded in the executable's resources.
