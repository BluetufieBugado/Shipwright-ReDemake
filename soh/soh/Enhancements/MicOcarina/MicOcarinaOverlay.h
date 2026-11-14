#pragma once

// Indicadores visuais do ocarina por microfone. Chame uma vez por frame, DENTRO de um frame do ImGui
// (ex.: em DrawElement de uma janela). Só desenha enquanto o ocarina está em uso no jogo.
//   micEnabled:  "Microphone Ocarina Playback" está ligado
//   showStreaks: faixas de luz cruzando a tela (só aparecem se micEnabled)
//   showStatus:  ícone de microfone no topo da tela (ligado/desligado)
void MicOcarinaOverlay_Draw(bool micEnabled, bool showStreaks, bool showStatus);
