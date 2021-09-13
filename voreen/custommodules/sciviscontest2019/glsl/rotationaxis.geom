#version 330

layout(points) in;
layout(line_strip, max_vertices=2) out;

in vec4  geom_position[1];
in float geom_vel[1];
in vec3  geom_angular_momenta[1];
in float geom_radius[1];

out vec2 frag_coord;
out float frag_vel;

uniform mat4 projectionMatrix;

void main() {
    float extend = 1.2f;

    float r = geom_radius[0];
    vec4 momenta = vec4(geom_angular_momenta[0],0);
    vec4 axis = momenta*r*extend;

    vec4 projectedPos1 = projectionMatrix*(geom_position[0]+axis);
    projectedPos1/=projectedPos1.w;

    vec4 projectedPos2 = projectionMatrix*(geom_position[0]-axis);
    projectedPos2/=projectedPos2.w;

    if(projectedPos1.z <= 1.0f && projectedPos1.z >= -1.0f && projectedPos2.z <= 1.0f && projectedPos2.z >= -1.0f) {

        float minz = min(projectedPos1.z, projectedPos2.z);
        projectedPos1.z=minz;
        projectedPos2.z=minz;

        frag_coord  = vec2(0.5, 1.0);
        frag_vel    = geom_vel[0];
        gl_Position = projectedPos1;
        EmitVertex();

        frag_coord  = vec2(-0.5, 1.0);
        frag_vel    = geom_vel[0];
        gl_Position = projectedPos2;
        EmitVertex();

        EndPrimitive();
    }
}