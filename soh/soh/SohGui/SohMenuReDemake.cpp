#include "SohMenu.h"

#include <imgui.h>
#include <cmath>
#include <libultraship/bridge/consolevariablebridge.h>

#include "soh/cvar_prefixes.h"
#include "soh/SohGui/UIWidgets.hpp"
#include "soh/Enhancements/MicOcarina/MicOcarina.h"

#include <cstdint>
#include <memory>

#ifndef __WIIU__
#include <ship/Context.h>
#include <ship/controller/controldeck/ControlDeck.h>
#include <ship/controller/controldevice/controller/mapping/sdl/SDLButtonToButtonMapping.h>
#include <ship/controller/controldevice/controller/mapping/sdl/SDLAxisDirectionToButtonMapping.h>
#include <ship/controller/controldevice/controller/mapping/keyboard/KeyboardKeyToButtonMapping.h>
#include <ship/controller/controldevice/controller/mapping/mouse/MouseButtonToButtonMapping.h>
#include <fast/Fast3dWindow.h>
#endif

// BTN_A, BTN_CUP etc. (mesmo include usado pelo SohInputEditorWindow.cpp)
extern "C" {
#include "z64.h"
}

namespace SohGui {
extern std::shared_ptr<SohMenu> mSohMenu;

using namespace UIWidgets;

// Mesmos CVars que já existiam (code_800EC960.c e o overlay continuam lendo estes nomes).
#define CVAR_MIC(name) CVAR_SETTING("CustomOcarina." name)

static bool MicDisabled() {
    return !CVarGetInteger(CVAR_MIC("Microphone"), 0);
}

static bool ModernControlsDisabled() {
    return !CVarGetInteger(CVAR_ENHANCEMENT("RemakeControls"), 1);
}

namespace {

// ---------------------------------------------------------------- Controls guide
// Desenho do controle (estilo N64 "moderno") + tabela explicando cada botão com o modo moderno ligado.
// Tudo desenhado com ImDrawList, sem imagens externas. Coordenadas de referência: 380 x 320.
constexpr float kGuideRefW = 380.0f;
constexpr float kGuideRefH = 320.0f;

struct GuideEntry {
    const char* button;
    const char* text;
    bool changed; // função nova com o modo moderno (anel ciano no desenho)
    bool planned; // ainda não implementado (texto apagado)
};

// Edite os textos aqui.
const GuideEntry kGuideEntries[] = {
    { "Z", "Aim at a target and raise the shield.", true, false },
    { "L", "Put the sword away.", false, false },
    { "R", "Open the map. (coming soon)", true, true },
    { "C-Up", "Use the focused item (shown in the top-right corner).", true, false },
    { "C-Left / C-Right",
      "Hold to open the item carousel (the game pauses). Press again to pick the previous / next item, release to "
      "close.",
      true, false },
    { "C-Down", "Jump. (coming soon)", true, true },
    { "D-Pad Up", "Call Navi.", true, false },
    { "D-Pad Left / Right / Down", "Free item slots. Choose the items in the pause menu.", true, false },
    { "B", "Attack. With the carousel open, switches between Default and By Type ordering.", true, false },
    { "A", "Context actions (talk, open, roll, ...).", false, false },
    { "Start", "Pause menu.", false, false },
    { "Control Stick", "Move Link.", false, false },
};

void FillRotatedEllipse(ImDrawList* dl, ImVec2 c, float rx, float ry, float rot, ImU32 col) {
    constexpr int kSegments = 48;
    ImVec2 pts[kSegments];
    const float cs = std::cos(rot);
    const float sn = std::sin(rot);
    for (int i = 0; i < kSegments; i++) {
        const float a = (2.0f * 3.14159265f * (float)i) / (float)kSegments;
        const float x = std::cos(a) * rx;
        const float y = std::sin(a) * ry;
        pts[i] = ImVec2(c.x + x * cs - y * sn, c.y + x * sn + y * cs);
    }
    dl->AddConvexPolyFilled(pts, kSegments, col);
}

void DrawControllerDiagram(ImDrawList* dl, ImVec2 origin, float s) {
    const ImU32 kBodyEdge = IM_COL32(95, 95, 100, 255);
    const ImU32 kBody = IM_COL32(150, 150, 155, 255);
    const ImU32 kDark = IM_COL32(62, 62, 68, 255);
    const ImU32 kMid = IM_COL32(100, 100, 106, 255);
    const ImU32 kWhite = IM_COL32(255, 255, 255, 255);
    const ImU32 kAccent = IM_COL32(0, 190, 255, 255);

    auto P = [&](float x, float y) { return ImVec2(origin.x + x * s, origin.y + y * s); };

    auto Label = [&](ImVec2 c, const char* text) {
        const ImVec2 ts = ImGui::CalcTextSize(text);
        dl->AddText(ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f), kWhite, text);
    };

    auto Round = [&](float cx, float cy, float r, ImU32 fill, const char* label, bool ringed) {
        dl->AddCircleFilled(P(cx, cy), r * s, fill);
        if (label != nullptr) {
            Label(P(cx, cy), label);
        }
        if (ringed) {
            dl->AddCircle(P(cx, cy), (r + 4.0f) * s, kAccent, 0, 2.5f);
        }
    };

    enum Dir { Up, Down, Left, Right };
    auto Arrow = [&](float cx, float cy, float r, Dir d, ImU32 col) {
        ImVec2 a, b, c;
        switch (d) {
            case Up:
                a = P(cx, cy - r);
                b = P(cx - r, cy + r * 0.7f);
                c = P(cx + r, cy + r * 0.7f);
                break;
            case Down:
                a = P(cx, cy + r);
                b = P(cx - r, cy - r * 0.7f);
                c = P(cx + r, cy - r * 0.7f);
                break;
            case Left:
                a = P(cx - r, cy);
                b = P(cx + r * 0.7f, cy - r);
                c = P(cx + r * 0.7f, cy + r);
                break;
            default:
                a = P(cx + r, cy);
                b = P(cx - r * 0.7f, cy - r);
                c = P(cx - r * 0.7f, cy + r);
                break;
        }
        dl->AddTriangleFilled(a, b, c, col);
    };

    // Corpo: contorno escuro primeiro, depois o preenchimento.
    const float edge = 3.0f;
    dl->AddRectFilled(P(26.0f - edge, 28.0f - edge), P(354.0f + edge, 230.0f + edge), kBodyEdge, 83.0f * s);
    FillRotatedEllipse(dl, P(78.0f, 215.0f), (50.0f + edge) * s, (88.0f + edge) * s, -0.2f, kBodyEdge);
    FillRotatedEllipse(dl, P(302.0f, 215.0f), (50.0f + edge) * s, (88.0f + edge) * s, 0.2f, kBodyEdge);
    dl->AddRectFilled(P(26.0f, 28.0f), P(354.0f, 230.0f), kBody, 80.0f * s);
    FillRotatedEllipse(dl, P(78.0f, 215.0f), 50.0f * s, 88.0f * s, -0.2f, kBody);
    FillRotatedEllipse(dl, P(302.0f, 215.0f), 50.0f * s, 88.0f * s, 0.2f, kBody);

    // Gatilhos: Z (atrás), L e R.
    dl->AddRectFilled(P(50.0f, 2.0f), P(112.0f, 20.0f), kDark, 6.0f * s);
    Label(P(81.0f, 11.0f), "Z");
    dl->AddRectFilled(P(62.0f, 18.0f), P(124.0f, 38.0f), kMid, 6.0f * s);
    Label(P(93.0f, 28.0f), "L");
    dl->AddRectFilled(P(256.0f, 18.0f), P(318.0f, 38.0f), kMid, 6.0f * s);
    Label(P(287.0f, 28.0f), "R");
    // Anéis dos que têm função nova (Z e R).
    dl->AddRect(P(50.0f - 3.0f, 2.0f - 3.0f), P(112.0f + 3.0f, 20.0f + 3.0f), kAccent, 8.0f * s, 0, 2.5f);
    dl->AddRect(P(256.0f - 3.0f, 18.0f - 3.0f), P(318.0f + 3.0f, 38.0f + 3.0f), kAccent, 8.0f * s, 0, 2.5f);

    // Três botões pequenos no topo (decorativos).
    Round(175.0f, 53.0f, 8.0f, kMid, nullptr, false);
    Round(205.0f, 53.0f, 8.0f, kMid, nullptr, false);
    Round(238.0f, 53.0f, 8.0f, kMid, nullptr, false);

    // Analógico.
    Round(95.0f, 98.0f, 34.0f, kDark, nullptr, false);
    Round(95.0f, 98.0f, 26.0f, IM_COL32(175, 175, 180, 255), nullptr, false);
    Round(95.0f, 98.0f, 17.0f, IM_COL32(205, 205, 210, 255), nullptr, false);

    // D-Pad.
    dl->AddRectFilled(P(134.0f, 149.0f), P(182.0f, 167.0f), kDark, 3.0f * s);
    dl->AddRectFilled(P(149.0f, 134.0f), P(167.0f, 182.0f), kDark, 3.0f * s);
    const ImU32 kDpadArrow = IM_COL32(150, 150, 156, 255);
    Arrow(158.0f, 141.0f, 4.0f, Up, kDpadArrow);
    Arrow(158.0f, 175.0f, 4.0f, Down, kDpadArrow);
    Arrow(142.0f, 158.0f, 4.0f, Left, kDpadArrow);
    Arrow(174.0f, 158.0f, 4.0f, Right, kDpadArrow);
    dl->AddCircle(P(158.0f, 158.0f), 33.0f * s, kAccent, 0, 2.5f);

    // Start, B e A.
    Round(208.0f, 92.0f, 15.0f, IM_COL32(215, 40, 40, 255), "S", false);
    Round(248.0f, 130.0f, 14.0f, IM_COL32(40, 150, 70, 255), "B", true);
    Round(285.0f, 160.0f, 15.0f, IM_COL32(50, 90, 185, 255), "A", false);

    // Botões C (amarelos, losango).
    const ImU32 kYellow = IM_COL32(240, 180, 40, 255);
    const ImU32 kArrowDark = IM_COL32(70, 50, 10, 255);
    Round(318.0f, 70.0f, 12.0f, kYellow, nullptr, true);
    Round(292.0f, 96.0f, 12.0f, kYellow, nullptr, true);
    Round(343.0f, 96.0f, 12.0f, kYellow, nullptr, true);
    Round(318.0f, 122.0f, 12.0f, kYellow, nullptr, true);
    Arrow(318.0f, 70.0f, 6.0f, Up, kArrowDark);
    Arrow(292.0f, 96.0f, 6.0f, Left, kArrowDark);
    Arrow(343.0f, 96.0f, 6.0f, Right, kArrowDark);
    Arrow(318.0f, 122.0f, 6.0f, Down, kArrowDark);
}

void DrawControlsGuide() {
    const ImVec4 accent = ImGui::ColorConvertU32ToFloat4(IM_COL32(0, 190, 255, 255));
    const float s = (ImGui::GetFontSize() / 16.0f) * 0.85f;
    const ImVec2 size(kGuideRefW * s, kGuideRefH * s);
    const bool sideBySide = ImGui::GetContentRegionAvail().x >= size.x + ImGui::GetFontSize() * 24.0f;

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::Dummy(size);
    DrawControllerDiagram(ImGui::GetWindowDrawList(), origin, s);

    if (sideBySide) {
        ImGui::SameLine();
    } else {
        ImGui::Spacing();
    }

    ImGui::BeginGroup();
    ImGui::PushStyleColor(ImGuiCol_Text, accent);
    ImGui::TextWrapped("Buttons with a cyan ring have a new function with Modern Item Switching.");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    if (ImGui::BeginTable("##RemakeControlsGuide", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH)) {
        ImGui::TableSetupColumn("Button", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 9.0f);
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
        for (const GuideEntry& e : kGuideEntries) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::PushStyleColor(ImGuiCol_Text, e.changed ? accent : ImGui::GetStyleColorVec4(ImGuiCol_Text));
            ImGui::TextWrapped("%s", e.button);
            ImGui::PopStyleColor();
            ImGui::TableNextColumn();
            if (e.planned) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            }
            ImGui::TextWrapped("%s", e.text);
            if (e.planned) {
                ImGui::PopStyleColor();
            }
        }
        ImGui::EndTable();
    }
    ImGui::EndGroup();
}

// ---------------------------------------------------------------- Layout presets (gamepad e PC)
// Os vínculos são por porta (não por dispositivo), então não precisa de controle conectado para aplicar.
#ifndef __WIIU__
constexpr uint8_t kPresetPort = 0;   // porta 1 (aba "Link" do editor de controles)
constexpr int32_t kAxisPositive = 1; // mesmo valor usado nos defaults (SDLAxisDirectionToButtonMapping)
static_assert(kPresetPort == 0, "A chave HasConfig abaixo assume a porta 1.");

struct PresetButtonBinding {
    CONTROLLERBUTTONS_T bitmask;
    SDL_GameControllerButton button;
};

struct PresetAxisBinding {
    CONTROLLERBUTTONS_T bitmask;
    SDL_GameControllerAxis axis;
};

struct PresetKeyBinding {
    CONTROLLERBUTTONS_T bitmask;
    Ship::KbScancode key;
};

struct PresetMouseBinding {
    CONTROLLERBUTTONS_T bitmask;
    Ship::MouseBtn button;
};

// Gamepad (nomes do Xbox 360 / ViGEm). Edite aqui para trocar o layout.
const PresetButtonBinding kPresetButtons[] = {
    { BTN_CLEFT, SDL_CONTROLLER_BUTTON_LEFTSHOULDER },   // L  -> carrossel / item anterior
    { BTN_CRIGHT, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER }, // R  -> carrossel / próximo item
    { BTN_B, SDL_CONTROLLER_BUTTON_Y },                  // Y  -> ataque
    { BTN_CDOWN, SDL_CONTROLLER_BUTTON_X },              // X  -> pulo (ainda sem função)
    { BTN_A, SDL_CONTROLLER_BUTTON_A },                  // A  -> ações
    { BTN_L, SDL_CONTROLLER_BUTTON_B },                  // B  -> guardar
    { BTN_R, SDL_CONTROLLER_BUTTON_BACK },               // -  -> mapa (ainda sem função)
    { BTN_START, SDL_CONTROLLER_BUTTON_START },          // +  -> menu
};

const PresetAxisBinding kPresetAxes[] = {
    { BTN_Z, SDL_CONTROLLER_AXIS_TRIGGERLEFT },    // ZL -> mira / escudo
    { BTN_CUP, SDL_CONTROLLER_AXIS_TRIGGERRIGHT }, // ZR -> usa o item em foco
};

// PC (teclado + mouse). WASD continua sendo o analógico esquerdo (não é tocado). Edite aqui para trocar teclas.
const PresetKeyBinding kPcKeys[] = {
    { BTN_CUP, Ship::LUS_KB_F },        // F       -> usa o item em foco
    { BTN_CLEFT, Ship::LUS_KB_Q },      // Q       -> carrossel / item anterior
    { BTN_CRIGHT, Ship::LUS_KB_E },     // E       -> carrossel / próximo item
    { BTN_A, Ship::LUS_KB_SPACE },      // Espaço  -> ações
    { BTN_CDOWN, Ship::LUS_KB_SHIFT },  // Shift   -> pulo (ainda sem função)
    { BTN_L, Ship::LUS_KB_R },          // R       -> guardar
    { BTN_R, Ship::LUS_KB_M },          // M       -> mapa (ainda sem função)
    { BTN_START, Ship::LUS_KB_ENTER },  // Enter   -> menu
    { BTN_DLEFT, Ship::LUS_KB_1 },      // 1       -> slot D-Pad esquerda
    { BTN_DDOWN, Ship::LUS_KB_2 },      // 2       -> slot D-Pad baixo
    { BTN_DRIGHT, Ship::LUS_KB_3 },     // 3       -> slot D-Pad direita
    { BTN_DUP, Ship::LUS_KB_C },        // C       -> Navi
};

const PresetMouseBinding kPcMouse[] = {
    { BTN_B, Ship::LUS_MOUSE_BTN_LEFT }, // botão esquerdo -> ataque
    { BTN_Z, Ship::LUS_MOUSE_BTN_RIGHT }, // botão direito  -> mira / escudo
};

// Botões que o preset de PC limpa (teclado e mouse) antes de recriar os vínculos.
const CONTROLLERBUTTONS_T kPcClearedButtons[] = {
    BTN_A,     BTN_B,      BTN_L,     BTN_R,      BTN_Z,      BTN_START, BTN_CUP,
    BTN_CDOWN, BTN_CLEFT,  BTN_CRIGHT, BTN_DUP,   BTN_DDOWN,  BTN_DLEFT, BTN_DRIGHT,
};

void BindPresetMapping(const std::shared_ptr<Ship::ControllerButton>& button,
                       const std::shared_ptr<Ship::ControllerButtonMapping>& mapping) {
    button->AddButtonMapping(mapping);
    mapping->SaveToConfig();
    button->SaveButtonMappingIdsToConfig();
}

// Igual ao editor de controles: marca a porta como configurada pelo jogador. Os CVars definidos aqui e logo antes
// dos vínculos são gravados em disco pelos Save() que cada SaveToConfig() faz.
void MarkPresetPortConfigured() {
#ifdef CVAR_PREFIX_CONTROLLERS
    CVarSetInteger(CVAR_PREFIX_CONTROLLERS ".Port1.HasConfig", 1);
#endif
}

// Mesmo cálculo dos callbacks de "Enable Mouse Controls" / "Auto Capture Mouse Input" do editor de controles.
void RefreshMouseCapture() {
    auto wnd = std::dynamic_pointer_cast<Fast::Fast3dWindow>(Ship::Context::GetRawInstance()->GetWindow());
    if (wnd == nullptr) {
        return;
    }
    const bool enabled =
        CVarGetInteger(CVAR_SETTING("EnableMouse"), 0) != 0 && CVarGetInteger(CVAR_SETTING("AutoCaptureMouse"), 1) != 0;
    wnd->SetAutoCaptureMouse(enabled);
}

bool ApplyRemakeControllerLayout() {
    auto controller = Ship::Context::GetRawInstance()->GetControlDeck()->GetControllerByPort(kPresetPort);
    if (controller == nullptr) {
        return false;
    }

    MarkPresetPortConfigured();
    // O analógico direito move a câmera: Free Look (os C já saem do analógico direito abaixo) e mira com ele.
    CVarSetInteger(CVAR_SETTING("FreeLook.Enabled"), 1);
    CVarSetInteger(CVAR_SETTING("Controls.RightStickAim"), 1);

    for (const PresetButtonBinding& b : kPresetButtons) {
        auto button = controller->GetButton(b.bitmask);
        if (button == nullptr) {
            continue;
        }
        button->ClearAllButtonMappingsForDeviceType(Ship::PhysicalDeviceType::SDLGamepad);
        BindPresetMapping(button, std::make_shared<Ship::SDLButtonToButtonMapping>(
                                      kPresetPort, b.bitmask, static_cast<int32_t>(b.button)));
    }

    for (const PresetAxisBinding& a : kPresetAxes) {
        auto button = controller->GetButton(a.bitmask);
        if (button == nullptr) {
            continue;
        }
        button->ClearAllButtonMappingsForDeviceType(Ship::PhysicalDeviceType::SDLGamepad);
        BindPresetMapping(button, std::make_shared<Ship::SDLAxisDirectionToButtonMapping>(
                                      kPresetPort, a.bitmask, static_cast<int32_t>(a.axis), kAxisPositive));
    }
    return true;
}

bool ApplyPcControllerLayout() {
    auto controller = Ship::Context::GetRawInstance()->GetControlDeck()->GetControllerByPort(kPresetPort);
    if (controller == nullptr) {
        return false;
    }

    MarkPresetPortConfigured();
    // O mouse move a câmera em terceira pessoa (precisa do Free Look) e mira o escudo.
    CVarSetInteger(CVAR_SETTING("EnableMouse"), 1);
    CVarSetInteger(CVAR_SETTING("DisableThirdPersonMouse"), 0);
    CVarSetInteger(CVAR_SETTING("FreeLook.Enabled"), 1);

    for (CONTROLLERBUTTONS_T bitmask : kPcClearedButtons) {
        auto button = controller->GetButton(bitmask);
        if (button == nullptr) {
            continue;
        }
        button->ClearAllButtonMappingsForDeviceType(Ship::PhysicalDeviceType::Keyboard);
        button->ClearAllButtonMappingsForDeviceType(Ship::PhysicalDeviceType::Mouse);
    }

    for (const PresetKeyBinding& k : kPcKeys) {
        auto button = controller->GetButton(k.bitmask);
        if (button == nullptr) {
            continue;
        }
        BindPresetMapping(button, std::make_shared<Ship::KeyboardKeyToButtonMapping>(kPresetPort, k.bitmask, k.key));
    }

    for (const PresetMouseBinding& m : kPcMouse) {
        auto button = controller->GetButton(m.bitmask);
        if (button == nullptr) {
            continue;
        }
        BindPresetMapping(button, std::make_shared<Ship::MouseButtonToButtonMapping>(kPresetPort, m.bitmask, m.button));
    }

    RefreshMouseCapture();
    return true;
}

struct PresetUiState {
    double messageUntil = 0.0;
    bool failed = false;
};

// Texto + botão + popup de confirmação (Apply / Cancel, os dois fecham o popup).
void DrawPresetPopup(const char* intro, const char* buttonLabel, const char* popupId, const char* details,
                     const char* doneText, bool (*apply)(), PresetUiState& state) {
    ImGui::TextWrapped("%s", intro);

    if (ImGui::Button(buttonLabel)) {
        ImGui::OpenPopup(popupId);
    }

    if (ImGui::BeginPopupModal(popupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(details);
        ImGui::Spacing();

        if (ImGui::Button("Apply")) {
            state.failed = !apply();
            state.messageUntil = ImGui::GetTime() + 4.0;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (ImGui::GetTime() < state.messageUntil) {
        ImGui::TextWrapped("%s", state.failed ? "Could not find the controller on port 1." : doneText);
    }
}

void DrawRemakeLayoutPreset() {
    static PresetUiState sGamepadState;
    static PresetUiState sPcState;

    DrawPresetPopup("Gamepad: sets the bindings of port 1 to the layout used by Modern Item Switching (Xbox button "
                    "names) and lets the right stick move the camera.",
                    "Apply Remake Layout", "Apply Remake Layout##RemakeLayoutConfirm",
                    "This replaces the gamepad (SDL) bindings of A, B, L, R, Z, Start and the four C buttons on\n"
                    "port 1, and turns on Free Look and Right Stick Aiming. The D-Pad, the sticks and the\n"
                    "keyboard bindings are not changed.\n"
                    "\n"
                    "  LT (ZL)  ->  Z\n"
                    "  RT (ZR)  ->  C-Up\n"
                    "  LB (L)  ->  C-Left\n"
                    "  RB (R)  ->  C-Right\n"
                    "  Y  ->  B\n"
                    "  X  ->  C-Down\n"
                    "  A  ->  A\n"
                    "  B  ->  L\n"
                    "  Back (-)  ->  R\n"
                    "  Start (+)  ->  Start\n"
                    "\n"
                    "To undo the bindings, use Set Defaults in the controller editor.",
                    "Remake layout applied.", ApplyRemakeControllerLayout, sGamepadState);

    ImGui::Spacing();

    DrawPresetPopup("Keyboard + mouse: WASD to move, mouse to move the camera, with a layout for one hand on the "
                    "keyboard and one on the mouse.",
                    "Apply PC Layout", "Apply PC Layout##PcLayoutConfirm",
                    "This replaces the keyboard and mouse bindings of A, B, L, R, Z, Start, the four C buttons and\n"
                    "the D-Pad on port 1, and turns on Mouse Controls and Free Look so the mouse moves the\n"
                    "camera. WASD still moves Link. Gamepad bindings are not changed.\n"
                    "\n"
                    "  Left click  ->  B (attack)\n"
                    "  Right click  ->  Z (aim / shield)\n"
                    "  F  ->  C-Up (use the focused item)\n"
                    "  Q / E  ->  C-Left / C-Right (item carousel)\n"
                    "  Space  ->  A\n"
                    "  Shift  ->  C-Down (jump, coming soon)\n"
                    "  R  ->  L (put the sword away)\n"
                    "  M  ->  R (map, coming soon)\n"
                    "  Enter  ->  Start\n"
                    "  1 / 2 / 3  ->  D-Pad Left / Down / Right (free item slots)\n"
                    "  C  ->  D-Pad Up (Navi)\n"
                    "\n"
                    "Press F2 to toggle mouse capture. Mouse Controls and Free Look can be turned off again in\n"
                    "Settings > Controls.",
                    "PC layout applied.", ApplyPcControllerLayout, sPcState);
}
#else
void DrawRemakeLayoutPreset() {
    ImGui::TextWrapped("The layout presets are not available on this platform.");
}
#endif

} // namespace

void SohMenu::AddMenuReDemake() {
    // Padrões do ReDemake: só aplicados se o jogador nunca mexeu na opção (CVar ainda não existe).
    // Se ele desmarcar depois, o valor 0 fica salvo e é respeitado.
    if (CVarGetInteger(CVAR_ENHANCEMENT("PauseMenuAnimatedLink"), -1) == -1) {
        CVarSetInteger(CVAR_ENHANCEMENT("PauseMenuAnimatedLink"), 1);
    }

    // Controles do remake: ligados por padrão (RemakeControls.cpp também assume 1 quando o CVar não existe).
    if (CVarGetInteger(CVAR_ENHANCEMENT("RemakeControls"), -1) == -1) {
        CVarSetInteger(CVAR_ENHANCEMENT("RemakeControls"), 1);
    }

    AddMenuEntry("ReDemake", CVAR_SETTING("Menu.ReDemakeSidebarSection"));

    // ------------------------------------------------------------------ Ocarina
    AddSidebarEntry("ReDemake", "Ocarina", 1);
    WidgetPath path = { "ReDemake", "Ocarina", SECTION_COLUMN_1 };

    AddWidget(path, "Microphone Ocarina", WIDGET_SEPARATOR_TEXT);

    AddWidget(path, "Microphone Ocarina Playback", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_MIC("Microphone"))
        .Options(CheckboxOptions().Tooltip(
            "Play ocarina notes by humming, singing or playing a real instrument into your microphone. "
            "Recognized notes: D, F, A, B and high D. Use the sliders below to calibrate octave, "
            "sensitivity and tuning."));

    AddWidget(path, "Mic Sensitivity", WIDGET_CVAR_SLIDER_INT)
        .CVar(CVAR_MIC("MicSensitivity"))
        .PreFunc([](WidgetInfo& info) { info.isHidden = MicDisabled(); })
        .Options(IntSliderOptions().Min(0).Max(100).DefaultValue(45).Format("%d").ShowButtons(true).Tooltip(
            "Higher values pick up quieter sounds."));

    AddWidget(path, "Mic Octave Shift", WIDGET_CVAR_SLIDER_INT)
        .CVar(CVAR_MIC("MicOctave"))
        .PreFunc([](WidgetInfo& info) { info.isHidden = MicDisabled(); })
        .Options(IntSliderOptions().Min(-3).Max(3).DefaultValue(0).Format("%d").ShowButtons(true).Tooltip(
            "Shifts the register up or down."));

    AddWidget(path, "Mic Tuning (cents)", WIDGET_CVAR_SLIDER_INT)
        .CVar(CVAR_MIC("MicTuning"))
        .PreFunc([](WidgetInfo& info) { info.isHidden = MicDisabled(); })
        .Options(IntSliderOptions().Min(-50).Max(50).DefaultValue(0).Format("%d").ShowButtons(true).Tooltip(
            "Tunes the detection to your instrument, in hundredths of a semitone."));

    AddWidget(path, "Mute In-Game Ocarina While Playing", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_MIC("MicMute"))
        .PreFunc([](WidgetInfo& info) { info.isHidden = MicDisabled(); })
        .Options(CheckboxOptions().Tooltip(
            "The in-game ocarina stays silent while you play on your real instrument or hum, like the remake. "
            "Only the notes played with the microphone are muted; the game still plays the song once you "
            "play the melody correctly."));

    AddWidget(path, "Show Microphone Indicator", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_MIC("MicVisual"))
        .PreFunc([](WidgetInfo& info) { info.isHidden = MicDisabled(); })
        .Options(CheckboxOptions().Tooltip(
            "Shows streaks of light across the screen while the game is picking up sound from your "
            "microphone. Blue = A, yellow = C buttons, gray = sound heard but not a valid ocarina note."));

    AddWidget(path, "Hide Microphone Status Icon", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_MIC("MicHideStatus"))
        .PreFunc([](WidgetInfo& info) { info.isHidden = MicDisabled(); })
        .Options(CheckboxOptions().Tooltip(
            "By default, a microphone icon at the top of the screen tells you whether the microphone is on "
            "or off while the ocarina is out. Check this to hide it."));

    // Calibração ao vivo: só roda enquanto esta aba está desenhada (o mic fecha sozinho ~3 s depois).
    // Requer WIDGET_CUSTOM + CustomFunction; veja a observação no final da resposta se não compilar.
    AddWidget(path, "MicLivePreview", WIDGET_CUSTOM)
        .PreFunc([](WidgetInfo& info) { info.isHidden = MicDisabled(); })
        .CustomFunction([](WidgetInfo& info) {
            MicOcarina_Poll(CVarGetInteger(CVAR_MIC("MicOctave"), 0), CVarGetInteger(CVAR_MIC("MicSensitivity"), 45),
                            CVarGetInteger(CVAR_MIC("MicTuning"), 0));
            static const char* noteNames[] = { "A (D4)", "C-Down (F4)", "C-Right (A4)", "C-Left (B4)", "C-Up (D5)" };
            const int note = MicOcarina_GetNote();
            ImGui::Text("Mic: %.0f Hz  ->  %s", MicOcarina_GetDebugHz(),
                        (note >= 0 && note <= 4) ? noteNames[note] : "-");
        });

    // ------------------------------------------------------------------ Controls
    AddSidebarEntry("ReDemake", "Controls", 1);
    WidgetPath controlsPath = { "ReDemake", "Controls", SECTION_COLUMN_1 };

    AddWidget(controlsPath, "Item Controls", WIDGET_SEPARATOR_TEXT);

    AddWidget(controlsPath, "Modern Item Switching", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_ENHANCEMENT("RemakeControls"))
        .Options(CheckboxOptions().Tooltip(
            "Remake-style item controls: ZL (Z) also raises the shield, ZR uses the focused item shown in the "
            "top-right corner, and holding L or R pauses the game and opens the item carousel. The C buttons "
            "are no longer drawn on the HUD. Turn this off to get the original C button behavior back. "
            "Requires the controller bindings described in the project notes (ZR on C-Up, L on C-Left, "
            "R on C-Right)."));

    AddWidget(controlsPath, "Layout Presets", WIDGET_SEPARATOR_TEXT)
        .PreFunc([](WidgetInfo& info) { info.isHidden = ModernControlsDisabled(); });

    AddWidget(controlsPath, "RemakeLayoutPreset", WIDGET_CUSTOM)
        .PreFunc([](WidgetInfo& info) { info.isHidden = ModernControlsDisabled(); })
        .CustomFunction([](WidgetInfo& info) { DrawRemakeLayoutPreset(); })
        .HideInSearch(true);

    AddWidget(controlsPath, "Controls Guide", WIDGET_SEPARATOR_TEXT)
        .PreFunc([](WidgetInfo& info) { info.isHidden = ModernControlsDisabled(); });

    AddWidget(controlsPath, "ControlsGuideDiagram", WIDGET_CUSTOM)
        .PreFunc([](WidgetInfo& info) { info.isHidden = ModernControlsDisabled(); })
        .CustomFunction([](WidgetInfo& info) { DrawControlsGuide(); })
        .HideInSearch(true);
}
} // namespace SohGui
