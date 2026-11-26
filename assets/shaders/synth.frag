in vec2 v_uv;
in vec4 v_color;
in float v_mode;

uniform sampler2D u_font_tex;

out vec4 frag_color;

void main() {
    if (v_mode > 1.5) {
        /* Mode 2: SDF rounded card / circle knob face (v_uv in [-1, 1]) */
        float d = length(max(abs(v_uv) - vec2(0.72), 0.0)) - 0.28;
        float aa = 1.0 - smoothstep(-0.04, 0.04, d);
        frag_color = vec4(v_color.rgb, v_color.a * aa);
    } else if (v_mode > 0.5) {
        /* Mode 1: Single-channel UTF-8 font glyph atlas */
        float a = texture(u_font_tex, v_uv).r;
        frag_color = vec4(v_color.rgb, v_color.a * a);
    } else {
        /* Mode 0: Solid / anti-aliased vector quad, arc segment, cable, or scope trace */
        frag_color = v_color;
    }
}
