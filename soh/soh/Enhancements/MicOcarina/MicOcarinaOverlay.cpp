// Indicador visual do ocarina por microfone (faixas de luz cruzando a tela).
#include "MicOcarinaOverlay.h"
#include "MicOcarina.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace {
struct Streak {
    float x;     // ponta da frente (fração da largura da tela)
    float y;     // fração da altura
    float len;   // comprimento (fração da largura)
    float speed; // frações da largura por segundo
    float bright;
    ImU32 rgb;   // cor sem alpha
};

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
} // namespace

void MicOcarinaOverlay_Draw() {
    static std::vector<Streak> streaks;
    static float spawnAcc = 0.0f;

    ImGuiIO& io = ImGui::GetIO();
    const float dt = std::min(io.DeltaTime, 0.1f);
    const float W = io.DisplaySize.x;
    const float H = io.DisplaySize.y;
    const float level = MicOcarina_GetLevel();
    const int note = MicOcarina_GetNote();

    if (level > 0.0f) {
        spawnAcc += dt * (12.0f + 38.0f * level);
        while (spawnAcc >= 1.0f) {
            spawnAcc -= 1.0f;
            if (streaks.size() >= 96) {
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
            streaks.push_back(s);
        }
    } else {
        spawnAcc = 0.0f;
    }

    if (streaks.empty()) {
        return;
    }

    for (auto& s : streaks) {
        s.x += s.speed * dt;
    }
    streaks.erase(std::remove_if(streaks.begin(), streaks.end(), [](const Streak& s) { return s.x - s.len > 1.0f; }),
                  streaks.end());

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const float thick = std::max(2.0f, 2.0f * ImGui::GetFontSize() / 13.0f);

    for (const auto& s : streaks) {
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
