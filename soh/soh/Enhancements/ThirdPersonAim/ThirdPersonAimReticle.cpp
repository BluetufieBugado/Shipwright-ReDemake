// Retículo da mira em terceira pessoa: anel com um ponto no centro da tela.
// Com a câmera por cima do ombro, o centro da tela é exatamente a direção para onde o Link mira.
#include "ThirdPersonAimReticle.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>

namespace {
// 0 = invisível, 1 = totalmente visível. Segue a mira com um fade curto, parecido com o zoom da câmera.
float gFade = 0.0f;
// Últimos valores mostrados: o texto continua visível (e some junto com o anel) quando a mira acaba.
int gAmmo = -1;
int gScore = -1;
int gScoreKind = 0;

// Texto com contorno escuro, para continuar legível em qualquer fundo.
void DrawOutlinedText(ImDrawList* dl, float size, ImVec2 pos, ImU32 color, ImU32 shadow, const char* text) {
    ImFont* font = ImGui::GetFont();
    const float o = std::max(1.0f, size * 0.07f);
    dl->AddText(font, size, ImVec2(pos.x - o, pos.y), shadow, text);
    dl->AddText(font, size, ImVec2(pos.x + o, pos.y), shadow, text);
    dl->AddText(font, size, ImVec2(pos.x, pos.y - o), shadow, text);
    dl->AddText(font, size, ImVec2(pos.x, pos.y + o), shadow, text);
    dl->AddText(font, size, pos, color, text);
}
} // namespace

void ThirdPersonAimReticle_Draw(bool aiming, float sizePercent, float opacity, int ammo, int score, int scoreKind) {
    ImGuiIO& io = ImGui::GetIO();
    const float dt = std::min(io.DeltaTime, 0.1f);

    if (aiming) {
        gAmmo = ammo;
        gScore = score;
        gScoreKind = scoreKind;
    }

    const float target = aiming ? 1.0f : 0.0f;
    gFade += (target - gFade) * std::min(1.0f, dt * 14.0f);
    if (gFade < 0.01f) {
        gFade = 0.0f;
        return;
    }

    const float alpha = std::min(1.0f, std::max(0.0f, opacity)) * gFade;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    const ImVec2 c(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f);

    const float H = vp->Size.y;
    const float r = std::max(4.0f, H * sizePercent * 0.01f);
    const float t = std::max(1.5f, H * 0.002f); // espessura do anel

    const ImU32 shadow = IM_COL32(0, 0, 0, static_cast<int>(alpha * 190.0f));
    const ImU32 white = IM_COL32(255, 255, 255, static_cast<int>(alpha * 255.0f));

    // Contorno escuro por baixo, para o retículo continuar visível em fundos claros.
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    dl->AddCircle(c, r, shadow, 48, t * 2.4f);
    dl->AddCircle(c, r, white, 48, t);
    dl->AddCircleFilled(c, t * 1.9f, shadow, 16);
    dl->AddCircleFilled(c, t * 1.2f, white, 16);

    // Munição ao lado do anel (e o contador do minigame logo abaixo).
    if (gAmmo >= 0) {
        char buf[32];
        const float size = std::max(12.0f, H * 0.034f);
        const float x = c.x + r + size * 0.6f;
        const ImU32 ammoColor = (gAmmo == 0) ? IM_COL32(255, 90, 90, static_cast<int>(alpha * 255.0f)) : white;

        std::snprintf(buf, sizeof(buf), "%d", gAmmo);
        DrawOutlinedText(dl, size, ImVec2(x, c.y - size * 0.5f), ammoColor, shadow, buf);

        if (gScore >= 0 && gScoreKind != 0) {
            const float small = size * 0.6f;
            std::snprintf(buf, sizeof(buf), "%s %d", (gScoreKind == 1) ? "Acertos" : "Pontos", gScore);
            DrawOutlinedText(dl, small, ImVec2(x, c.y + size * 0.55f), white, shadow, buf);
        }
    }
}
