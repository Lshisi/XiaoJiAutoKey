// Separable blur adapted from OverShifted/LiquidGlass.
#version 440

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 sampleStep;
};
layout(binding = 1) uniform sampler2D sourceTexture;

vec4 sampleAt(vec2 uv) {
    return texture(sourceTexture, clamp(uv, vec2(0.001), vec2(0.999)));
}

void main() {
    vec2 uv = qt_TexCoord0;
    vec4 color = sampleAt(uv) * 0.1964825501511404;
    color += (sampleAt(uv + sampleStep * 1.411764705882353)
        + sampleAt(uv - sampleStep * 1.411764705882353)) * 0.2969069646728344;
    color += (sampleAt(uv + sampleStep * 3.2941176470588234)
        + sampleAt(uv - sampleStep * 3.2941176470588234)) * 0.09447039785044732;
    color += (sampleAt(uv + sampleStep * 5.176470588235294)
        + sampleAt(uv - sampleStep * 5.176470588235294)) * 0.010381362401148057;
    fragColor = color * qt_Opacity;
}
