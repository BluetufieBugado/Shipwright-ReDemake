#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Chame todo frame enquanto o ocarina estiver ativo.
// Abre o microfone sob demanda e o fecha sozinho após ~3 s sem ser chamada.
void MicOcarina_Poll(void);

// Nota detectada agora: 0=D4(A) 1=F4(C-baixo) 2=A4(C-direita) 3=B4(C-esquerda) 4=D5(C-cima), -1=nenhuma
int MicOcarina_GetNote(void);

// Encerra tudo (também é registrado via atexit).
void MicOcarina_Stop(void);

#ifdef __cplusplus
}
#endif
