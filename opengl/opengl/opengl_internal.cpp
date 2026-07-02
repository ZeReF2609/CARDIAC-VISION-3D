#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <cmath>
#include "src/Shader.h"
#include "src/ModelInternal.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

namespace {
ImVec2 WorldToScreen(
    const glm::vec3& pos,
    const glm::mat4& model,
    const glm::mat4& view,
    const glm::mat4& projection,
    int width,
    int height)
{
    glm::vec4 clip =
        projection * view * model * glm::vec4(pos, 1.0f);

    if (clip.w <= 0.0f)
        return ImVec2(-9999, -9999);

    glm::vec3 ndc = glm::vec3(clip) / clip.w;

    float x = (ndc.x + 1.0f) * 0.5f * width;
    float y = (1.0f - ndc.y) * 0.5f * height;

    return ImVec2(x, y);
}

// ─── Resolución ──────────────────────────────────────────────────
int windowWidth = 0;
int windowHeight = 0;

// ─── Cámara libre (FPS) ──────────────────────────────────────────
float cameraYaw = 180.0f;
float cameraPitch = 10.0f;
float targetYaw = 180.0f;
float targetPitch = 10.0f;
float moveSpeed = 0.10f;
float targetMoveSpeed = 0.10f;
glm::vec3 camPos = glm::vec3(0.0f, -0.10f, 0.0f);

// ─── Mouse ────────────────────────────────────────────────────────
float lastX = 0.0f;
float lastY = 0.0f;

bool dragging = false;
bool firstMouse = true;
int selectedPart = 0;
bool hideUI = false;
bool goBackToExternal = false;

struct Marker
{
    int id;
    glm::vec3 pos;
};

Marker markers[] =
{
    {1, glm::vec3(0.2f, 13.0f, 0.0f)},   // Aorta
    {2, glm::vec3(0.5f, 11.3f, 0.5f)},   // Pulmonar
    {3, glm::vec3(1.0f, 7.0f, -1.2f)},   // Mitral
    {4, glm::vec3(-1.2f, 6.2f, 0.0f)},   // Tricuspide
    {5, glm::vec3(0.0f, 10.0f, 0.0f)},   // Aortica
    {6, glm::vec3(1.8f, 8.7f, 0.0f)},    // Auricula Izq
    {7, glm::vec3(-2.0f, 8.0f, 0.0f)},   // Auricula Der
    {8, glm::vec3(2.7f, 4.2f, 0.4f)},    // Ventriculo Izq
    {9, glm::vec3(-1.0f, 3.0f, 0.0f)}    // Ventriculo Der
};

// ─── Cursores ─────────────────────────────────────────────────────
GLFWcursor* cursorHand = nullptr;
GLFWcursor* cursorHandGrab = nullptr;

// ─── Inercia ──────────────────────────────────────────────────────
float velocityYaw = 0.0f;
float velocityPitch = 0.0f;
float prevTargetYaw = 90.0f;
float prevTargetPitch = 8.0f;

// ─── Pan (botón medio) ────────────────────────────────────────────
bool  panning = false;
float panX = 0.0f;
float panY = 0.0f;
float targetPanX = 0.0f;
float targetPanY = 0.0f;
float lastPanMouseX = 0.0f;
float lastPanMouseY = 0.0f;
bool  firstPan = true;

// ─────────────────────────────────────────────────────────────────
// Genera cursor de manita en código — sin PNG externo
// ─────────────────────────────────────────────────────────────────
GLFWcursor* makeHandCursor(bool closed)
{
    const int W = 32, H = 32;
    unsigned char pixels[W * H * 4];
    memset(pixels, 0, sizeof(pixels));  // todo transparente

    auto setPixel = [&](int x, int y,
        unsigned char r, unsigned char g,
        unsigned char b, unsigned char a)
        {
            if (x < 0 || x >= W || y < 0 || y >= H) return;
            int i = (y * W + x) * 4;
            pixels[i + 0] = r;
            pixels[i + 1] = g;
            pixels[i + 2] = b;
            pixels[i + 3] = a;
        };

    // Color piel + contorno oscuro
    auto skin = [&](int x, int y) { setPixel(x, y, 238, 195, 154, 255); };
    auto dark = [&](int x, int y) { setPixel(x, y, 80, 50, 20, 200); };

    auto fillRect = [&](int x0, int y0, int x1, int y1, bool outline = true)
        {
            for (int y = y0; y <= y1; y++)
                for (int x = x0; x <= x1; x++)
                {
                    bool edge = (x == x0 || x == x1 || y == y0 || y == y1);
                    if (outline && edge) dark(x, y);
                    else                 skin(x, y);
                }
        };

    if (!closed)
    {
        // ── Mano abierta ──────────────────────────────────────────
        // Dedo índice
        fillRect(6, 1, 9, 13);
        // Dedo medio
        fillRect(10, 0, 13, 13);
        // Dedo anular
        fillRect(14, 1, 17, 13);
        // Dedo meñique
        fillRect(18, 4, 21, 13);
        // Pulgar
        fillRect(1, 9, 6, 15);
        // Palma
        fillRect(4, 13, 22, 23);
        // Muñeca
        fillRect(6, 23, 20, 27);
    }
    else
    {
        // ── Mano cerrada (puño) ───────────────────────────────────
        // Nudillos
        fillRect(4, 7, 21, 13);
        // Palma
        fillRect(3, 13, 22, 22);
        // Pulgar doblado
        fillRect(1, 10, 5, 16);
        // Muñeca
        fillRect(5, 22, 20, 26);
    }

    GLFWimage img;
    img.width = W;
    img.height = H;
    img.pixels = pixels;

    // Hotspot: punta del dedo medio para mano abierta,
    //          centro nudillos para puño
    int hotX = closed ? 12 : 11;
    int hotY = closed ? 7 : 0;

    return glfwCreateCursor(&img, hotX, hotY);
}

// ─────────────────────────────────────────────────────────────────
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    windowWidth = width;
    windowHeight = height;
    glViewport(0, 0, width, height);
}

// ─────────────────────────────────────────────────────────────────
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    // Botón izquierdo → rotar
    if (button == GLFW_MOUSE_BUTTON_LEFT)
    {
        if (action == GLFW_PRESS)
        {
            dragging = true;
            firstMouse = true;
            prevTargetYaw = targetYaw;
            prevTargetPitch = targetPitch;
            velocityYaw = 0.0f;
            velocityPitch = 0.0f;
            glfwSetCursor(window, cursorHandGrab);
        }
        else if (action == GLFW_RELEASE)
        {
            dragging = false;
            glfwSetCursor(window, cursorHand);
        }
    }

    // Botón medio → pan
    if (button == GLFW_MOUSE_BUTTON_MIDDLE)
    {
        if (action == GLFW_PRESS) { panning = true;  firstPan = true; }
        else { panning = false; }
    }
}

// ─────────────────────────────────────────────────────────────────
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    targetMoveSpeed += (float)yoffset * 0.02f;
    if (targetMoveSpeed < 0.02f) targetMoveSpeed = 0.02f;
    if (targetMoveSpeed > 1.0f) targetMoveSpeed = 1.0f;
}

// ─────────────────────────────────────────────────────────────────
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    // Pan (botón medio)
    if (panning)
    {
        if (firstPan)
        {
            lastPanMouseX = (float)xpos;
            lastPanMouseY = (float)ypos;
            firstPan = false;
            return;
        }
        float dx = (float)xpos - lastPanMouseX;
        float dy = (float)ypos - lastPanMouseY;
        lastPanMouseX = (float)xpos;
        lastPanMouseY = (float)ypos;

        float panSens = 0.0025f * moveSpeed;
        targetPanX += dx * panSens;
        targetPanY -= dy * panSens;
        return;
    }

    // Rotación (botón izquierdo)
    if (!dragging) return;

    if (firstMouse)
    {
        lastX = (float)xpos;
        lastY = (float)ypos;
        firstMouse = false;
        return;
    }

    float dx = (float)xpos - lastX;
    float dy = (float)ypos - lastY;
    lastX = (float)xpos;
    lastY = (float)ypos;

    float sensitivity = 0.05f;

    prevTargetYaw = targetYaw;
    prevTargetPitch = targetPitch;

    targetYaw += dx * sensitivity;
    targetPitch -= dy * sensitivity;

    if (targetPitch > 78.0f) targetPitch = 78.0f;
    if (targetPitch < -78.0f) targetPitch = -78.0f;

    velocityYaw = (targetYaw - prevTargetYaw) * 0.6f;
    velocityPitch = (targetPitch - prevTargetPitch) * 0.6f;
}

} // namespace

// ─────────────────────────────────────────────────────────────────
int runInternalView()
{
    // ─── Reset state ────────────────────────────────────────────────
    goBackToExternal = false;
    camPos = glm::vec3(0.0f, -0.10f, 0.0f);
    cameraYaw = 180.0f; cameraPitch = 10.0f;
    targetYaw = 180.0f; targetPitch = 10.0f;
    moveSpeed = 0.10f; targetMoveSpeed = 0.10f;
    dragging = false; firstMouse = true; hideUI = false;
    selectedPart = 0;
    velocityYaw = 0.0f; velocityPitch = 0.0f;
    panning = false; panX = 0.0f; panY = 0.0f;
    targetPanX = 0.0f; targetPanY = 0.0f; firstPan = true;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* vidMode = glfwGetVideoMode(primaryMonitor);
    windowWidth = vidMode->width;
    windowHeight = vidMode->height;

    GLFWwindow* window = glfwCreateWindow(
        windowWidth, windowHeight,
        "Parte interna del corazon en 3D",
        NULL, NULL
    );

    if (!window) return -1;

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwGetFramebufferSize(window, &windowWidth, &windowHeight);
    glViewport(0, 0, windowWidth, windowHeight);

    lastX = windowWidth * 0.5f;
    lastY = windowHeight * 0.5f;

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);

    if (glewInit() != GLEW_OK) return -1;

    // ---------- IMGUI INIT ----------
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();

    style.WindowRounding = 14.0f;
    style.FrameRounding = 8.0f;
    style.WindowBorderSize = 1.5f;
    style.FrameBorderSize = 1.0f;
    style.ScrollbarRounding = 10.0f;

    ImVec4* colors = style.Colors;

    // Fondo panel
    colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.08f, 0.14f, 0.92f);

    // Botones
    colors[ImGuiCol_Button] = ImVec4(0.15f, 0.32f, 0.58f, 1.0f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.20f, 0.45f, 0.82f, 1.0f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.12f, 0.60f, 0.95f, 1.0f);

    // Bordes
    colors[ImGuiCol_Border] = ImVec4(0.2f, 0.7f, 1.0f, 0.65f);

    // Header
    colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.20f, 0.42f, 1.0f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.35f, 0.75f, 1.0f);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);

    // Crear cursores de manita
    cursorHand = makeHandCursor(false);
    cursorHandGrab = makeHandCursor(true);
    glfwSetCursor(window, cursorHand);

    // ──────────────────────────────────────────────────────────────
    Shader shader(
        "res/Shader/internal_vertex.glsl",
        "res/Shader/internal_fragment.glsl"
    );

    ModelInternal heartInternal(
        "res/models/heart_internal_glb/source/human_heart_3d_model_fbx_gltf.glb"
    );

    double lastTime = glfwGetTime();
    double goBackTime = 0.0;

    // ─────────────────────────── LOOP ────────────────────────────
    while (!glfwWindowShouldClose(window))
    {
        if (goBackToExternal && glfwGetTime() - goBackTime > 0.5)
            break;

        glfwPollEvents();

        static bool shiftPressedLastFrame = false;

        bool shiftPressed =
            glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;

        if (shiftPressed && !shiftPressedLastFrame)
            hideUI = !hideUI;

        shiftPressedLastFrame = shiftPressed;
        
        double now = glfwGetTime();
        float dt = (float)(now - lastTime);
        lastTime = now;

        float time = (float)glfwGetTime(); //desde
        float bpm = 60.0f;                    // latidos por minuto
        float beatFreq = bpm / 60.0f;         // Hz

        float phase = fmod(time * beatFreq, 1.0f);

        float heartbeat = 0.0f;

        // auricular kick (pequeño)
        heartbeat += exp(-pow((phase - 0.20f) / 0.050f, 2.0f)) * 0.25f;

        // ventricular contraction (principal)
        heartbeat += exp(-pow((phase - 0.42f) / 0.070f, 2.0f)) * 1.0f;

        dt = glm::clamp(dt, 0.0f, 0.05f);
        
        
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS && !goBackToExternal)
        {
            goBackToExternal = true;
            goBackTime = glfwGetTime();
        }

        // ── Inercia post-release ──────────────────────────────────
        if (!dragging)
        {
            float decay = pow(0.88f, dt * 60.0f);
            velocityYaw *= decay;
            velocityPitch *= decay;

            targetYaw += velocityYaw;
            targetPitch += velocityPitch;

            if (targetPitch > 78.0f) { targetPitch = 78.0f; velocityPitch = 0.0f; }
            if (targetPitch < -78.0f) { targetPitch = -78.0f; velocityPitch = 0.0f; }
        }

        // ── Lerp suave ─────────────────────────────────────────────
        float lerpSpeed = 1.0f - pow(0.001f, dt);

        cameraYaw += (targetYaw - cameraYaw) * lerpSpeed;
        cameraPitch += (targetPitch - cameraPitch) * lerpSpeed;
        moveSpeed += (targetMoveSpeed - moveSpeed) * lerpSpeed;
        panX += (targetPanX - panX) * lerpSpeed;
        panY += (targetPanY - panY) * lerpSpeed;

        // ── Direcciones de cámara ─────────────────────────────────
        float yawRad = glm::radians(cameraYaw);
        float pitchRad = glm::radians(cameraPitch);

        glm::vec3 forward;
        forward.x = cos(pitchRad) * cos(yawRad);
        forward.y = sin(pitchRad);
        forward.z = cos(pitchRad) * sin(yawRad);
        forward = glm::normalize(forward);

        glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec3 right = glm::normalize(glm::cross(forward, up));

        // ── WASD ──────────────────────────────────────────────────
        float speed = moveSpeed * dt * 5.0f;
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            camPos += forward * speed;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            camPos -= forward * speed;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            camPos -= right * speed;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            camPos += right * speed;
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
            camPos += up * speed;
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
            camPos -= up * speed;

        // ── Render ────────────────────────────────────────────────
        glClearColor(
            0.01f,   // R
            0.03f,   // G
            0.08f,   // B
            1.0f
        );
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        shader.use();
        float cycle = fmod((float)glfwGetTime() * 1.2f, 1.0f);

        // 0–0.45 = diástole
        // 0.45–0.55 = cierre AV
        // 0.55–0.85 = eyección
        // 0.85–1.0 = relajación

        for (int i = 0; i < 100; i++)
        {
            glm::mat4 boneMatrix = glm::mat4(1.0f);

            float avOpen = 0.0f;

            // Diástole: abierta
            if (cycle < 0.42f)
            {
                avOpen = 1.0f;
            }
            // Cierre explosivo
            else if (cycle < 0.50f)
            {
                float t = (cycle - 0.42f) / 0.08f;
                avOpen = 1.0f - pow(t, 0.35f);
            }
            // Rebote elástico
            else if (cycle < 0.56f)
            {
                float t = (cycle - 0.50f) / 0.06f;
                avOpen = sin(t * 3.14159f) * 0.18f;
            }
            float avAngle = avOpen * 14.0f;

            // ---------------- MITRAL ----------------
            if (i == 11 || i == 12)
            {
                std::string boneName =
                    (i == 11)
                    ? "left_mitral_valve_jnt.15"
                    : "right_mitral_valve_jnt.16";

                const BoneInfo* bone = heartInternal.GetBoneInfo(boneName);

                if (bone)
                {
                    float angle =
                        (i == 11)
                        ? -avAngle * 0.50f
                        : avAngle * 0.98f;

                    glm::mat4 rot = glm::rotate(
                        glm::mat4(1.0f),
                        glm::radians(angle),
                        glm::normalize(glm::vec3(0.35f, 0.0f, 1.0f))
                    );

                    float inward = 0.0f;

                    if (i == 11)
                        inward = avOpen * 0.14f;   
                    else
                        inward = -avOpen * 0.12f;  

                    glm::mat4 slide = glm::translate(
                        glm::mat4(1.0f),
                        glm::vec3(inward, 0.0f, 0.0f)
                    );

                    float liftY = (i == 11)
                        ? avOpen * 0.06f
                        : avOpen * 0.02f;

                    glm::mat4 lift = glm::translate(
                        glm::mat4(1.0f),
                        glm::vec3(0.0f, liftY, 0.0f)
                    );

                    boneMatrix =
                        glm::inverse(bone->offset) *
                        lift *
                        slide *
                        rot *
                        bone->offset;
                }
            }

            // ---------------- TRICUSPIDE ----------------
            if (i == 19 || i == 20)
            {
                std::string boneName =
                    (i == 19)
                    ? "left_tricuspid_valve_jnt.23"
                    : "right_tricuspid_valve_jnt.24";

                const BoneInfo* bone = heartInternal.GetBoneInfo(boneName);

                if (bone)
                {
                    float bounce =
                        sin((float)glfwGetTime() * 6.0f) *
                        pow(avOpen, 2.0f) * 0.07f;

                    float triAngle =
                        (i == 19)
                        ? -(avAngle * 0.54f + bounce)
                        : (avAngle * 0.38f + bounce);

                    glm::mat4 rot = glm::rotate(
                        glm::mat4(1.0f),
                        glm::radians(triAngle),
                        glm::normalize(glm::vec3(0.25f, 0.0f, 1.0f))
                    );

                    float inward =
                        (i == 19)
                        ? avOpen * 0.20f
                        : -avOpen * 0.020f;

                    glm::mat4 slide = glm::translate(
                        glm::mat4(1.0f),
                        glm::vec3(inward, 0.0f, 0.0f)
                    );

                    boneMatrix =
                        glm::inverse(bone->offset) *
                        slide *
                        rot *
                        bone->offset;
                }
            }
            
            // ---------------- AORTICA ----------------
            if (i == 13 || i == 15 || i == 17)
            {
                std::string boneName;

                if (i == 13)
                    boneName = "aortic_valve_02_jnt.17"; // ARRIBA
                else if (i == 15)
                    boneName = "aortic_valve_03_jnt.19"; // derecha
                else
                    boneName = "aortic_valve_01_jnt.21"; // izquierda

                const BoneInfo* bone = heartInternal.GetBoneInfo(boneName);

                if (bone)
                {
                    float semiOpen = 0.0f;

                    if (cycle > 0.55f && cycle < 0.85f)
                    {
                        float t = (cycle - 0.55f) / 0.30f;
                        semiOpen = sin(t * 3.14159f);
                        semiOpen = pow(semiOpen, 0.55f);
                    }

                    float angle = semiOpen * 20.0f;

                    glm::vec3 axis;
                    glm::mat4 slide = glm::mat4(1.0f);

                    // ---------- PETALO SUPERIOR ----------
                    if (i == 13)
                    {
                        angle *= -1.0f;
                        axis = glm::vec3(1.0f, 0.2f, 0.0f);

                        slide = glm::translate(
                            glm::mat4(1.0f),
                            glm::vec3(
                                0.0f,
                                semiOpen * 0.12f,
                                semiOpen * 0.18f
                            )
                        );
                    }

                    // ---------- DERECHA ----------
                    else if (i == 15)
                    {
                        axis = glm::vec3(-1.0f, 0.0f, 0.3f);
                    }

                    // ---------- IZQUIERDA ----------
                    else
                    {
                        angle *= 1.18f;   // abre un poco más
                        axis = glm::vec3(0.65f, -0.35f, 0.55f);
                    }

                    glm::mat4 rot = glm::rotate(
                        glm::mat4(1.0f),
                        glm::radians(angle),
                        glm::normalize(axis)
                    );

                    boneMatrix =
                        glm::inverse(bone->offset) *
                        slide *
                        rot *
                        bone->offset;
                }
            }

            // ---------------- PULMONAR ----------------
            if (i == 5 || i == 7)
            {
                std::string boneName =
                    (i == 5)
                    ? "right_pulmonary_valve_jnt.9"
                    : "left_pulmonary_valve_jnt.11";

                const BoneInfo* bone = heartInternal.GetBoneInfo(boneName);

                if (bone)
                {
                    float pulmOpen = 0.0f;

                    if (cycle > 0.55f && cycle < 0.82f)
                    {
                        float t = (cycle - 0.55f) / 0.27f;
                        pulmOpen = sin(t * 3.14159f);
                        pulmOpen = pow(pulmOpen, 1.15f); // menos chasquido
                    }

                    float angle =
                        (i == 5)
                        ? -pulmOpen * 16.0f
                        : pulmOpen * 16.0f;

                    glm::vec3 axis =
                        (i == 5)
                        ? glm::vec3(0.7f, 0.35f, 0.2f)
                        : glm::vec3(-0.7f, 0.35f, 0.2f);

                    glm::mat4 rot = glm::rotate(
                        glm::mat4(1.0f),
                        glm::radians(angle),
                        glm::normalize(axis)
                    );

                    float inward =
                        (i == 5)
                        ? pulmOpen * 0.018f
                        : -pulmOpen * 0.018f;

                    glm::mat4 lift = glm::translate(
                        glm::mat4(1.0f),
                        glm::vec3(
                            inward,
                            pulmOpen * 0.055f,
                            pulmOpen * 0.03f
                        )
                    );

                    boneMatrix =
                        glm::inverse(bone->offset) *
                        lift *
                        rot *
                        bone->offset;
                }
            }

            std::string uniformName =
                "finalBonesMatrices[" + std::to_string(i) + "]";

            glUniformMatrix4fv(
                glGetUniformLocation(shader.ID, uniformName.c_str()),
                1,
                GL_FALSE,
                glm::value_ptr(boneMatrix)
            );
        }

        glUniform1f(
            glGetUniformLocation(shader.ID, "heartbeat"),
            heartbeat
        );

        glm::mat4 model = glm::mat4(1.0f);

        model = glm::translate(model, glm::vec3(0.0f, -0.80f, 0.01f));

        // LATIDO GLOBAL
        float pulseScale = 0.104f * (1.0f + heartbeat * 0.018f);

        float fillPhase = sin(phase * 6.28318f - 1.1f) * 0.5f + 0.5f;
        fillPhase = pow(fillPhase, 1.8f);

        float globalPulse = 1.0f + fillPhase * 0.018f;

        model = glm::scale(model, glm::vec3(
            0.104f * globalPulse,
            0.104f * (1.0f + fillPhase * 0.012f),
            0.104f * globalPulse
        ));

        model = glm::rotate(model, glm::radians(-8.0f), glm::vec3(1, 0, 0));
        model = glm::rotate(model, glm::radians(18.0f), glm::vec3(0, 1, 0));

        glm::mat4 view = glm::lookAt(
            camPos,
            camPos + forward,
            up
        );

        float aspect = (float)windowWidth / (float)std::max(windowHeight, 1);
        glm::mat4 projection = glm::perspective(
            glm::radians(40.0f),
            aspect,
            0.01f,
            50.0f
        );

        glUniformMatrix4fv(glGetUniformLocation(shader.ID, "model"),
            1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(shader.ID, "view"),
            1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shader.ID, "projection"),
            1, GL_FALSE, glm::value_ptr(projection));

        glUniform3f(glGetUniformLocation(shader.ID, "lightPos"),
            3.2f, 4.8f, 5.8f);
        glUniform3f(glGetUniformLocation(shader.ID, "viewPos"),
            camPos.x, camPos.y, camPos.z);

        shader.setInt("baseTexture", 0);
        shader.setInt("normalTexture", 1);

        heartInternal.Draw();

        // ---------- IMGUI FRAME ----------
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (!hideUI)
        {
            float menuW = std::min(320.0f, windowWidth * 0.25f);
            float menuH = std::min(480.0f, windowHeight * 0.55f);
            ImGui::SetNextWindowPos(ImVec2(20, 40), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(menuW, menuH), ImGuiCond_Always);

            ImGui::Begin("ATLAS ANATOMICO", nullptr,
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoCollapse);

            ImGui::Text("Seleccione estructura:");
            ImGui::Separator();

            float btnW = menuW - 30;
            if (ImGui::Button("Aorta", ImVec2(btnW, 28)))
                selectedPart = 1;
            if (ImGui::Button("Valvula Pulmonar", ImVec2(btnW, 28)))
                selectedPart = 2;
            if (ImGui::Button("Valvula Mitral", ImVec2(btnW, 28)))
                selectedPart = 3;
            if (ImGui::Button("Valvula Tricuspide", ImVec2(btnW, 28)))
                selectedPart = 4;
            if (ImGui::Button("Valvula Aortica", ImVec2(btnW, 28)))
                selectedPart = 5;
            if (ImGui::Button("Auricula Izquierda", ImVec2(btnW, 28)))
                selectedPart = 6;
            if (ImGui::Button("Auricula Derecha", ImVec2(btnW, 28)))
                selectedPart = 7;
            if (ImGui::Button("Ventriculo Izquierdo", ImVec2(btnW, 28)))
                selectedPart = 8;
            if (ImGui::Button("Ventriculo Derecho", ImVec2(btnW, 28)))
                selectedPart = 9;

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();


            ImGui::End();
        }
        if (!hideUI && selectedPart != 0)
        {
            float infoW = std::min(280.0f, windowWidth * 0.22f);
            float infoH = std::min(340.0f, windowHeight * 0.40f);
            ImGui::SetNextWindowPos(ImVec2(windowWidth - infoW - 20, 40), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(infoW, infoH), ImGuiCond_Always);

            ImGui::Begin("INFORMACION", nullptr,
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoCollapse);

            if (selectedPart == 1)
            {
                ImGui::TextColored(ImVec4(0, 0.9f, 1, 1), "1  AORTA");
                ImGui::Separator();
                ImGui::Text("UBICACION");
                ImGui::TextWrapped("Arteria principal que sale del ventriculo izquierdo.");
                ImGui::Spacing();
                ImGui::Text("FUNCION");
                ImGui::TextWrapped("Transporta sangre oxigenada hacia todo el cuerpo.");
            }
            else if (selectedPart == 2)
            {
                ImGui::TextColored(ImVec4(0, 0.9f, 1, 1), "2  VALVULA PULMONAR");
                ImGui::Separator();
                ImGui::Text("UBICACION");
                ImGui::TextWrapped("Entre ventriculo derecho y arteria pulmonar.");
                ImGui::Spacing();
                ImGui::Text("FUNCION");
                ImGui::TextWrapped("Permite paso de sangre hacia pulmones.");
            }
            else if (selectedPart == 3)
            {
                ImGui::TextColored(ImVec4(0, 0.9f, 1, 1), "3  VALVULA MITRAL");
                ImGui::Separator();
                ImGui::Text("UBICACION");
                ImGui::TextWrapped("Entre auricula izquierda y ventriculo izquierdo.");
                ImGui::Spacing();
                ImGui::Text("FUNCION");
                ImGui::TextWrapped("Evita reflujo de sangre.");
            }
            else if (selectedPart == 4)
            {
                ImGui::TextColored(ImVec4(0, 0.9f, 1, 1), "4  VALVULA TRICUSPIDE");
                ImGui::Separator();
                ImGui::Text("UBICACION");
                ImGui::TextWrapped("Entre auricula derecha y ventriculo derecho.");
                ImGui::Spacing();
                ImGui::Text("FUNCION");
                ImGui::TextWrapped("Impide retroceso de sangre.");
            }
            else if (selectedPart == 5)
            {
                ImGui::TextColored(ImVec4(0, 0.9f, 1, 1), "5  VALVULA AORTICA");
                ImGui::Separator();
                ImGui::Text("UBICACION");
                ImGui::TextWrapped("Entre ventriculo izquierdo y aorta.");
                ImGui::Spacing();
                ImGui::Text("FUNCION");
                ImGui::TextWrapped("Permite salida de sangre a la aorta.");
            }
            else if (selectedPart == 6)
            {
                ImGui::TextColored(ImVec4(0, 0.9f, 1, 1), "6  AURICULA IZQUIERDA");
                ImGui::Separator();
                ImGui::TextWrapped("Recibe sangre oxigenada de pulmones.");
            }
            else if (selectedPart == 7)
            {
                ImGui::TextColored(ImVec4(0, 0.9f, 1, 1), "7  AURICULA DERECHA");
                ImGui::Separator();
                ImGui::TextWrapped("Recibe sangre desoxigenada del cuerpo.");
            }
            else if (selectedPart == 8)
            {
                ImGui::TextColored(ImVec4(0, 0.9f, 1, 1), "8  VENTRICULO IZQUIERDO");
                ImGui::Separator();
                ImGui::TextWrapped("Bombea sangre a la aorta.");
            }
            else if (selectedPart == 9)
            {
                ImGui::TextColored(ImVec4(0, 0.9f, 1, 1), "9  VENTRICULO DERECHO");
                ImGui::Separator();
                ImGui::TextWrapped("Bombea sangre hacia pulmones.");
            }

            ImGui::End();
        }

        ImDrawList* draw = ImGui::GetForegroundDrawList();

        // ---------- DIBUJAR TODOS LOS PUNTOS ----------
        if (!hideUI)
        {
            for (auto& m : markers)
            {
                ImVec2 p = WorldToScreen(
                    m.pos,
                    model,
                    view,
                    projection,
                    windowWidth,
                    windowHeight
                );

                draw->AddCircleFilled(
                    p,
                    14.0f,
                    IM_COL32(40, 40, 40, 220)
                );

                draw->AddCircle(
                    p,
                    14.0f,
                    IM_COL32(255, 255, 255, 255),
                    32,
                    2.0f
                );

                char txt[8];
                sprintf_s(txt, "%d", m.id);

                draw->AddText(
                    ImVec2(p.x - 4, p.y - 7),
                    IM_COL32(255, 255, 255, 255),
                    txt
                );
            }
        }
        
        // ---------- HIGHLIGHT SELECCIONADO ----------
        if (!hideUI && selectedPart >= 1 && selectedPart <= 9)
        {

            Marker& m = markers[selectedPart - 1];

            ImVec2 p = WorldToScreen(
                m.pos,
                model,
                view,
                projection,
                windowWidth,
                windowHeight
            );

            float pulse = sin(glfwGetTime() * 5.0f) * 4.0f;

            draw->AddCircle(p, 20 + pulse,
                IM_COL32(0, 255, 255, 255), 64, 3.0f);

            draw->AddCircle(p, 30 + pulse,
                IM_COL32(0, 220, 255, 180), 64, 2.0f);

            draw->AddCircle(p, 40 + pulse,
                IM_COL32(0, 180, 255, 120), 64, 2.0f);
        }

        if (!hideUI)
        {
            float ctrlW = std::min(260.0f, windowWidth * 0.20f);
            ImGui::SetNextWindowPos(ImVec2(20, windowHeight - 140), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(ctrlW, 110), ImGuiCond_Always);

            ImGui::Begin("Controles", nullptr,
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoCollapse);

            ImGui::Text("CONTROLES");
            ImGui::Separator();
            ImGui::Text("WASD: Moverse");
            ImGui::Text("Q/E: Subir/Bajar");
            ImGui::Text("Rueda: Velocidad");
            ImGui::Text("Click izq: Mirar");
            ImGui::Text("Shift: Ocultar UI");
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0,1,1,1), "ENTER: Volver exterior");
            ImGui::Text("BACKSPACE: Salir");

            ImGui::End();
        }

        if (goBackToExternal)
        {
            ImGui::SetNextWindowPos(
                ImVec2(windowWidth * 0.5f - 210, windowHeight * 0.5f - 60),
                ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(420, 120), ImGuiCond_Always);
            ImGui::Begin("Transition", nullptr,
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoTitleBar);
            ImGui::TextColored(ImVec4(0,1,1,1), "VOLVIENDO A VISTA EXTERNA...");
            ImGui::Separator();
            ImGui::TextWrapped("Regresando al modo orbital...");
            ImGui::End();
        }

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (glfwGetKey(window, GLFW_KEY_BACKSPACE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        glfwSwapBuffers(window);
    }

    glfwDestroyCursor(cursorHand);
    glfwDestroyCursor(cursorHandGrab);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    return goBackToExternal ? 1 : 0;
}