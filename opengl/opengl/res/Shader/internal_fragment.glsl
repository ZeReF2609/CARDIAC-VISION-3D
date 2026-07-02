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

    //------------------------------------
    // MUSCLE COLOR CORRECTION
    //------------------------------------
    float muscleMask = texColor.r - texColor.b;
    muscleMask = clamp(muscleMask, 0.0, 1.0);
    texColor *= mix(vec3(1.0), vec3(0.95, 0.84, 0.84), muscleMask * 0.28);

    //------------------------------------
    // BLUE VESSEL DETECTION
    // Venas (azul puro) y arterias (rojo oscuro)
    //------------------------------------
    float blueMask    = clamp((texColor.b - texColor.r) * 2.5, 0.0, 1.0);
    float blueProtect = blueMask;

    // Arterias coronarias: rojo oscuro saturado (r alto, g y b bajos)
    float arteryMask  = clamp((texColor.r - texColor.g * 1.8)
                      * (1.0 - texColor.b * 2.0), 0.0, 1.0);

    //------------------------------------
    // NORMAL MAP
    //------------------------------------
    vec3 normalRaw = texture(normalTexture, TexCoords).rgb;
    normalRaw = normalRaw * 2.0 - 1.0;

    // Intensidad base — vasos necesitan más relieve
    float vesselMask   = clamp(blueMask + arteryMask, 0.0, 1.0);
    normalRaw.xy      *= mix(0.55, 1.10, vesselMask); // vasos: normal más fuerte

    float wallMask = smoothstep(0.60, 0.88, texColor.r);
    normalRaw.xy  *= mix(1.0, 0.12, wallMask * (1.0 - vesselMask));

    vec3 normal = normalize(TBN * normalRaw);

    vec3 lightDir   = normalize(lightPos - FragPos);
    vec3 viewDir    = normalize(viewPos  - FragPos);
    vec3 reflectDir = reflect(-lightDir, normal);
    float nDotV     = max(dot(normal, viewDir), 0.0);

    // -------- WET FRESNEL (suave) --------
    float wetFresnel = pow(1.0 - nDotV, 4.0);
    vec3 wetEdge = vec3(0.05, 0.012, 0.012) * wetFresnel * 0.18;

    float diff      = max(dot(normal, lightDir), 0.0);

    //------------------------------------
    // MICRO-AO
    //------------------------------------
    float curvature = clamp(1.0 - normalRaw.b, 0.0, 1.0);
    float microAO   = pow(curvature, 1.6);

    //------------------------------------
    // AO / CAVITY
    // Vasos tubulares tienen AO fuerte en su base (donde tocan el músculo)
    //------------------------------------
    float cavity    = pow(1.0 - nDotV, 2.8);
    float ao        = 1.0 - cavity * 0.52;
    ao             *= mix(1.0, 0.58, microAO * 0.65);

    // AO extra en base de vasos — sombra de contacto con el miocardio
    float vesselBaseAO = vesselMask
                       * pow(clamp(1.0 - nDotV, 0.0, 1.0), 2.0)
                       * clamp(1.0 - diff * 1.2, 0.0, 1.0);
    ao *= mix(1.0, 0.72, vesselBaseAO * 0.45);

    //------------------------------------
    // HEMI AMBIENT
    //------------------------------------
    float hemi        = normal.y * 0.5 + 0.5;
    vec3  skyColor    = vec3(0.88, 0.86, 0.86);
    vec3  groundColor = vec3(0.42, 0.28, 0.26);
    vec3  hemiLight   = mix(groundColor, skyColor, hemi);
    vec3 ambient = texColor * hemiLight * mix(0.38, 0.58, vesselMask) * mix(0.45, 1.0, ao);

    //------------------------------------
    // MAIN DIFFUSE
    //------------------------------------
    vec3 diffuse = diff * texColor * 0.84;

    //------------------------------------
    // VESSEL SELF-SHADOW
    // Vasos cilíndricos: lado opuesto a la luz muy oscuro
    // Simula oclusión tubular real
    //------------------------------------
    float vesselShadowFactor = pow(1.0 - diff, 2.5) * vesselMask;

    // Venas: sombra azul-oscura
    vec3 veinSelfShad   = texColor * vec3(0.02, 0.02, 0.08)
                        * vesselShadowFactor * blueMask * 4.5;

    // Arterias: sombra rojo-oscura
    vec3 arterySelfShad = texColor * vec3(0.12, 0.01, 0.01)
                        * vesselShadowFactor * arteryMask * 4.0;

    //------------------------------------
    // VESSEL HIGHLIGHT
    // Vasos cilíndricos tienen highlight especular elongado en su cúspide
    //------------------------------------
    float vesselSpec    = pow(max(dot(viewDir, reflectDir), 0.0), 28.0)
                        * vesselMask * diff;

    // Venas: highlight azulado
    vec3 veinHighlight = vec3(0.12, 0.16, 0.32) * vesselSpec * blueMask * 3.0;

    // Arterias: highlight rojo brillante
    vec3 arteryHighlight = vec3(0.42, 0.08, 0.08) * vesselSpec * arteryMask * 3.8;

    //------------------------------------
    // VESSEL CONTACT SHADOW
    // Sombra que proyecta el vaso sobre el miocardio circundante
    //------------------------------------
    float contactShadow = pow(curvature, 1.4)
                        * (1.0 - diff * 0.5)
                        * (1.0 - vesselMask)   // solo en músculo, no en el vaso
                        * (microAO * 0.9);
    vec3 contactDark = texColor * vec3(0.08, 0.01, 0.01) * contactShadow * 1.2;

    //------------------------------------
    // DEEP VEIN DARKENING (surcos miocárdicos)
    //------------------------------------
    float veinDepthShadow = pow(curvature, 1.8) * (1.0 - diff * 0.4)
                          * (1.0 - wallMask) * (1.0 - vesselMask);
    vec3 deepVein = texColor * vec3(0.14, 0.018, 0.018) * veinDepthShadow * 3.2;

    //------------------------------------
    // CAVITY DARKENING
    //------------------------------------
    float concavityMask = pow(clamp(1.0 - nDotV, 0.0, 1.0), 1.8)
                        * (1.0 - wallMask)
                        * clamp(1.0 - diff, 0.0, 1.0);
    vec3 cavityDark = texColor * vec3(0.18, 0.03, 0.03) * concavityMask * 1.8;

    //------------------------------------
    // CHORDAE SHADOW (interior)
    //------------------------------------
    float chordaeMask = wallMask
                      * pow(1.0 - diff, 2.2)
                      * clamp(1.0 - nDotV * 1.4, 0.0, 1.0);
    vec3 chordaeShadow  = vec3(0.10, 0.015, 0.010) * chordaeMask * 2.8;
    float chordaeBaseAO = pow(wallMask, 1.5)
                        * pow(clamp(1.0 - diff, 0.0, 1.0), 1.5)
                        * microAO;
    vec3 chordaeBaseOccl= vec3(0.08, 0.01, 0.01) * chordaeBaseAO * 3.5;

    //------------------------------------
    // BACK LIGHT
    //------------------------------------
    vec3  backLightDir = normalize(vec3(-0.5, 0.35, -1.0));
    float backDiff     = pow(max(dot(normal, -backLightDir), 0.0), 1.4);
    float backAtten    = mix(0.90, 0.28, wallMask);
    // Vasos no reciben backlight igual — son más opacos
    backAtten *= mix(1.0, 1.35, vesselMask);
    vec3 backLight     = texColor * backDiff * backAtten;

    //------------------------------------
    // BOUNCE LIGHT — protegido en azules
    //------------------------------------
    float bounce      = pow(max(dot(-normal, lightDir), 0.0), 2.0);
    vec3  bounceColor = mix(vec3(0.42, 0.09, 0.08), vec3(0.05, 0.05, 0.18), blueProtect);
    vec3  bounceLight = bounceColor * bounce * mix(1.0, 0.55, blueProtect);

    //------------------------------------
    // SPECULAR principal
    //------------------------------------
    float spec      = pow(max(dot(viewDir, reflectDir), 0.0), 40.0);
    float roughNoise= clamp(normalRaw.r * 0.5 + normalRaw.g * 0.5, 0.0, 1.0);
    spec           *= mix(0.7, 1.0, roughNoise);

    float whiteness = (texColor.r + texColor.g + texColor.b) / 3.0;
    float specStr;
    if (whiteness > 0.72)
        specStr = 0.0005;
    else if (texColor.b > texColor.r)
        specStr = 0.08;
    else
        specStr = 0.04;
    vec3 specular = vec3(specStr) * spec;

    //------------------------------------
    // WET HIGHLIGHT
    //------------------------------------
    float wet     = pow(max(dot(viewDir, reflectDir), 0.0), 80.0);
    float wetStr  = mix(0.008, 0.0008, wallMask);
    vec3  wetSpec = vec3(wetStr) * wet;

    // -------- FAKE BLOOM --------
    float bloomMask = smoothstep(0.72, 0.96, wet + spec);
    vec3 bloomGlow = vec3(0.18, 0.04, 0.04) * bloomMask * 0.22;

    //------------------------------------
    // RIM LIGHT
    //------------------------------------
    float rim      = pow(1.0 - nDotV, 2.0);
    vec3  rimColor = mix(vec3(0.12, 0.08, 0.08), vec3(0.05, 0.08, 0.22), blueProtect);
    vec3 rimLight = rimColor * rim * 1.35;

    //------------------------------------
    // VEIN SHADOW (surcos normales)
    //------------------------------------
    float veinMask = clamp(1.0 - normalRaw.b, 0.0, 1.0);
    float veinDepth= pow(veinMask, 2.0);
    vec3  veinOccl = vec3(-0.022, -0.004, -0.004) * veinDepth;
    vec3  veinShad = vec3(-0.048, -0.010, -0.010) * veinMask;

    //------------------------------------
    // VALVE SHADOW
    //------------------------------------
    float valveMask   = pow(1.0 - diff, 3.0);
    vec3  valveShadow = vec3(-0.08, -0.02, -0.02) * valveMask;

    //------------------------------------
    // SUBSURFACE — desactivado en azules
    //------------------------------------
    float sss        = pow(1.0 - nDotV, 2.2);
    vec3  subsurface = vec3(0.20, 0.05, 0.04) * sss * (1.0 - blueProtect);
    float backScatter = pow(max(dot(-lightDir, viewDir), 0.0), 2.0);
    vec3  backSSS     = vec3(0.28, 0.04, 0.04) * backScatter * (1.0 - blueProtect);

    //------------------------------------
    // FIBROUS TISSUE TINT
    //------------------------------------
    float fibrousMask = clamp(wallMask * (1.0 - whiteness * 0.5), 0.0, 1.0);
    vec3  fibrousTint = vec3(0.96, 0.82, 0.74);
    texColor = mix(texColor, texColor * fibrousTint, fibrousMask * 0.45);

    //------------------------------------
    // FINAL COMPOSITE
    //------------------------------------
    vec3 result =
    (
        ambient        +
        diffuse        +
        backLight      +
        bounceLight    +
        specular       +
        wetSpec        +
        bloomGlow      +
        rimLight       +
        veinShad       +
        veinOccl       +
        valveShadow    +
        subsurface     +
        backSSS        +
        veinHighlight  +
        arteryHighlight +
        wetEdge
    ) * ao;

    // Sustracciones post-AO
    result -= deepVein;
    result -= cavityDark;
    result -= chordaeShadow;
    result -= chordaeBaseOccl;
    result -= veinSelfShad;
    result -= arterySelfShad;
    result -= contactDark;

    result = max(result, texColor * 0.14);

    //------------------------------------
    // GAMMA
    //------------------------------------
    result = pow(result, vec3(1.01));

    //------------------------------------
    // WHITE TISSUE CORRECTION
    //------------------------------------
    float whiteTissue = step(0.82, texColor.r) * step(0.72, texColor.g);
    if (whiteTissue > 0.5)
    {
        vec3 pinkTint = vec3(1.00, 0.88, 0.90);
        result = mix(result, result * pinkTint, 0.35);
        result *= 0.88;
    }

    //------------------------------------
    // CONTRAST FINAL S-CURVE
    //------------------------------------
    float luma  = dot(result, vec3(0.299, 0.587, 0.114));
    float curve = luma * luma * (3.0 - 2.0 * luma);
    result      = mix(result * 0.92, result * 1.06, curve);

    float rearBoost = pow(1.0 - diff, 1.8) * 0.18;
    result += texColor * rearBoost;

    float vesselRearGlow = vesselMask * pow(1.0 - diff, 1.5) * 0.25;
    result += texColor * vesselRearGlow;

    // soft highlight compression
    result = min(result, vec3(1.15));
    result *= 1.035;


    
    FragColor = vec4(clamp(result, 0.0, 1.0), 1.0);
}