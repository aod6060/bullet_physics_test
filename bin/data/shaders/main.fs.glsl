/**
    main.fs.glsl

    This is the fragment shader for the bullet physics example.
*/

#version 400

uniform vec4 frag_Color;

out vec4 out_Color;

void main() {
    out_Color = frag_Color;
}