# Better Save Menu: edições (escrito sem compilar, igual às anteriores)

Arquivos novos (copiar para `soh/src/overlays/misc/ovl_kaleido_scope/`):
- `z_kaleido_save_popup_tex.h`
- `z_kaleido_save_popup.inc`

Tudo abaixo é em `z_kaleido_scope_PAL.c`. As edições 1 a 6 são pequenas.

## 1. Helper do CVar + deslocamento do mapa (perto de `MAP_PAGE_SHIFT_X`, ~linha 1494)

Adicione ANTES de `KaleidoScope_GetMapPageShiftX`:

```c
// "Better Save Menu": no menu em abas, o prompt de salvar vira uma janelinha em vez da pagina de save
static bool KaleidoScope_UseSavePopup(PauseContext* pauseCtx) {
    return CVarGetInteger(CVAR_ENHANCEMENT("TabbedPauseMenu"), 1) != 0 &&
           CVarGetInteger(CVAR_ENHANCEMENT("BetterSaveMenu"), 1) != 0 && !pauseCtx->randoQuestMode;
}
```

E troque o retorno de `KaleidoScope_GetMapPageShiftX`:

```c
    return (KaleidoScope_IsTabbedMapPage(pauseCtx) && (pauseCtx->state != 7 || KaleidoScope_UseSavePopup(pauseCtx)))
               ? MAP_PAGE_SHIFT_X
               : 0.0f;
```

## 2. Não desenhar as páginas vizinhas durante a janelinha (~linha 1710)

```c
        bool hideInactivePages = CVarGetInteger(CVAR_ENHANCEMENT("TabbedPauseMenu"), 1) != 0 &&
                                 (pauseCtx->state == 6 || tabbedOpenClose ||
                                  (pauseCtx->state == 7 && KaleidoScope_UseSavePopup(pauseCtx)));
```

## 3. Não desenhar a página "SAVE" do jogo (~linha 1905)

Troque a condição do `if` por:

```c
    if ((!(pauseCtx->state == 7 && KaleidoScope_UseSavePopup(pauseCtx)) &&
         GameInteractor_Should(VB_DRAW_SAVE_MENU, pauseCtx->state == 7, pauseCtx)) ||
        ((pauseCtx->state >= 8) && (pauseCtx->state < 0x12))) {
```

## 4. HUD escondido na aba Mapa também durante a janelinha (`KaleidoScope_IsMapTabShown`, ~linha 3800)

```c
    } else if (pauseCtx->state != 6 && pauseCtx->state != 0x12 &&
               !(pauseCtx->state == 7 && KaleidoScope_UseSavePopup(pauseCtx))) {
        return false;
    }
```

## 5. Include da janelinha

Logo depois do `}` final de `KaleidoScope_DrawMapSidebar` e antes de `void KaleidoScope_Draw(`:

```c
#include "z_kaleido_save_popup.inc"
```

## 6. Em `KaleidoScope_Draw` (~linhas 6050 a 6100)

a) Painel de info não desenha durante a janelinha:

```c
        if (!((pauseCtx->state >= 8) && (pauseCtx->state <= 0x11)) &&
            !(pauseCtx->state == 7 && KaleidoScope_UseSavePopup(pauseCtx))) {
            KaleidoScope_DrawInfoPanel(play);
        }
```

b) Barra de abas continua visível (adicione `|| (state == 7 && popup)`):

```c
        if (tabbedMenu && (pauseCtx->state == 4 || pauseCtx->state == 5 || pauseCtx->state == 6 ||
                           pauseCtx->state == 0x12 ||
                           (pauseCtx->state == 7 && KaleidoScope_UseSavePopup(pauseCtx))) &&
            pauseCtx->pageIndex <= PAUSE_EQUIP) {
```

c) Sidebar continua desenhada (apagada, sem foco) e depois a janelinha. Substitua o bloco da sidebar por:

```c
        // Sidebar da aba Mapa (menu aberto, ou parada atras da janelinha de salvar)
        if (tabbedMenu && (pauseCtx->state == 6 || (pauseCtx->state == 7 && KaleidoScope_UseSavePopup(pauseCtx))) &&
            KaleidoScope_IsTabbedMapPage(pauseCtx)) {
            KaleidoScope_DrawMapSidebar(play);
        }

        // Janelinha de salvar (por cima de tudo)
        if (tabbedMenu && pauseCtx->state == 7 && KaleidoScope_UseSavePopup(pauseCtx)) {
            KaleidoScope_DrawSavePopup(play);
        }
```

## 7. Em `KaleidoScope_Update`, `case 7:` (~linha 6990)

Primeira coisa dentro do `if (GameInteractor_Should(VB_LOAD_SAVE_MENU, 1, play)) {`, antes do `switch (pauseCtx->unk_1EC)`:

```c
                if (KaleidoScope_SavePopupUpdate(play)) {
                    break;
                }
```

(o `break` sai do `switch (pauseCtx->state)`; com a opção desligada nada muda.)

## 8. Entrada nas configurações

Procure onde "TabbedPauseMenu" aparece em `soh/soh/SohGui` (`Select-String -Path soh\soh\SohGui\*.cpp -Pattern TabbedPauseMenu`), copie aquele bloco logo abaixo e troque o CVar para `CVAR_ENHANCEMENT("BetterSaveMenu")`, o nome para "Better Save Menu" e `DefaultValue` para ligado. Sem essa entrada o recurso já fica ligado (default 1 no código), só não dá para desligar.

## Comportamento

- Salvar (sidebar) ou B: abre a janelinha sobre a aba atual, sem girar o cubo.
- Cima/baixo escolhe, A confirma, B/Start cancela.
- Sim: salva, mostra "Jogo salvo!" por ~2 s (ou fecha direto com SkipSaveConfirmation) e volta ao menu na mesma aba.
- Não/B: volta ao menu. Diferença da versão original: lá o "Não" fechava o menu de pausa inteiro.
- Game over e o menu sem abas continuam com o fluxo original.
