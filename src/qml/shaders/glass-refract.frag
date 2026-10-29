// Superellipse distance field and refraction adapted from OverShifted/LiquidGlass.
#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 surfaceSize;
    vec4 sampleRegion;
    vec4 surfaceColor;
    float materialOpacity;
    float backdropWeight;
    float cornerRadius;
    vec4 refractionCurve;
    float refractionPower;
};
layout(binding = 1) uniform sampler2D sourceTexture;

float superellipseDistance(vec2 p) {
    vec2 square = p * p;
    vec2 cube = square * p;
    return (dot(square, square) - 1.0) / (4.0 * length(cube) + 0.00001);
}

float panelDistance(vec2 p, vec2 halfSize, float radius) {
    vec2 corner = abs(p) - halfSize + radius;
    return min(corner.x, corner.y) > 0.0
        ? superellipseDistance(corner / radius) * radius
        : max(corner.x, corner.y) - radius;
}

void main() {
    vec2 p = (qt_TexCoord0 - 0.5) * surfaceSize;
    vec2 halfSize = max(surfaceSize * 0.5 - 0.5, vec2(1.0));
    float radius = min(cornerRadius, min(halfSize.x, halfSize.y));
    float d = panelDistance(p, halfSize, radius);
    float aa = max(fwidth(d), 0.6);
    float coverage = 1.0 - smoothstep(-aa, aa, d);
    float depth = max(-d, 0.0) / min(halfSize.x, halfSize.y);
    float lens = 1.0 - refractionCurve.y * pow(refractionCurve.z * 2.718281828459045,
        -refractionCurve.w * depth - refractionCurve.x);
    vec2 uv = sampleRegion.xy + sampleRegion.zw
        * ((qt_TexCoord0 - 0.5) * pow(clamp(lens, 0.0, 1.0), refractionPower) + 0.5);
    vec3 backdrop = texture(sourceTexture, clamp(uv, vec2(0.001), vec2(0.999))).rgb;
    vec3 color = mix(surfaceColor.rgb, backdrop, backdropWeight);
    float fallbackAlpha = max(materialOpacity, smoothstep(1.0, 7.0, -d) * 0.90);
    float alpha = mix(fallbackAlpha, 1.0, backdropWeight) * coverage;
    fragColor = vec4(clamp(color, 0.0, 1.0) * alpha, alpha) * qt_Opacity;
}
