#include "SohMenu.h"

#include <imgui.h>
#include <libultraship/bridge/consolevariablebridge.h>

#include "soh/cvar_prefixes.h"
#include "soh/SohGui/UIWidgets.hpp"
#include "soh/Enhancements/MicOcarina/MicOcarina.h"

namespace SohGui {
extern std::shared_ptr<SohMenu> mSohMenu;

using namespace UIWidgets;

// Mesmos CVars que já existiam (code_800EC960.c e o overlay continuam lendo estes nomes).
#define CVAR_MIC(name) CVAR_SETTING("CustomOcarina." name)

static bool MicDisabled() {
    return !CVarGetInteger(CVAR_MIC("Microphone"), 0);
}

void SohMenu::AddMenuReDemake() {
    // Padrões do ReDemake: só aplicados se o jogador nunca mexeu na opção (CVar ainda não existe).
    // Se ele desmarcar depois, o valor 0 fica salvo e é respeitado.
    if (CVarGetInteger(CVAR_ENHANCEMENT("PauseMenuAnimatedLink"), -1) == -1) {
        CVarSetInteger(CVAR_ENHANCEMENT("PauseMenuAnimatedLink"), 1);
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
}
} // namespace SohGui
