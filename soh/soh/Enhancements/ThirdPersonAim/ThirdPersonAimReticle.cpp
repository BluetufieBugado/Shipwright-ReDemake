// Retículo da mira em terceira pessoa: anel com um ponto no centro da tela.
// Com a câmera por cima do ombro, o centro da tela é exatamente a direção para onde o Link mira.
#include "ThirdPersonAimReticle.h"

#include <imgui.h>

#include <algorithm>

namespace {
// 0 = invisível, 1 = totalmente visível. Segue a mira com um fade curto, parecido com o zoom da câmera.
float gFade = 0.0f;
} // namespace

void ThirdPersonAimReticle_Draw(bool aiming, float sizePercent, float opacity) {
    ImGuiIO& io = ImGui::GetIO();
    const float dt = std::min(io.DeltaTime, 0.1f);

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
}
