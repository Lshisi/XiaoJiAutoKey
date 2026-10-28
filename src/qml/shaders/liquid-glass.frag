#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 surfaceSize;
    float darkAmount;
    float materialOpacity;
    float cornerRadius;
    float shadowInset;
    vec2 lightPoint;
    vec4 surfaceColor;
};

float roundedBox(vec2 point, vec2 halfSize, float radius) {
    vec2 delta = abs(point) - halfSize + vec2(radius);
    return length(max(delta, vec2(0.0))) + min(max(delta.x, delta.y), 0.0) - radius;
}

void main() {
    vec2 point = (qt_TexCoord0 - 0.5) * surfaceSize;
    vec2 halfSize = max(surfaceSize * 0.5 - vec2(shadowInset), vec2(1.0));
    float radius = min(cornerRadius, min(halfSize.x, halfSize.y));
    float distance = roundedBox(point, halfSize, radius);
    float edgeAA = max(fwidth(distance), 0.65);
    float coverage = 1.0 - smoothstep(-edgeAA, edgeAA, distance);

    // A beveled surface and rim lighting; no desktop image is sampled.
    vec2 inner = abs(point) - halfSize + vec2(radius);
    vec2 normal = normalize(max(inner, vec2(0.0)) + vec2(0.001)) * sign(point);
    if (max(inner.x, inner.y) < 0.0)
        normal = inner.x > inner.y ? vec2(sign(point.x), 0.0) : vec2(0.0, sign(point.y));
    vec2 light = normalize(vec2(-0.55, -0.8) + (lightPoint - 0.5) * vec2(0.35, 0.25));
    float facing = dot(normal, light);
    float bevel = exp(-max(-distance, 0.0) / 3.8);
    float rim = exp(-abs(distance + 0.65) * 1.7);
    float highlight = pow(max(facing, 0.0), 3.0);
    float reflected = pow(max(-facing, 0.0), 5.0) * 0.3;

    vec3 base = surfaceColor.rgb;
    vec3 tint = base + vec3(0.025) * (0.5 - qt_TexCoord0.y);
    tint += vec3(1.0) * bevel * (highlight * 0.18 + reflected * 0.10);
    tint -= vec3(0.06) * bevel * max(-facing, 0.0);
    float rimStrength = rim * (mix(0.14, 0.10, darkAmount) + highlight * 0.55 + reflected * 0.25);
    tint = mix(tint, vec3(1.0), rimStrength);

    float surfaceAlpha = clamp(materialOpacity + rimStrength * 0.25, 0.0, 1.0) * coverage;
    float shadowDistance = max(roundedBox(point - vec2(0.0, 2.0), halfSize, radius), 0.0);
    float shadow = exp(-shadowDistance / 2.2) * 0.13 * (1.0 - coverage);
    vec4 surface = vec4(clamp(tint, 0.0, 1.0) * surfaceAlpha, surfaceAlpha);
    fragColor = (surface + vec4(0.0, 0.0, 0.0, shadow) * (1.0 - surfaceAlpha)) * qt_Opacity;
}
