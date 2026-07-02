#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <cmath>
#include <algorithm>

#include "src/Shader.h"
#include "src/Model.h"

#include "external/imgui/imgui.h"
#include "external/imgui/imgui_impl_glfw.h"
#include "external/imgui/imgui_impl_opengl3.h"

int runInternalView();

float cameraYaw = 90.0f;
float cameraPitch = 0.0f;
float targetYaw = 90.0f;
float targetPitch = 0.0f;
float cameraDistance = 6.0f;
float targetDistance = 6.0f;

float lastX = 0.0f;
float lastY = 0.0f;

float mouseX = 0.0f;
float mouseY = 0.0f;

bool dragging = false;
bool panning = false;

float panX = 0.0f;
float panY = 0.0f;
float targetPanX = 0.0f;
float targetPanY = 0.0f;

float velocityYaw = 0.0f;
float velocityPitch = 0.0f;
float prevTargetYaw = 90.0f;
float prevTargetPitch = 0.0f;
float panZ = 0.0f;
bool firstMouse = true;

int selectedPart = 0;

// NUEVO V6
bool showUI = true;
bool shiftPressedLastFrame = false;
bool switchToInternal = false;
double switchTime = 0.0;
GLFWcursor* handCursor = nullptr;
GLFWcursor* grabCursor = nullptr;

int winW = 0, winH = 0;

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    winW = width;
    winH = height;
    glViewport(0, 0, width, height);
}

bool IsMouseOver(ImVec2 p, float radius)
{
    float dx = mouseX - p.x;
    float dy = mouseY - p.y;
    return (dx * dx + dy * dy) <= radius * radius;
}

ImVec2 Lerp(ImVec2 a, ImVec2 b, float t)
{
    ImVec2 p;
    p.x = a.x + (b.x - a.x) * t;
    p.y = a.y + (b.y - a.y) * t;
    return p;
}

ImVec2 Bezier(ImVec2 p0, ImVec2 p1, ImVec2 p2, float t)
{
    float u = 1.0f - t;
    float tt = t * t;
    float uu = u * u;

    ImVec2 p;
    p.x = uu * p0.x + 2.0f * u * t * p1.x + tt * p2.x;
    p.y = uu * p0.y + 2.0f * u * t * p1.y + tt * p2.y;
    return p;
}

// Partículas mejoradas
void DrawBloodParticle(ImDrawList* drawList, ImVec2 pos, ImU32 color)
{
    drawList->AddCircleFilled(pos, 16.0f, IM_COL32(255, 255, 255, 6));
    drawList->AddCircleFilled(pos, 10.0f, IM_COL32(255, 255, 255, 14));
    drawList->AddCircleFilled(pos, 5.0f, color);
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT)
    {
        if (action == GLFW_PRESS)
        {
            dragging = true;
            firstMouse = true;
        }
        else if (action == GLFW_RELEASE)
        {
            dragging = false;
        }
    }

    if (button == GLFW_MOUSE_BUTTON_MIDDLE)
    {
        if (action == GLFW_PRESS)
        {
            panning = true;
            firstMouse = true;
        }
        else if (action == GLFW_RELEASE)
        {
            panning = false;
        }
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    targetDistance -= (float)yoffset * 0.22f;

    if (targetDistance < 0.15f)
        targetDistance = 0.15f;

    if (targetDistance > 12.0f)
        targetDistance = 12.0f;
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    mouseX = (float)xpos;
    mouseY = (float)ypos;

    if (!dragging && !panning)
        return;

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

    if (dragging)
    {
        float sensitivity = 0.22f * (cameraDistance / 6.0f);

        prevTargetYaw = targetYaw;
        prevTargetPitch = targetPitch;

        targetYaw += dx * sensitivity;
        targetPitch -= dy * sensitivity;

        if (targetPitch > 75.0f) targetPitch = 75.0f;
        if (targetPitch < -75.0f) targetPitch = -75.0f;
        velocityYaw = (targetYaw - prevTargetYaw) * 0.6f;
        velocityPitch = (targetPitch - prevTargetPitch) * 0.6f;
    }

    if (panning)
    {
        float panSpeed = 0.0045f * cameraDistance;

        targetPanX += dx * panSpeed;
        targetPanY -= dy * panSpeed;
    }
}

ImVec2 WorldToScreen(
    glm::vec3 worldPos,
    glm::mat4 view,
    glm::mat4 projection,
    int screenWidth,
    int screenHeight)
{
    glm::vec4 clip = projection * view * glm::vec4(worldPos, 1.0f);

    if (clip.w <= 0.0f)
        return ImVec2(-1000, -1000);

    glm::vec3 ndc = glm::vec3(clip) / clip.w;

    ImVec2 screen;
    screen.x = ((ndc.x + 1.0f) * 0.5f) * screenWidth;
    screen.y = ((1.0f - ndc.y) * 0.5f) * screenHeight;

    return screen;
}

int main()
{
    glfwInit();

    while (true)
    {
        // ─── Reset state ────────────────────────────────────────────
        cameraYaw = 90.0f; cameraPitch = 0.0f;
        targetYaw = 90.0f; targetPitch = 0.0f;
        cameraDistance = 6.0f; targetDistance = 6.0f;
        panX = 0.0f; panY = 0.0f; targetPanX = 0.0f; targetPanY = 0.0f;
        velocityYaw = 0.0f; velocityPitch = 0.0f;
        selectedPart = 0; showUI = true; switchToInternal = false;
        dragging = false; shiftPressedLastFrame = false;

        // ─── INIT EXTERNAL ──────────────────────────────────────────

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        GLFWmonitor* primaryMon = glfwGetPrimaryMonitor();
        const GLFWvidmode* vm = glfwGetVideoMode(primaryMon);
        winW = vm->width;
        winH = vm->height;

        GLFWwindow* window = glfwCreateWindow(
            winW, winH, "CARDIAC VISION 3D Grupo 9", NULL, NULL
        );

        if (!window)
            return -1;
        glfwMakeContextCurrent(window);
        glfwGetFramebufferSize(window, &winW, &winH);
        glViewport(0, 0, winW, winH);
        lastX = winW * 0.5f;
        lastY = winH * 0.5f;

        handCursor = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
        grabCursor = glfwCreateStandardCursor(GLFW_HAND_CURSOR);

        glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
        glfwSetCursorPosCallback(window, mouse_callback);
        glfwSetMouseButtonCallback(window, mouse_button_callback);
        glfwSetScrollCallback(window, scroll_callback);

        if (glewInit() != GLEW_OK)
            return -1;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        (void)io;
        io.Fonts->AddFontFromFileTTF(
            "C:/Windows/Fonts/segoeui.ttf", 18.0f
        );
        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 330");

        glEnable(GL_DEPTH_TEST);

        Shader shader(
            "res/Shader/vertexShader.glsl",
            "res/Shader/fragmentShader.glsl"
        );

        Model heart("res/models/heart_real/scene.gltf");

        switchToInternal = false;
        bool goToInternal = false;

        while (!glfwWindowShouldClose(window) && !goToInternal)
        {
            static double lastTime = glfwGetTime();
            double now = glfwGetTime();
            float dt = (float)(now - lastTime);
            lastTime = now;

            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
                glfwSetWindowShouldClose(window, true);

            if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS && !switchToInternal)
            {
                switchToInternal = true;
                switchTime = glfwGetTime();
            }

            if (dragging)
                glfwSetCursor(window, grabCursor);
            else
                glfwSetCursor(window, handCursor);

            bool shiftNow =
                glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;

            if (shiftNow && !shiftPressedLastFrame)
                showUI = !showUI;

            shiftPressedLastFrame = shiftNow;

            glClearColor(0.015f, 0.04f, 0.09f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            shader.use();
            shader.setInt("baseTexture", 0);
            shader.setInt("normalTexture", 1);

            if (!dragging)
            {
                float decay = pow(0.88f, dt * 60.0f);

                velocityYaw *= decay;
                velocityPitch *= decay;

                targetYaw += velocityYaw;
                targetPitch += velocityPitch;
            }

            // ── WASD externo ──────────────────────────────────────────
            float orbitSpeed = 60.0f * dt;
            float zoomSpeed = 3.0f * dt;
            if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
                targetDistance -= zoomSpeed;
            if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
                targetDistance += zoomSpeed;
            if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
                targetYaw += orbitSpeed;
            if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
                targetYaw -= orbitSpeed;
            if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
                targetPitch += orbitSpeed;
            if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
                targetPitch -= orbitSpeed;

            if (targetDistance < 0.15f) targetDistance = 0.15f;
            if (targetDistance > 12.0f) targetDistance = 12.0f;
            if (targetPitch > 75.0f) targetPitch = 75.0f;
            if (targetPitch < -75.0f) targetPitch = -75.0f;

            float lerpSpeed = 1.0f - pow(0.001f, dt);

            cameraYaw += (targetYaw - cameraYaw) * lerpSpeed;
            cameraPitch += (targetPitch - cameraPitch) * lerpSpeed;
            cameraDistance += (targetDistance - cameraDistance) * lerpSpeed;

            panX += (targetPanX - panX) * lerpSpeed;
            panY += (targetPanY - panY) * lerpSpeed;

            glm::mat4 model = glm::mat4(1.0f);

            float time = (float)glfwGetTime();

            float pulse1 = pow(std::max(0.0f, sin(time * 5.2f)), 18.0f) * 0.05f;
            float pulse2 = pow(std::max(0.0f, sin(time * 5.2f - 0.65f)), 22.0f) * 0.03f;
            float beat = 1.0f + pulse1 + pulse2;

            model = glm::scale(model, glm::vec3(3.5f * beat));
            model = glm::rotate(model, glm::radians(270.0f), glm::vec3(0, 1, 0));
            model = glm::rotate(model, glm::radians(-90.0f), glm::vec3(1, 0, 0));

            float yawRad = glm::radians(cameraYaw);
            float pitchRad = glm::radians(cameraPitch);

            glm::vec3 cameraPos;
            cameraPos.x = cameraDistance * cos(pitchRad) * cos(yawRad);
            cameraPos.y = cameraDistance * sin(pitchRad);
            cameraPos.z = cameraDistance * cos(pitchRad) * sin(yawRad);

            glm::vec3 cameraTarget(panX, panY, panZ);

            glm::mat4 view = glm::lookAt(
                cameraPos + cameraTarget,
                cameraTarget,
                glm::vec3(0.0f, 1.0f, 0.0f)
            );

            float aspect = (float)winW / (float)std::max(winH, 1);
            glm::mat4 projection = glm::perspective(
                glm::radians(35.0f), aspect, 0.01f, 100.0f
            );

            glUniformMatrix4fv(
                glGetUniformLocation(shader.ID, "model"),
                1, GL_FALSE, glm::value_ptr(model)
            );
            glUniformMatrix4fv(
                glGetUniformLocation(shader.ID, "view"),
                1, GL_FALSE, glm::value_ptr(view)
            );
            glUniformMatrix4fv(
                glGetUniformLocation(shader.ID, "projection"),
                1, GL_FALSE, glm::value_ptr(projection)
            );
            glUniform3f(glGetUniformLocation(shader.ID, "lightPos"),
                3.0f, 5.5f, 4.0f);
            glUniform3f(glGetUniformLocation(shader.ID, "viewPos"),
                cameraPos.x, cameraPos.y, cameraPos.z);

            heart.Draw();

            ImDrawList* drawList = ImGui::GetForegroundDrawList();

            glm::vec3 aortaPos(0.1f, 1.0f, 0.0f);
            glm::vec3 ventLeftPos(0.32f, -0.62f, 0.28f);
            glm::vec3 ventRightPos(-0.18f, -0.55f, 0.25f);
            glm::vec3 auriculaPos(-0.55f, 0.25f, 0.20f);

            ImVec2 aorta2D = WorldToScreen(aortaPos, view, projection, winW, winH);
            ImVec2 ventLeft2D = WorldToScreen(ventLeftPos, view, projection, winW, winH);
            ImVec2 ventRight2D = WorldToScreen(ventRightPos, view, projection, winW, winH);
            ImVec2 auricula2D = WorldToScreen(auriculaPos, view, projection, winW, winH);
            if (showUI)
            {
                float flowTime = fmod((float)glfwGetTime() * 0.65f, 1.0f);

                ImVec2 controlBlue;
                controlBlue.x = (auricula2D.x + ventRight2D.x) * 0.5f - 40.0f;
                controlBlue.y = (auricula2D.y + ventRight2D.y) * 0.5f + 20.0f;

                for (int i = 0; i < 9; i++)
                {
                    float t = fmod(flowTime + i * 0.11f, 1.0f);
                    ImVec2 blood = Bezier(auricula2D, controlBlue, ventRight2D, t);
                    DrawBloodParticle(drawList, blood, IM_COL32(90, 170, 255, 220));
                }

                ImVec2 controlRed;
                controlRed.x = (ventLeft2D.x + aorta2D.x) * 0.5f + 60.0f;
                controlRed.y = (ventLeft2D.y + aorta2D.y) * 0.5f - 40.0f;

                for (int i = 0; i < 9; i++)
                {
                    float t = fmod(flowTime + i * 0.11f, 1.0f);
                    ImVec2 blood = Bezier(ventLeft2D, controlRed, aorta2D, t);
                    DrawBloodParticle(drawList, blood, IM_COL32(255, 80, 80, 220));
                }

                float r = 7.0f;

                drawList->AddCircleFilled(aorta2D,
                    IsMouseOver(aorta2D, r) ? 11.0f : r, IM_COL32(255, 255, 0, 255));
                drawList->AddCircleFilled(ventLeft2D,
                    IsMouseOver(ventLeft2D, r) ? 11.0f : r, IM_COL32(0, 255, 255, 255));
                drawList->AddCircleFilled(ventRight2D,
                    IsMouseOver(ventRight2D, r) ? 11.0f : r, IM_COL32(255, 120, 120, 255));
                drawList->AddCircleFilled(auricula2D,
                    IsMouseOver(auricula2D, r) ? 11.0f : r, IM_COL32(150, 255, 150, 255));

                float glowPulse = 16.0f + sin(glfwGetTime() * 4.0f) * 4.0f;

                auto drawSelectedGlow = [&](ImVec2 pos, ImU32 color)
                    {
                        drawList->AddCircle(pos, glowPulse, color, 32, 4.0f);
                        drawList->AddCircle(pos, glowPulse + 10.0f, color, 32, 2.0f);
                    };

                if (selectedPart == 1)
                    drawSelectedGlow(aorta2D, IM_COL32(255, 255, 0, 255));
                if (selectedPart == 2)
                    drawSelectedGlow(auricula2D, IM_COL32(150, 255, 150, 255));
                if (selectedPart == 3)
                    drawSelectedGlow(ventRight2D, IM_COL32(255, 120, 120, 255));
                if (selectedPart == 4)
                    drawSelectedGlow(ventLeft2D, IM_COL32(0, 255, 255, 255));

                if (IsMouseOver(aorta2D, r) &&
                    glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
                    selectedPart = 1;
                if (IsMouseOver(auricula2D, r) &&
                    glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
                    selectedPart = 2;
                if (IsMouseOver(ventRight2D, r) &&
                    glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
                    selectedPart = 3;
                if (IsMouseOver(ventLeft2D, r) &&
                    glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
                    selectedPart = 4;

                ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 18.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.2f);
                ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.02f, 0.05f, 0.10f, 0.82f));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.7f, 1.0f, 0.55f));
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.25f, 0.45f, 0.85f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.16f, 0.40f, 0.70f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.20f, 0.50f, 0.85f, 1.0f));
                float uiW = std::min(330.0f, winW * 0.28f);
                ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
                ImGui::SetNextWindowSize(ImVec2(uiW, std::min(610.0f, winH * 0.85f)), ImGuiCond_Always);

                ImGui::Begin("CARDIAC VISION 3D", nullptr,
                    ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);

                ImGui::TextColored(ImVec4(0.0f, 0.85f, 1.0f, 1.0f),
                    "Sistema Anatomico Interactivo");
                ImGui::Separator();

                switch (selectedPart)
                {
                case 0:
                    ImGui::TextWrapped("Seleccione un hotspot del corazon para explorar "
                        "la anatomia cardiaca en detalle.");
                    break;
                case 1:
                    ImGui::TextColored(ImVec4(1, 1, 0, 1), "AORTA");
                    ImGui::Separator();
                    ImGui::TextWrapped("Principal arteria del sistema circulatorio. "
                        "Transporta sangre oxigenada desde el "
                        "ventriculo izquierdo hacia todo el cuerpo.");
                    break;
                case 2:
                    ImGui::TextColored(ImVec4(0.4f, 1, 0.4f, 1), "AURICULA");
                    ImGui::Separator();
                    ImGui::TextWrapped("Cavidad superior encargada de recibir "
                        "la sangre venosa o pulmonar y dirigirla"
                        "hacia los ventrículos.");
                    break;
                case 3:
                    ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "VENTRICULO DERECHO");
                    ImGui::Separator();
                    ImGui::TextWrapped("Bombea sangre desoxigenada hacia "
                        "la circulación pulmonar para su"
                        "oxigenación en los pulmones.");
                    break;
                case 4:
                    ImGui::TextColored(ImVec4(0, 1, 1, 1), "VENTRICULO IZQUIERDO");
                    ImGui::Separator();
                    ImGui::TextWrapped("Cavidad muscular más potente del corazón. "
                        "Impulsa sangre oxigenada hacia "
                        "todo el organismo mediante la aorta.");
                    break;
                }

                ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "DATOS DEL CORAZON");
                ImGui::Button("4 Cavidades", ImVec2(130, 35));
                ImGui::SameLine();
                ImGui::Button("4 Valvulas", ImVec2(130, 35));
                ImGui::Button("72 Lat/min", ImVec2(130, 35));
                ImGui::SameLine();
                ImGui::Button("7500 L/dia", ImVec2(130, 35));
                ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

                ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "FLUJOS");
                ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "Flujo Azul");
                ImGui::TextWrapped("Sangre desoxigenada que regresa del cuerpo.");
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Flujo Rojo");
                ImGui::TextWrapped("Sangre oxigenada que sale por la aorta.");

                ImGui::Spacing(); ImGui::Separator();

                ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.0f, 1.0f), "CONTROLES");
                ImGui::BulletText("Mouse Izq: Rotar");
                ImGui::BulletText("Scroll: Zoom");
                ImGui::BulletText("WASD: Orbitar");
                ImGui::BulletText("Q/E: Subir/Bajar");
                ImGui::BulletText("SHIFT: Ocultar UI");
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f),
                    "ENTER: Vista Interna");

                ImGui::End();
                ImGui::PopStyleColor(5);
                ImGui::PopStyleVar(3);
            }
            if (switchToInternal)
            {
                ImGui::SetNextWindowPos(ImVec2(winW * 0.5f - 210, winH * 0.5f - 60), ImGuiCond_Always);
                ImGui::SetNextWindowSize(ImVec2(420, 120), ImGuiCond_Always);

                ImGui::Begin("Transition", nullptr,
                    ImGuiWindowFlags_NoResize |
                    ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoTitleBar);

                ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f),
                    "CAMBIANDO A VISTA INTERNA...");
                ImGui::Separator();
                ImGui::TextWrapped("Cargando exploracion de cavidades, valvulas "
                    "y estructuras internas del corazon...");
                ImGui::End();
            }

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            if (switchToInternal && glfwGetTime() - switchTime > 0.5)
                goToInternal = true;

            glfwSwapBuffers(window);
            glfwPollEvents();
        }

        // Cleanup external view
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyCursor(handCursor);
        glfwDestroyCursor(grabCursor);
        glfwDestroyWindow(window);

        if (!goToInternal)
            break;

        // Launch internal view
        int ret = runInternalView();
        if (ret == 0)
            break;

        // ret == 1 → restart external view
    }

    glfwTerminate();
    return 0;
}