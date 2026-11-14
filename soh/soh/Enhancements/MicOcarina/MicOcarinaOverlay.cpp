// Indicadores visuais do ocarina por microfone: faixas de luz e ícone de status no topo da tela.
#include "MicOcarinaOverlay.h"
#include "MicOcarina.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

// Definida em code_800EC960.c: true enquanto o ocarina está em uso (tocar livre, ensinar canção, etc).
extern "C" unsigned char AudioOcarina_IsActive(void);

namespace {
struct Streak {
    float x;     // ponta da frente (fração da largura da tela)
    float y;     // fração da altura
    float len;   // comprimento (fração da largura)
    float speed; // frações da largura por segundo
    float bright;
    ImU32 rgb;   // cor sem alpha
};

std::vector<Streak> gStreaks;
float gSpawnAcc = 0.0f;

float Rand01() {
    return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
}

ImU32 ColorForNote(int note) {
    if (note == 0) {
        return IM_COL32(90, 170, 255, 0); // A: azul
    }
    if (note >= 1 && note <= 4) {
        return IM_COL32(255, 220, 90, 0); // botões C: amarelo
    }
    return IM_COL32(210, 210, 210, 0); // som captado, mas não é uma nota válida
}

void DrawStreaks(ImGuiIO& io, float dt, float level, int note) {
    const float W = io.DisplaySize.x;
    const float H = io.DisplaySize.y;

    if (level > 0.0f) {
        gSpawnAcc += dt * (12.0f + 38.0f * level);
        while (gSpawnAcc >= 1.0f) {
            gSpawnAcc -= 1.0f;
            if (gStreaks.size() >= 96) {
                break;
            }
            Streak s;
            // Cada nota tem sua "pista": notas mais agudas passam mais alto na tela.
            float lane = note >= 0 ? 0.52f - 0.06f * static_cast<float>(note) : 0.22f + 0.30f * Rand01();
            s.y = lane + (Rand01() - 0.5f) * 0.03f;
            s.len = 0.05f + 0.09f * Rand01();
            s.x = -0.02f + 0.2f * Rand01();
            s.speed = 0.55f + 0.6f * Rand01();
            s.bright = 0.5f + 0.5f * level;
            s.rgb = ColorForNote(note);
            gStreaks.push_back(s);
        }
    } else {
        gSpawnAcc = 0.0f;
    }

    if (gStreaks.empty()) {
        return;
    }

    for (auto& s : gStreaks) {
        s.x += s.speed * dt;
    }
    gStreaks.erase(
        std::remove_if(gStreaks.begin(), gStreaks.end(), [](const Streak& s) { return s.x - s.len > 1.0f; }),
        gStreaks.end());

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const float thick = std::max(2.0f, 2.0f * ImGui::GetFontSize() / 13.0f);

    for (const auto& s : gStreaks) {
        const float fade = std::min(1.0f, std::max(0.0f, 1.0f - s.x)) * s.bright; // some ao chegar na borda direita
        const int a = static_cast<int>(fade * 220.0f);
        const ImU32 rgb = s.rgb & 0x00FFFFFF;
        const ImU32 head = rgb | (static_cast<ImU32>(a) << IM_COL32_A_SHIFT);
        const ImU32 tail = rgb; // alpha 0
        const float x1 = s.x * W;
        const float x0 = (s.x - s.len) * W;
        const float y = s.y * H;
        dl->AddRectFilledMultiColor(ImVec2(x0, y - thick * 0.5f), ImVec2(x1, y + thick * 0.5f), tail, head, head, tail);
    }
}

// Ícone de microfone desenhado com primitivas (não depende de fonte de ícones).
void DrawMicIcon(ImDrawList* dl, ImVec2 c, float r, bool on) {
    const ImU32 bg = on ? IM_COL32(240, 120, 30, 255) : IM_COL32(100, 100, 100, 255);
    const ImU32 fg = IM_COL32(255, 255, 255, 255);
    const float t = std::max(1.5f, r * 0.12f);
    dl->AddCircleFilled(c, r, bg, 32);
    dl->AddRectFilled(ImVec2(c.x - r * 0.2f, c.y - r * 0.52f), ImVec2(c.x + r * 0.2f, c.y + r * 0.12f), fg, r * 0.2f);
    dl->PathArcTo(ImVec2(c.x, c.y + r * 0.02f), r * 0.38f, 0.0f, IM_PI, 16);
    dl->PathStroke(fg, 0, t);
    dl->AddLine(ImVec2(c.x, c.y + r * 0.40f), ImVec2(c.x, c.y + r * 0.62f), fg, t);
    dl->AddLine(ImVec2(c.x - r * 0.24f, c.y + r * 0.62f), ImVec2(c.x + r * 0.24f, c.y + r * 0.62f), fg, t);
    if (!on) { // risco vermelho: microfone desligado
        dl->AddLine(ImVec2(c.x - r * 0.62f, c.y - r * 0.62f), ImVec2(c.x + r * 0.62f, c.y + r * 0.62f),
                    IM_COL32(230, 60, 60, 255), t * 1.6f);
    }
}

void DrawStatus(ImGuiIO& io, bool micEnabled, float level) {
    const char* text = !micEnabled ? "Microphone off" : (level > 0.0f ? "Listening to sound..." : "Microphone on");
    const float fs = ImGui::GetFontSize();
    const ImVec2 ts = ImGui::CalcTextSize(text);
    const float r = fs * 1.0f;
    const float padX = fs * 0.8f;
    const float h = r * 2.0f + fs * 0.5f;
    const float w = padX + r * 2.0f + fs * 0.7f + ts.x + padX;
    const float x0 = (io.DisplaySize.x - w) * 0.5f;
    const float y0 = io.DisplaySize.y * 0.04f;

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x0 + w, y0 + h), IM_COL32(10, 10, 14, 190), h * 0.5f);
    dl->AddRect(ImVec2(x0, y0), ImVec2(x0 + w, y0 + h), IM_COL32(255, 255, 255, 50), h * 0.5f);

    float pulse = 1.0f;
    if (micEnabled && level > 0.0f) { // o ícone "respira" enquanto ouve som
        pulse += 0.08f * static_cast<float>(std::sin(ImGui::GetTime() * 12.0));
    }
    DrawMicIcon(dl, ImVec2(x0 + padX * 0.6f + r, y0 + h * 0.5f), r * 0.85f * pulse, micEnabled);
    dl->AddText(ImVec2(x0 + padX * 0.6f + r * 2.0f + fs * 0.6f, y0 + (h - ts.y) * 0.5f),
                micEnabled ? IM_COL32(255, 255, 255, 255) : IM_COL32(190, 190, 190, 255), text);
}
} // namespace

void MicOcarinaOverlay_Draw(bool micEnabled, bool showStreaks, bool showStatus) {
    // Tudo isto só existe enquanto o jogador está com o ocarina em uso no jogo.
    if (!AudioOcarina_IsActive()) {
        gStreaks.clear();
        gSpawnAcc = 0.0f;
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    const float dt = std::min(io.DeltaTime, 0.1f);
    const float level = micEnabled ? MicOcarina_GetLevel() : 0.0f;

    if (showStreaks && micEnabled) {
        DrawStreaks(io, dt, level, MicOcarina_GetNote());
    } else {
        gStreaks.clear();
        gSpawnAcc = 0.0f;
    }
    if (showStatus) {
        DrawStatus(io, micEnabled, level);
    }
}
