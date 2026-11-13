#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Chame todo frame enquanto o ocarina (ou o painel de config) estiver ativo.
// Abre o microfone sob demanda e o fecha sozinho após ~3 s sem ser chamada.
//   octaveShift: -3..3  (desloca o registro; resolve "meu Ré saiu como Ré agudo")
//   sensitivity: 0..100 (maior = capta sons mais fracos)
//   tuningCents: -50..50 (afinação do instrumento em centésimos de semitom)
void MicOcarina_Poll(int octaveShift, int sensitivity, int tuningCents);

// Nota detectada agora: 0=D4(A) 1=F4(C-baixo) 2=A4(C-direita) 3=B4(C-esquerda) 4=D5(C-cima), -1=nenhuma
int MicOcarina_GetNote(void);

// Última frequência detectada em Hz (0 se nenhuma). Útil para calibrar.
float MicOcarina_GetDebugHz(void);

// Intensidade do som captado: 0 = silêncio (abaixo do limiar de sensibilidade), 0.25..1 = som presente.
// Usado pelo indicador visual; vale 0 sempre que o microfone está fechado.
float MicOcarina_GetLevel(void);

// Encerra tudo (também é registrado via atexit).
void MicOcarina_Stop(void);

#ifdef __cplusplus
}
#endif
