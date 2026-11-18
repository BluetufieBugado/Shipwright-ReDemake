// Faixa de itens do carrossel (estilo remake): fundo escurecido, slots quadrados em linha com o selecionado
// maior e dourado, setas, nome do item, barra de posição e dica "Select". Desenha com a draw list de primeiro plano,
// igual ao OcarinaSongMenu.
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
extern PlayState* gPlayState;
}

namespace {

float sFade = 0.0f;         // 0 = invisível, 1 = totalmente visível
float sScroll = 0.0f;       // posição (em itens) que a faixa está mostrando, com animação
bool sScrollValid = false;  // false = a próxima abertura começa já no item atual, sem deslizar
bool sLastSortByType = false; // para detectar a troca de ordenação e não deslizar pela faixa embaralhada

std::shared_ptr<Fast::Fast3dGui> GetFastGui() {
    return std::dynamic_pointer_cast<Fast::Fast3dGui>(Ship::Context::GetRawInstance()->GetWindow()->GetGui());
}

// Registra o ícone do item no ImGui na primeira vez que ele é usado (mesma tabela do HUD: gItemIcons).
ImTextureID GetItemTexture(int item) {
    static bool sRegistered[256] = {};
    if (item < 0 || item >= ITEM_NONE) {
        return nullptr;
    }
    auto gui = GetFastGui();
    if (gui == nullptr) {
        return nullptr;
    }
    char name[32];
    snprintf(name, sizeof(name), "RemakeItem_%d", item);
    if (!sRegistered[item]) {
        const char* path = (const char*)gItemIcons[item];
        if (path == nullptr) {
            return nullptr;
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

// Nome do item, pela numeração dos itens do inventário do jogo (os mesmos valores dos ITEM_*).
const char* GetItemName(int item) {
    switch (item) {
        case 0x00: return "Deku Stick";
        case 0x01: return "Deku Nut";
        case 0x02: return "Bomb";
        case 0x03: return "Fairy Bow";
        case 0x04: return "Fire Arrow";
        case 0x05: return "Din's Fire";
        case 0x06: return "Fairy Slingshot";
        case 0x07: return "Fairy Ocarina";
        case 0x08: return "Ocarina of Time";
        case 0x09: return "Bombchu";
        case 0x0A: return "Hookshot";
        case 0x0B: return "Longshot";
        case 0x0C: return "Ice Arrow";
        case 0x0D: return "Farore's Wind";
        case 0x0E: return "Boomerang";
        case 0x0F: return "Lens of Truth";
        case 0x10: return "Magic Beans";
        case 0x11: return "Megaton Hammer";
        case 0x12: return "Light Arrow";
        case 0x13: return "Nayru's Love";
        case 0x14: return "Empty Bottle";
        case 0x15: return "Red Potion";
        case 0x16: return "Green Potion";
        case 0x17: return "Blue Potion";
        case 0x18: return "Fairy";
        case 0x19: return "Fish";
        case 0x1A: return "Lon Lon Milk";
        case 0x1B: return "Ruto's Letter";
        case 0x1C: return "Blue Fire";
        case 0x1D: return "Bug";
        case 0x1E: return "Big Poe";
        case 0x1F: return "Lon Lon Milk (Half)";
        case 0x20: return "Poe";
        case 0x21: return "Weird Egg";
        case 0x22: return "Cucco";
        case 0x23: return "Zelda's Letter";
        case 0x24: return "Keaton Mask";
        case 0x25: return "Skull Mask";
        case 0x26: return "Spooky Mask";
        case 0x27: return "Bunny Hood";
        case 0x28: return "Goron Mask";
        case 0x29: return "Zora Mask";
        case 0x2A: return "Gerudo Mask";
        case 0x2B: return "Mask of Truth";
        case 0x2C: return "SOLD OUT";
        case 0x2D: return "Pocket Egg";
        case 0x2E: return "Pocket Cucco";
        case 0x2F: return "Cojiro";
        case 0x30: return "Odd Mushroom";
        case 0x31: return "Odd Potion";
        case 0x32: return "Poacher's Saw";
        case 0x33: return "Broken Goron's Sword";
        case 0x34: return "Prescription";
        case 0x35: return "Eyeball Frog";
        case 0x36: return "Eye Drops";
        case 0x37: return "Claim Check";
        default: return "";
    }
}

float TextWidth(const char* text, float fs) {
    return ImGui::CalcTextSize(text).x * fs / ImGui::GetFontSize();
}

// Texto centralizado em 'center' (horizontal e vertical), com sombra escura.
void DrawTextCentered(ImDrawList* dl, const char* text, ImVec2 center, float fs, ImU32 col) {
    const ImVec2 p(center.x - TextWidth(text, fs) * 0.5f, center.y - fs * 0.5f);
    const ImU32 shadow = IM_COL32(0, 0, 0, (col >> IM_COL32_A_SHIFT) & 0xFF);
    const float off = fmaxf(1.0f, fs * 0.05f);
    dl->AddText(ImGui::GetFont(), fs, ImVec2(p.x + off, p.y + off), shadow, text);
    dl->AddText(ImGui::GetFont(), fs, p, col, text);
}

// Item em foco no canto superior direito (estilo remake): quadrado escuro com o ícone, quantidade no canto
// inferior direito, etiqueta "R" pequena em cima e o rótulo "ZR" embaixo.
// Segue a opacidade do C-esquerdo do HUD original (some e escurece junto com o HUD).
void DrawCornerItem() {
    // Ajuste fino (frações da altura da tela), medido no print do remake (1148x645)
    const float kSize = 0.112f;        // lado do quadrado
    const float kMarginRight = 0.080f; // distância da borda direita
    const float kTop = 0.062f;         // distância do topo até o quadrado

    if (!RemakeControls_IsEnabled() || gPlayState == nullptr || RemakeControls_IsCornerHidden()) {
        return;
    }
    if (gSaveContext.gameMode != GAMEMODE_NORMAL) {
        return;
    }
    // Menu de pausa (e o de debug) aberto: o item do canto some, senão cobre as rupias.
    // O carrossel não usa o pauseCtx (é só o isPaused do Play_Update), então não é afetado.
    if (gPlayState->pauseCtx.state != 0 || gPlayState->pauseCtx.debugState != 0) {
        return;
    }
    const int item = gSaveContext.equips.buttonItems[1];
    if (item >= 0xF0) {
        return; // nenhum item em foco
    }
    const float alpha = (float)gPlayState->interfaceCtx.cLeftAlpha / 255.0f;
    if (alpha <= 0.01f) {
        return;
    }

    ImGuiViewport* vp = ImGui::GetMainViewport();
    // Camada de FUNDO do ImGui: fica por cima do jogo, mas atrás do menu do SoH (como o HUD de verdade)
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const float W = vp->Size.x;
    const float H = vp->Size.y;
    const float size = H * kSize;
    const ImVec2 mn(vp->Pos.x + W - H * kMarginRight - size, vp->Pos.y + H * kTop);
    const ImVec2 mx(mn.x + size, mn.y + size);
    const ImVec2 c((mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f);
    const float round = size * 0.04f; // quase quadrado, como no remake

    // Quadrado quase preto com borda fina clara
    dl->AddRectFilled(mn, mx, IM_COL32(8, 8, 8, (int)(225.0f * alpha)), round);
    dl->AddRect(mn, mx, IM_COL32(200, 200, 200, (int)(180.0f * alpha)), round, 0, fmaxf(1.0f, size * 0.02f));

    // Ícone do item (grande, quase do tamanho do quadrado)
    ImTextureID tex = GetItemTexture(item);
    if (tex) {
        const float is = size * 0.92f;
        dl->AddImage(tex, ImVec2(c.x - is * 0.5f, c.y - is * 0.5f), ImVec2(c.x + is * 0.5f, c.y + is * 0.5f),
                     ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, (int)(255.0f * alpha)));
    }

    // Quantidade no canto inferior direito (número grande)
    int ammo = 0;
    bool full = false;
    if (GetAmmoInfo(item, &ammo, &full)) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", ammo);
        const float fs = size * 0.40f;
        const int a = (int)(255.0f * alpha);
        ImU32 col = IM_COL32(255, 255, 255, a);
        if (ammo == 0) {
            col = IM_COL32(255, 90, 90, a);
        } else if (full) {
            col = IM_COL32(120, 255, 120, a);
        }
        const float tw = TextWidth(buf, fs);
        DrawTextCentered(dl, buf, ImVec2(mx.x - size * 0.10f - tw * 0.5f, mx.y - size * 0.08f - fs * 0.5f), fs, col);
    }

    // Etiqueta "R" pequena, ACIMA do quadrado (não encosta na borda)
    {
        const float bw = size * 0.24f;
        const float bh = size * 0.18f;
        const float cy = mn.y - size * 0.08f - bh * 0.5f;
        const ImVec2 bmn(c.x - bw * 0.5f, cy - bh * 0.5f);
        const ImVec2 bmx(c.x + bw * 0.5f, cy + bh * 0.5f);
        dl->AddRectFilled(bmn, bmx, IM_COL32(225, 225, 225, (int)(235.0f * alpha)), bh * 0.3f);
        DrawTextCentered(dl, "R", ImVec2(c.x, cy), bh * 0.85f, IM_COL32(30, 30, 30, (int)(255.0f * alpha)));
    }

    // Rótulo "ZR" em uma caixinha escura com borda, ABAIXO do quadrado
    {
        const float bw = size * 0.28f;
        const float bh = size * 0.24f;
        const float cy = mx.y + size * 0.10f + bh * 0.5f;
        const ImVec2 bmn(c.x - bw * 0.5f, cy - bh * 0.5f);
        const ImVec2 bmx(c.x + bw * 0.5f, cy + bh * 0.5f);
        dl->AddRectFilled(bmn, bmx, IM_COL32(12, 12, 12, (int)(220.0f * alpha)), bh * 0.2f);
        dl->AddRect(bmn, bmx, IM_COL32(200, 200, 200, (int)(180.0f * alpha)), bh * 0.2f, 0, 1.0f);
        DrawTextCentered(dl, "ZR", ImVec2(c.x, cy), bh * 0.62f, IM_COL32(235, 235, 235, (int)(255.0f * alpha)));
    }
}

} // namespace

void RemakeControlsOverlay_Draw() {
    DrawCornerItem(); // desenhado antes do carrossel: o escurecimento do carrossel passa por cima
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
    dl->AddRectFilled(vp->Pos, ImVec2(vp->Pos.x + W, vp->Pos.y + H), IM_COL32(0, 0, 0, (int)(165.0f * sFade)));

    // Itens do inventário que servem para a idade atual, na ordem escolhida (slots ou por tipo)
    int slots[ARRAY_COUNT(gSaveContext.inventory.items)];
    const int count = RemakeControls_GetCarouselSlots(slots, (int)ARRAY_COUNT(slots));
    int sel = -1;
    const int equipped = gSaveContext.equips.cButtonSlots[0];
    for (int i = 0; i < count; i++) {
        if (slots[i] == equipped) {
            sel = i;
            break;
        }
    }
    if (count == 0 || sel < 0) {
        return;
    }

    // Trocou a ordenação: a faixa pula direto para a nova posição do item, sem deslizar
    const bool sortByType = RemakeControls_IsSortByType();
    if (sortByType != sLastSortByType) {
        sLastSortByType = sortByType;
        sScrollValid = false;
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

    // Medidas proporcionais à altura da tela (valores tirados da imagem de referência, 1080p)
    const float cx = vp->Pos.x + W * 0.5f;
    const float cy = vp->Pos.y + H * 0.47f;
    const float slotBase = H * 0.134f; // lado do slot comum
    const float spacing = H * 0.213f;  // distância entre os centros dos slots

    auto drawSlot = [&](int pos) {
        const int idx = ((pos % count) + count) % count;
        const int item = gSaveContext.inventory.items[slots[idx]];
        const float d = fabsf((float)pos - sScroll);
        const float focus = fmaxf(0.0f, 1.0f - d); // 1 = no centro, 0 = a um item ou mais de distância
        float alpha = 0.55f + 0.45f * focus;
        if (d > 2.0f) {
            alpha *= fmaxf(0.0f, 3.0f - d); // some nas pontas da faixa
        }
        alpha *= sFade;
        if (alpha <= 0.01f) {
            return;
        }

        const float size = slotBase * (1.0f + 0.12f * focus);
        const float half = size * 0.5f;
        const ImVec2 c(cx + ((float)pos - sScroll) * spacing, cy);
        const ImVec2 mn(c.x - half, c.y - half);
        const ImVec2 mx(c.x + half, c.y + half);
        const float round = size * 0.05f;

        // Preenchimento: marrom escuro no slot comum, dourado claro no selecionado
        const int fr = (int)(46.0f + 130.0f * focus);
        const int fg = (int)(36.0f + 102.0f * focus);
        const int fb = (int)(24.0f + 42.0f * focus);
        dl->AddRectFilled(mn, mx, IM_COL32(fr, fg, fb, (int)(235.0f * alpha)), round);

        // Brilho em volta do selecionado
        if (focus > 0.01f) {
            for (int g = 3; g >= 1; g--) {
                const float o = (float)g * size * 0.025f;
                dl->AddRect(ImVec2(mn.x - o, mn.y - o), ImVec2(mx.x + o, mx.y + o),
                            IM_COL32(255, 255, 255, (int)(34.0f * focus * sFade)), round + o, 0, 2.0f);
            }
        }

        // Moldura: borda escura, filete interno e cantos ornamentados
        const float inset = size * 0.04f;
        dl->AddRect(mn, mx, IM_COL32(18, 13, 8, (int)(255.0f * alpha)), round, 0, 2.0f);
        dl->AddRect(ImVec2(mn.x + inset, mn.y + inset), ImVec2(mx.x - inset, mx.y - inset),
                    IM_COL32(96, 78, 50, (int)(170.0f * alpha)), round, 0, 1.0f);
        const float cs = size * 0.07f;
        const ImU32 cornerCol = IM_COL32(120, 98, 62, (int)(220.0f * alpha));
        dl->AddRectFilled(ImVec2(mn.x + inset, mn.y + inset), ImVec2(mn.x + inset + cs, mn.y + inset + cs), cornerCol);
        dl->AddRectFilled(ImVec2(mx.x - inset - cs, mn.y + inset), ImVec2(mx.x - inset, mn.y + inset + cs), cornerCol);
        dl->AddRectFilled(ImVec2(mn.x + inset, mx.y - inset - cs), ImVec2(mn.x + inset + cs, mx.y - inset), cornerCol);
        dl->AddRectFilled(ImVec2(mx.x - inset - cs, mx.y - inset - cs), ImVec2(mx.x - inset, mx.y - inset), cornerCol);
        if (focus > 0.01f) {
            dl->AddRect(mn, mx, IM_COL32(255, 255, 255, (int)(255.0f * focus * sFade)), round, 0, 3.0f);
        }

        // Ícone do item
        ImTextureID tex = GetItemTexture(item);
        if (tex) {
            const float is = size * 0.74f;
            dl->AddImage(tex, ImVec2(c.x - is * 0.5f, c.y - is * 0.5f), ImVec2(c.x + is * 0.5f, c.y + is * 0.5f),
                         ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, (int)(255.0f * alpha)));
        }

        // Quantidade no canto inferior direito do slot
        int ammo = 0;
        bool full = false;
        if (GetAmmoInfo(item, &ammo, &full)) {
            char buf[8];
            snprintf(buf, sizeof(buf), "%d", ammo);
            const float fs = size * 0.27f;
            const int a = (int)(255.0f * alpha);
            ImU32 col = IM_COL32(255, 255, 255, a);
            if (ammo == 0) {
                col = IM_COL32(255, 90, 90, a);
            } else if (full) {
                col = IM_COL32(120, 255, 120, a);
            }
            const float tw = TextWidth(buf, fs);
            DrawTextCentered(dl, buf, ImVec2(mx.x - size * 0.07f - tw * 0.5f, mx.y - size * 0.07f - fs * 0.5f), fs, col);
        }
    };

    // Até 7 slots visíveis (menos se o inventário tiver menos); o do centro é desenhado por último
    const int base = (int)roundf(sScroll);
    int lo = -3;
    int hi = 3;
    if (count < 7) {
        lo = -((count - 1) / 2);
        hi = lo + count - 1;
    }
    for (int k = lo; k <= hi; k++) {
        if (fabsf((float)(base + k) - sScroll) >= 0.5f) {
            drawSlot(base + k);
        }
    }
    for (int k = lo; k <= hi; k++) {
        if (fabsf((float)(base + k) - sScroll) < 0.5f) {
            drawSlot(base + k);
        }
    }

    const int fadeA = (int)(255.0f * sFade);

    // Setas dos dois lados do slot selecionado
    {
        const float ax = slotBase * 0.80f;
        const float aw = slotBase * 0.08f;
        const float ah = slotBase * 0.20f;
        const ImU32 col = IM_COL32(255, 255, 255, fadeA);
        dl->AddTriangleFilled(ImVec2(cx - ax - aw, cy), ImVec2(cx - ax, cy - ah * 0.5f), ImVec2(cx - ax, cy + ah * 0.5f),
                              col);
        dl->AddTriangleFilled(ImVec2(cx + ax + aw, cy), ImVec2(cx + ax, cy - ah * 0.5f), ImVec2(cx + ax, cy + ah * 0.5f),
                              col);
    }

    // Nome do item selecionado
    const char* name = GetItemName(gSaveContext.inventory.items[slots[sel]]);
    if (name[0] != '\0') {
        DrawTextCentered(dl, name, ImVec2(cx, cy + H * 0.206f), H * 0.052f, IM_COL32(255, 255, 255, fadeA));
    }

    // Barra de posição: trilho com um marcador que anda conforme o item selecionado
    {
        const float bw = H * 0.14f;
        const float bh = H * 0.008f;
        const float by = cy + H * 0.262f;
        const float left = cx - bw * 0.5f;
        dl->AddRectFilled(ImVec2(left, by - bh * 0.5f), ImVec2(left + bw, by + bh * 0.5f),
                          IM_COL32(255, 255, 255, (int)(45.0f * sFade)), bh * 0.5f);
        const float tw = bw * 0.2f;
        float t = 0.5f;
        if (count > 1) {
            t = fminf(1.0f, fmaxf(0.0f, sScroll / (float)(count - 1)));
        }
        const float tx = left + t * (bw - tw);
        dl->AddRectFilled(ImVec2(tx, by - bh * 0.5f), ImVec2(tx + tw, by + bh * 0.5f), IM_COL32(240, 200, 110, fadeA),
                          bh * 0.5f);
    }

    // Dica "Select" com as teclas L e R
    {
        const char* label = "Select";
        const float fs = H * 0.028f;
        const float r = H * 0.014f;
        const float gap = H * 0.012f;
        const float between = H * 0.006f;
        const float tw = TextWidth(label, fs);
        const float total = tw + gap + 4.0f * r + between;
        const float y = cy + H * 0.308f;
        const float startX = cx - total * 0.5f;
        DrawTextCentered(dl, label, ImVec2(startX + tw * 0.5f, y), fs, IM_COL32(255, 255, 255, fadeA));
        const char* keys[2] = { "L", "R" };
        for (int i = 0; i < 2; i++) {
            const ImVec2 kc(startX + tw + gap + r + (float)i * (2.0f * r + between), y);
            dl->AddCircleFilled(kc, r, IM_COL32(255, 255, 255, (int)(235.0f * sFade)), 24);
            DrawTextCentered(dl, keys[i], kc, r * 1.45f, IM_COL32(30, 30, 30, fadeA));
        }
    }

    // Ordenação: pontinhos (um por modo, o atual aceso), nome do modo e dica "Sort" com a tecla Y.
    // Posição e tamanhos são frações de H, fáceis de ajustar aqui.
    {
        const char* modeNames[2] = { "Default", "By Type" };
        const char* label = modeNames[sortByType ? 1 : 0];
        const char* hint = "Sort";
        const float fs = H * 0.028f;
        const float r = H * 0.014f;
        const float dotR = H * 0.0045f;
        const float dotGap = H * 0.010f;
        const float gap = H * 0.012f;
        const float bigGap = H * 0.030f;
        const float y = cy + H * 0.352f;
        // Largura fixa para o nome do modo: a linha não anda quando o texto muda
        const float labelW = fmaxf(TextWidth(modeNames[0], fs), TextWidth(modeNames[1], fs));
        const float hintW = TextWidth(hint, fs);
        const float dotsW = 4.0f * dotR + dotGap;
        const float total = dotsW + gap + labelW + bigGap + hintW + gap + 2.0f * r;
        float x = cx - total * 0.5f;

        for (int i = 0; i < 2; i++) {
            const bool on = (i == (sortByType ? 1 : 0));
            dl->AddCircleFilled(ImVec2(x + dotR + (float)i * (2.0f * dotR + dotGap), y), dotR,
                                IM_COL32(255, 255, 255, (int)((on ? 255.0f : 70.0f) * sFade)), 12);
        }
        x += dotsW + gap;
        DrawTextCentered(dl, label, ImVec2(x + labelW * 0.5f, y), fs, IM_COL32(255, 255, 255, fadeA));
        x += labelW + bigGap;
        DrawTextCentered(dl, hint, ImVec2(x + hintW * 0.5f, y), fs, IM_COL32(255, 255, 255, fadeA));
        x += hintW + gap;
        const ImVec2 kc(x + r, y);
        dl->AddCircleFilled(kc, r, IM_COL32(255, 255, 255, (int)(235.0f * sFade)), 24);
        DrawTextCentered(dl, "Y", kc, r * 1.45f, IM_COL32(30, 30, 30, fadeA));
    }
}
