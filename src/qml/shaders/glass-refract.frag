// Refraction model adapted from OverShifted/LiquidGlass, MIT.
// Copyright (c) 2026 Sepehr Kalanaki. See third_party/liquid-glass/LICENSE.
#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 surfaceSize;
    vec4 surfaceColor;
    float darkAmount;
    float cornerRadius;
};
layout(binding = 1) uniform sampler2D sourceTexture;
float sdSuperellipse(vec2 p) {
    vec2 square = p * p;
    float numerator = dot(square, square) - 1.0;
    vec2 cube = square * p;
    return numerator / (4.0 * length(cube) + 0.00001);
}
float panelDistance(vec2 p, vec2 halfSize, float radius) {
    vec2 corner = abs(p) - halfSize + radius;
    return min(corner.x, corner.y) > 0.0 ? sdSuperellipse(corner / radius) * radius : max(corner.x, corner.y) - radius;
}
void main() {
    vec2 p = (qt_TexCoord0 - 0.5) * surfaceSize;
    vec2 halfSize = max(surfaceSize * 0.5 - 8.0, vec2(1.0));
    float radius = min(cornerRadius, min(halfSize.x, halfSize.y));
    float d = panelDistance(p, halfSize, radius);
    float aa = max(fwidth(d), 0.55);
    float coverage = 1.0 - smoothstep(-aa, aa, d);
    float depth = max(-d, 0.0) / radius;
    float lens = 1.0 - 0.16 * pow(5.2 * 2.718281828, -6.9 * depth - 0.7);
    vec2 uv = (qt_TexCoord0 - 0.5) * pow(lens, 1.6) + 0.5;
    vec3 backdrop = texture(sourceTexture, clamp(uv, vec2(0.001), vec2(0.999))).rgb;
    vec3 tint = mix(backdrop, surfaceColor.rgb, mix(0.87, 0.83, darkAmount));
    vec2 gradient = vec2(panelDistance(p + vec2(0.5, 0), halfSize, radius) - panelDistance(p - vec2(0.5, 0), halfSize, radius),
        panelDistance(p + vec2(0, 0.5), halfSize, radius) - panelDistance(p - vec2(0, 0.5), halfSize, radius));
    vec2 normal = gradient / max(length(gradient), 0.0001);
    float light = pow(max(dot(normal, normalize(vec2(-0.6, -0.8))), 0.0), 3.0);
    float rim = exp(-abs(d + 0.7) * 1.7) * (0.12 + light * 0.36);
    tint = mix(tint, vec3(1.0), rim);
    float shadowDistance = max(panelDistance(p - vec2(0, 2), halfSize, radius), 0.0);
    float shadow = exp(-shadowDistance / 2.2) * 0.13 * (1.0 - coverage);
    fragColor = vec4(tint * coverage, coverage + shadow * (1.0 - coverage)) * qt_Opacity;
}
