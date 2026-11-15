#pragma once

// Retículo da mira em terceira pessoa (estilingue, arco e hookshot).
// Desenhado com a draw list de primeiro plano do ImGui: não cria nenhuma janela, moldura, título ou botão.
// Chame uma vez por frame, DENTRO de um frame do ImGui (ex.: no DrawElement de uma janela invisível).

extern "C" {
// Definida em z_player.c: true enquanto o Link está mirando com a mira em terceira pessoa
// (e não enquanto o jogo está pausado).
unsigned char Player_IsThirdPersonAiming(void);
}

//   aiming:      o retículo deve estar visível agora (aparece e some suavemente)
//   sizePercent: raio do anel em % da altura da tela
//   opacity:     opacidade máxima, de 0 a 1
void ThirdPersonAimReticle_Draw(bool aiming, float sizePercent, float opacity);
