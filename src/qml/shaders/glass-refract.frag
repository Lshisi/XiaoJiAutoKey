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
    float darkAmount;
    float backdropWeight;
    float cornerRadius;
    vec2 lightPoint;
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

vec3 toLinear(vec3 color) {
    return mix(color / 12.92, pow((color + 0.055) / 1.055, vec3(2.4)),
        step(vec3(0.04045), color));
}

vec3 toSrgb(vec3 color) {
    return mix(color * 12.92, 1.055 * pow(max(color, vec3(0.0)), vec3(1.0 / 2.4)) - 0.055,
        step(vec3(0.0031308), color));
}

void main() {
    vec2 p = (qt_TexCoord0 - 0.5) * surfaceSize;
    vec2 halfSize = max(surfaceSize * 0.5 - 0.5, vec2(1.0));
    float radius = min(cornerRadius, min(halfSize.x, halfSize.y));
    float d = panelDistance(p, halfSize, radius);
    float aa = max(fwidth(d), 0.6);
    float coverage = 1.0 - smoothstep(-aa, aa, d);
    float depth = max(-d, 0.0) / min(halfSize.x, halfSize.y);
    float lens = 1.0 - 2.3 * pow(5.2 * 2.718281828, -6.9 * depth - 0.7);
    vec2 uv = sampleRegion.xy + sampleRegion.zw
        * ((qt_TexCoord0 - 0.5) * pow(lens, 3.0) + 0.5);
    vec3 backdrop = texture(sourceTexture, clamp(uv, vec2(0.001), vec2(0.999))).rgb;
    float core = smoothstep(1.0, 7.0, -d);
    vec3 linearColor = toLinear(mix(backdrop, surfaceColor.rgb, materialOpacity * 0.14));
    float luminance = dot(linearColor, vec3(0.2126, 0.7152, 0.0722));
    vec3 lightMaterial = mix(linearColor, vec3(1.0),
        clamp((0.48 - luminance) / max(1.0 - luminance, 0.0001), 0.0, 1.0));
    vec3 darkMaterial = linearColor * min(1.0, 0.10 / max(luminance, 0.0001));
    vec3 readable = mix(lightMaterial, darkMaterial, darkAmount);
    vec3 tint = mix(surfaceColor.rgb, toSrgb(mix(linearColor, readable, core)), backdropWeight);

    vec2 gradient = vec2(
        panelDistance(p + vec2(0.5, 0.0), halfSize, radius)
            - panelDistance(p - vec2(0.5, 0.0), halfSize, radius),
        panelDistance(p + vec2(0.0, 0.5), halfSize, radius)
            - panelDistance(p - vec2(0.0, 0.5), halfSize, radius));
    vec2 normal = gradient / max(length(gradient), 0.0001);
    vec2 lightDirection = normalize(vec2(-0.6, -0.8) + (lightPoint - 0.5) * 0.35);
    float light = pow(max(dot(normal, lightDirection), 0.0), 3.0);
    float rim = exp(-abs(d + 0.7) * 1.7);
    float bevel = exp(-max(-d, 0.0) / 3.5);
    tint += bevel * (light * 0.16 - (1.0 - light) * mix(0.08, 0.04, darkAmount));
    float highlight = rim * (0.16 + light * 0.42);
    tint = mix(tint, vec3(1.0), highlight);
    float alpha = mix(max(materialOpacity, core * 0.90), 1.0, backdropWeight) * coverage;
    fragColor = vec4(clamp(tint, 0.0, 1.0) * alpha, alpha) * qt_Opacity;
}
