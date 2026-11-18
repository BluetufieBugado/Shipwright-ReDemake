#pragma once

// Funções com ligação C: chamadas também por z_parameter.c (usam int no lugar de s32 de propósito).
extern "C" {
// O sistema de controles do remake está ligado (CVar)?
int RemakeControls_IsEnabled(void);
// 1 = o HUD original não deve desenhar os botões C (o item em foco vai para o canto da tela)
int RemakeControls_HideOriginalCButtons(void);
// Chamada por Interface_Draw a cada frame: o HUD está escondendo os botões C neste momento (aba Quest/Mapa)?
void RemakeControls_SetCornerHidden(int hidden);
}
bool RemakeControls_IsCornerHidden();

// Estado do carrossel de itens (RemakeControls.cpp)
bool RemakeControls_IsCarouselOpen();
// O item deste slot do inventário serve para a idade atual do Link?
bool RemakeControls_IsSlotUsable(int slot);
// Slots do inventário que aparecem no carrossel, já na ordem atual (padrão = ordem dos slots, ou "por tipo").
// Preenche outSlots (até maxSlots) e retorna quantos são. L/R e a faixa da tela usam esta mesma lista.
int RemakeControls_GetCarouselSlots(int* outSlots, int maxSlots);
// A ordenação "por tipo" está ligada? (alterna com Y com o carrossel aberto)
bool RemakeControls_IsSortByType();

// Faixa de itens do carrossel (RemakeControlsOverlay.cpp).
// Chame uma vez por frame, de dentro do Draw() de uma janela hospedeira SEM moldura (sem ImGui::Begin).
// Desenha direto numa camada da tela: não existe janela, título nem X para a pessoa fechar.
// Também desenha o item em foco no canto superior direito (substitui os botões C no HUD).
void RemakeControlsOverlay_Draw();
