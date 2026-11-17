#pragma once

// Retículo da mira em terceira pessoa (estilingue, arco e hookshot).
// Desenhado com a draw list de primeiro plano do ImGui: não cria nenhuma janela, moldura, título ou botão.
// Chame uma vez por frame, DENTRO de um frame do ImGui (ex.: no DrawElement de uma janela invisível).

extern "C" {
// Definida em z_player.c: true enquanto o Link está mirando com a mira em terceira pessoa
// (e não enquanto o jogo está pausado).
unsigned char Player_IsThirdPersonAiming(void);
// Ammo the HUD would show for the weapon in hand (arrows, seeds, minigame shots), or -1 for none.
int Player_GetThirdPersonAimAmmo(void);
// Minigame counter shown under the ammo, or -1 for none. kind: 1 = gallery hits, 2 = horseback archery points.
int Player_GetThirdPersonAimScore(int* kind);
}

//   aiming:      o retículo deve estar visível agora (aparece e some suavemente)
//   sizePercent: raio do anel em % da altura da tela
//   opacity:     opacidade máxima, de 0 a 1
//   ammo:        munição ao lado do anel (-1 = não mostrar)
//   score:       contador do minigame abaixo da munição (-1 = não mostrar)
//   scoreKind:   1 = acertos na Galeria de Tiro, 2 = pontos no arco a cavalo
void ThirdPersonAimReticle_Draw(bool aiming, float sizePercent, float opacity, int ammo, int score, int scoreKind);
