#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;
layout (location = 5) in ivec4 aBoneIDs;
layout (location = 6) in vec4 aWeights;

out vec3 FragPos;
out vec2 TexCoords;
out mat3 TBN;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float heartbeat;

const int MAX_BONES = 100;
uniform mat4 finalBonesMatrices[MAX_BONES];

void main()
{
    // Latido más orgánico:
    // contracción rápida, relajación suave
    float organicBeat = pow(heartbeat, 0.65);

    mat4 boneTransform = mat4(1.0);

    // Solo usar bones cuando realmente existan
    if (aBoneIDs[0] != -1)
    {
        boneTransform = mat4(0.0);

        boneTransform += finalBonesMatrices[aBoneIDs[0]] * aWeights[0];
        boneTransform += finalBonesMatrices[aBoneIDs[1]] * aWeights[1];
        boneTransform += finalBonesMatrices[aBoneIDs[2]] * aWeights[2];
        boneTransform += finalBonesMatrices[aBoneIDs[3]] * aWeights[3];
    }

    vec4 localPosition = boneTransform * vec4(aPos, 1.0);
    vec3 animatedPos = localPosition.xyz;
    float apexCore = 1.0 - smoothstep(1.5, 5.0, aPos.y);

    // NUEVO MASK MUCHO MÁS AMPLIO (DEBUG)
    float valveLeafletMask =
    smoothstep(4.0, 10.0, aPos.y) *
    (1.0 - smoothstep(12.5, 15.5, aPos.y));


    //---------------------------------
    // 1. VENTRICLES CONTRACTION
    //---------------------------------
    float lowerMask = 1.0 - smoothstep(4.0, 7.0, aPos.y);
    float midMask   = 1.0 - smoothstep(7.0, 10.0, aPos.y);

    float ventricleMask = lowerMask * 1.0 + midMask * 0.45;

    float delayedBeat = pow(heartbeat, 1.3);
    float filling = pow(1.0 - heartbeat, 2.0);

    // expansión en diástole
    animatedPos.x *= (1.0 + filling * 0.018 * ventricleMask);
    animatedPos.z *= (1.0 + filling * 0.022 * ventricleMask);

    // contracción en sístole
    animatedPos.x *= (1.0 - delayedBeat * 0.040 * ventricleMask);
    animatedPos.z *= (1.0 - delayedBeat * 0.048 * ventricleMask);
    animatedPos.y += delayedBeat * 0.11 * ventricleMask;

    animatedPos.y += delayedBeat * 0.060 * apexCore;
    animatedPos.x *= (1.0 - delayedBeat * 0.018 * apexCore);
    animatedPos.z *= (1.0 - delayedBeat * 0.018 * apexCore);

    float valveBeat = pow(heartbeat, 1.8);

    animatedPos.y -= valveBeat * 0.045 * valveLeafletMask;
    animatedPos.z *= (1.0 - valveBeat * 0.025 * valveLeafletMask);
    
    //---------------------------------
    // 2. VALVE ZONE PULSE
    //---------------------------------
    float valveMaskY =
    smoothstep(7.0, 8.8, aPos.y) *
    (1.0 - smoothstep(10.8, 12.2, aPos.y));

    float centerExclusion =
    1.0 - (1.0 - smoothstep(0.65, 1.55, abs(aPos.x)));

    float valveMask = valveMaskY * centerExclusion;

    float valveOpen = sin(organicBeat * 3.14159);

    float valveSide = sign(aPos.x);

    // cerrar hacia centro
    animatedPos.x -= valveSide * valveOpen * 0.085 * valveMask;

    // bajar ligeramente
    animatedPos.y -= abs(valveOpen) * 0.035 * valveMask;

    // curvatura hacia adentro
    animatedPos.z *= (1.0 - valveOpen * 0.035 * valveMask);
    
    //---------------------------------
    // 3. AORTA / ARTERIES PULSE
    //---------------------------------
    float arteryMask = smoothstep(11.0, 14.0, aPos.y);

    float arteryBeat = pow(max(heartbeat - 0.22, 0.0), 0.7);

    animatedPos.x *= (1.0 + arteryBeat * 0.030 * arteryMask);
    animatedPos.z *= (1.0 + arteryBeat * 0.034 * arteryMask);
    animatedPos.y += arteryBeat * 0.020 * arteryMask;

    //---------------------------------
    // 4. HELICAL TWIST (MUCHO MÁS REAL)
    //---------------------------------
    float apexMask = 1.0 - smoothstep(3.5, 7.5, aPos.y);
    float baseMask = smoothstep(9.0, 13.0, aPos.y);

    float apexTwist = delayedBeat * 0.095 * apexMask;
    float baseTwist = delayedBeat * -0.050 * baseMask;

    float twist = apexTwist + baseTwist;

    float s = sin(twist);
    float c = cos(twist);

    vec2 rotatedXZ;
    rotatedXZ.x = animatedPos.x * c - animatedPos.z * s;
    rotatedXZ.y = animatedPos.x * s + animatedPos.z * c;

    animatedPos.x = rotatedXZ.x;
    animatedPos.z = rotatedXZ.y;

    //---------------------------------
    // STANDARD PIPELINE
    //---------------------------------
    FragPos = vec3(model * vec4(animatedPos, 1.0));
    TexCoords = aTexCoords;

    vec3 T = normalize(mat3(model) * aTangent);
    vec3 B = normalize(mat3(model) * aBitangent);
    vec3 N = normalize(mat3(model) * aNormal);

    TBN = mat3(T, B, N);

    gl_Position = projection * view * vec4(FragPos, 1.0);
}