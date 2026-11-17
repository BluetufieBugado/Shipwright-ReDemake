// Faixa de itens do carrossel (estilo remake): fundo escurecido, itens em linha com o selecionado em destaque
// e a quantidade embaixo. Desenha com a draw list de primeiro plano, igual ao OcarinaSongMenu.
#include "RemakeControls.h"

#include <ship/Context.h>
#include <fast/Fast3dGui.h>
#include <imgui.h>
#include <cmath>
#include <cstdio>
#include <memory>

extern "C" {
#include "z64.h"
#include "macros.h"
#include "variables.h"
#include "functions.h"
}

namespace {

float sFade = 0.0f;         // 0 = invisível, 1 = totalmente visível
float sScroll = 0.0f;       // posição (em itens) que a faixa está mostrando, com animação
bool sScrollValid = false;  // false = a próxima abertura começa já no item atual, sem deslizar

std::shared_ptr<Fast::Fast3dGui> GetFastGui() {
    return std::dynamic_pointer_cast<Fast::Fast3dGui>(Ship::Context::GetRawInstance()->GetWindow()->GetGui());
}

// Registra o ícone do item no ImGui na primeira vez que ele é usado (mesma tabela do HUD: gItemIcons).
ImTextureID GetItemTexture(int item) {
    static bool sRegistered[256] = {};
    if (item < 0 || item >= ITEM_NONE) {
        return ImTextureID{};
    }
    auto gui = GetFastGui();
    if (gui == nullptr) {
        return ImTextureID{};
    }
    char name[32];
    snprintf(name, sizeof(name), "RemakeItem_%d", item);
    if (!sRegistered[item]) {
        const char* path = (const char*)gItemIcons[item];
        if (path == nullptr) {
            return ImTextureID{};
        }
        gui->LoadGuiTexture(name, path, "", ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        sRegistered[item] = true;
    }
    return gui->GetTextureByName(name);
}

// Mesma regra do HUD original (Interface_DrawAmmoCount): quais itens mostram quantidade e quando é "cheio".
bool GetAmmoInfo(int item, int* outAmmo, bool* outFull) {
    int i = item;
    if (i >= ITEM_BOW_ARROW_FIRE && i <= ITEM_BOW_ARROW_LIGHT) {
        i = ITEM_BOW; // flechas mágicas mostram a quantidade do arco
    }
    if (!(i == ITEM_STICK || i == ITEM_NUT || i == ITEM_BOMB || i == ITEM_BOW || i == ITEM_SLINGSHOT ||
          i == ITEM_BOMBCHU || i == ITEM_BEAN)) {
        return false;
    }
    const int ammo = AMMO(i);
    *outAmmo = ammo;
    *outFull = (i == ITEM_BOW && ammo == CUR_CAPACITY(UPG_QUIVER)) ||
               (i == ITEM_BOMB && ammo == CUR_CAPACITY(UPG_BOMB_BAG)) ||
               (i == ITEM_SLINGSHOT && ammo == CUR_CAPACITY(UPG_BULLET_BAG)) ||
               (i == ITEM_STICK && ammo == CUR_CAPACITY(UPG_STICKS)) ||
               (i == ITEM_NUT && ammo == CUR_CAPACITY(UPG_NUTS)) || (i == ITEM_BOMBCHU && ammo == 50) ||
               (i == ITEM_BEAN && ammo == 15);
    return true;
}

} // namespace

void RemakeControlsOverlay_Draw() {
    const ImGuiIO& io = ImGui::GetIO();
    const float dt = fminf(io.DeltaTime, 0.1f);

    // Fade de entrada e saída (suavizado, independe da velocidade do jogo, que está parado)
    const bool open = RemakeControls_IsCarouselOpen();
    sFade += ((open ? 1.0f : 0.0f) - sFade) * fminf(1.0f, dt * 18.0f);
    if (!open && sFade < 0.02f) {
        sFade = 0.0f;
        sScrollValid = false;
        return;
    }

    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const float W = vp->Size.x;
    const float H = vp->Size.y;

    // Fundo escurecido (o desfoque vem numa etapa própria)
    dl->AddRectFilled(vp->Pos, ImVec2(vp->Pos.x + W, vp->Pos.y + H), IM_COL32(0, 0, 0, (int)(150.0f * sFade)));

    // Itens do inventário que servem para a idade atual, na ordem dos slots
    int slots[ARRAY_COUNT(gSaveContext.inventory.items)];
    int count = 0;
    int sel = -1;
    const int equipped = gSaveContext.equips.cButtonSlots[0];
    for (int s = 0; s < ARRAY_COUNT(gSaveContext.inventory.items); s++) {
        if (!RemakeControls_IsSlotUsable(s)) {
            continue;
        }
        if (s == equipped) {
            sel = count;
        }
        slots[count++] = s;
    }
    if (count == 0 || sel < 0) {
        return;
    }

    // Desliza até o item selecionado pelo caminho mais curto (a lista dá a volta, como o L/R)
    if (!sScrollValid) {
        sScroll = (float)sel;
        sScrollValid = true;
    }
    float delta = (float)sel - sScroll;
    const float halfCount = 0.5f * (float)count;
    while (delta > halfCount) {
        delta -= (float)count;
    }
    while (delta < -halfCount) {
        delta += (float)count;
    }
    sScroll += delta * fminf(1.0f, dt * 16.0f);
    if (sScroll < 0.0f) {
        sScroll += (float)count;
    }
    if (sScroll >= (float)count) {
        sScroll -= (float)count;
    }

    const float cx = vp->Pos.x + W * 0.5f;
    const float cy = vp->Pos.y + H * 0.5f;
    const float iconBase = fminf(W, H) * 0.085f;
    const float spacing = iconBase * 1.45f;

    auto drawItem = [&](int pos) {
        const int idx = ((pos % count) + count) % count;
        const int item = gSaveContext.inventory.items[slots[idx]];
        const float d = fabsf((float)pos - sScroll);
        const float focus = fmaxf(0.0f, 1.0f - d); // 1 = no centro, 0 = a um item ou mais de distância
        float alpha = 0.40f + 0.60f * focus;
        if (d > 2.0f) {
            alpha *= fmaxf(0.0f, 3.0f - d); // some nas pontas da faixa
        }
        alpha *= sFade;
        if (alpha <= 0.01f) {
            return;
        }

        const float size = iconBase * (1.0f + 0.6f * focus);
        const ImVec2 c(cx + ((float)pos - sScroll) * spacing, cy);

        // Disco escuro atrás do ícone; o selecionado ganha um anel branco
        dl->AddCircleFilled(c, size * 0.62f, IM_COL32(18, 22, 36, (int)(200.0f * alpha)), 40);
        if (focus > 0.01f) {
            dl->AddCircle(c, size * 0.66f, IM_COL32(255, 255, 255, (int)(255.0f * focus * sFade)), 48, 3.0f);
        }

        ImTextureID tex = GetItemTexture(item);
        if (tex) {
            dl->AddImage(tex, ImVec2(c.x - size * 0.5f, c.y - size * 0.5f),
                         ImVec2(c.x + size * 0.5f, c.y + size * 0.5f), ImVec2(0, 0), ImVec2(1, 1),
                         IM_COL32(255, 255, 255, (int)(255.0f * alpha)));
        }

        // Quantidade embaixo do ícone
        int ammo = 0;
        bool full = false;
        if (GetAmmoInfo(item, &ammo, &full)) {
            char buf[8];
            snprintf(buf, sizeof(buf), "%d", ammo);
            const float baseFs = ImGui::GetFontSize();
            const float fs = baseFs * (1.1f + 0.7f * focus);
            const float tw = ImGui::CalcTextSize(buf).x * fs / baseFs;
            const ImVec2 p(c.x - tw * 0.5f, c.y + size * 0.62f + 4.0f);
            const int a = (int)(255.0f * alpha);
            ImU32 col = IM_COL32(255, 255, 255, a);
            if (ammo == 0) {
                col = IM_COL32(255, 90, 90, a);
            } else if (full) {
                col = IM_COL32(120, 255, 120, a);
            }
            dl->AddText(ImGui::GetFont(), fs, ImVec2(p.x + 1.0f, p.y + 1.0f), IM_COL32(0, 0, 0, a), buf);
            dl->AddText(ImGui::GetFont(), fs, p, col, buf);
        }
    };

    // Até 7 itens visíveis (menos se o inventário tiver menos); o do centro é desenhado por último
    const int base = (int)roundf(sScroll);
    int lo = -3;
    int hi = 3;
    if (count < 7) {
        lo = -((count - 1) / 2);
        hi = lo + count - 1;
    }
    for (int k = lo; k <= hi; k++) {
        if (fabsf((float)(base + k) - sScroll) >= 0.5f) {
            drawItem(base + k);
        }
    }
    for (int k = lo; k <= hi; k++) {
        if (fabsf((float)(base + k) - sScroll) < 0.5f) {
            drawItem(base + k);
        }
    }
}
