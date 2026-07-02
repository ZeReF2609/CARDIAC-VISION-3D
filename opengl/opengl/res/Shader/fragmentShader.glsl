#version 330 core

out vec4 FragColor;

in vec3 FragPos;
in vec2 TexCoords;
in mat3 TBN;

uniform vec3 lightPos;
uniform vec3 viewPos;

uniform sampler2D baseTexture;
uniform sampler2D normalTexture;

void main()
{
    vec3 texColor = texture(baseTexture, TexCoords).rgb;
    if (texColor.r > 0.65 && texColor.b > 0.35)
    {
    texColor.r *= 1.08;
    texColor.g *= 0.95;
    texColor.b *= 0.92;
    }
    if (texColor.b > 0.55 && texColor.r < 0.45)
    {
    texColor = mix(texColor, vec3(0.20, 0.65, 1.0), 0.35);
    }

    vec3 normal = texture(normalTexture, TexCoords).rgb;
    normal = normal * 2.0 - 1.0;
    normal = normalize(TBN * normal);

    vec3 lightDir = normalize(lightPos - FragPos);

    // Ambient
    vec3 ambient = 0.50 * vec3(1.0);

    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * vec3(1.0);

    // Specular (húmedo)
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, normal);

    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 48);
    vec3 specular = 0.65 * spec * vec3(1.0);
    float fresnel = pow(1.0 - max(dot(viewDir, normal), 0.0), 2.0);
    vec3 softGlow = vec3(0.18, 0.03, 0.04) * fresnel;


    vec3 result = (ambient + diffuse + specular) * texColor + softGlow;


    FragColor = vec4(result, 1.0);
}