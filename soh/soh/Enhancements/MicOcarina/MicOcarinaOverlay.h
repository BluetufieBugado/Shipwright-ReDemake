#pragma once

// Indicador visual do ocarina por microfone: faixas de luz que cruzam a tela enquanto o jogo capta som.
// Deve ser chamado uma vez por frame, DENTRO de um frame do ImGui (ex.: em DrawElement de uma janela).
void MicOcarinaOverlay_Draw();
