#pragma once

// Estado do carrossel de itens (RemakeControls.cpp)
bool RemakeControls_IsCarouselOpen();
// O item deste slot do inventário serve para a idade atual do Link?
bool RemakeControls_IsSlotUsable(int slot);

// Faixa de itens do carrossel (RemakeControlsOverlay.cpp).
// Chame uma vez por frame, de dentro do Draw() de uma janela hospedeira SEM moldura (sem ImGui::Begin).
// Desenha direto numa camada da tela: não existe janela, título nem X para a pessoa fechar.
void RemakeControlsOverlay_Draw();
