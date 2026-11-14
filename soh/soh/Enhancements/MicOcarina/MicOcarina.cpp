// Ocarina por microfone: captura áudio (SDL2), detecta o pitch (YIN) e
// converte em uma das 5 notas do ocarina de Ocarina of Time.
#include "MicOcarina.h"

#if __has_include(<SDL2/SDL.h>)
#include <SDL2/SDL.h>
#else
#include <SDL.h>
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <mutex>
#include <thread>
#include <vector>

namespace {
constexpr int kSampleRate = 44100;
constexpr int kWindow = 2048;  // amostras por análise
constexpr int kMaxTau = 1024;  // ~43 Hz
constexpr int kMinTau = 20;    // ~2200 Hz (cobre flauta doce aguda)
constexpr float kYinThresh = 0.15f;
constexpr int kHopMs = 12;
constexpr int kNeedSilence = 7;     // ~84 ms de silêncio para soltar a nota
constexpr Uint32 kIdleTimeoutMs = 3000;

// --- Estabilizador de notas (votação) ---
// Cada análise (~12 ms) vota em uma nota. Olhamos as últimas kHist votações:
//  - kAttackSkip: quadros ignorados no começo de cada nota (o ataque é instável).
//  - kAcquire: votos (de kHist) para aceitar uma nota vinda do silêncio.
//  - kSwitch: votos (de kHist) para trocar de uma nota segurada para outra.
//    Quanto MAIOR, mais tolerante a desvios (mas a troca de nota demora um pouco mais).
//    Desvios de até (kHist - kSwitch) quadros são ignorados.
constexpr int kHist = 8;
constexpr int kAttackSkip = 2;
constexpr int kAcquire = 5;
constexpr int kSwitch = 6;

SDL_AudioDeviceID gDev = 0;
std::thread gThread;
std::atomic<bool> gRun{ false };
std::atomic<int> gNote{ -1 };
std::atomic<Uint32> gLastPoll{ 0 };
bool gAtExitRegistered = false;

std::mutex gMutex;
std::vector<float> gRing(kWindow * 4, 0.0f);
size_t gWrite = 0;

// Ajustes (atualizados pela thread do jogo em MicOcarina_Poll)
std::atomic<float> gRmsThreshold{ 0.0126f };
std::atomic<float> gTuningCents{ 0.0f };
std::atomic<int> gOctaveShift{ 0 };
std::atomic<bool> gTolerant{ false };
std::atomic<float> gDebugHz{ 0.0f };
std::atomic<float> gLevel{ 0.0f }; // 0 = silêncio; 0.25..1 = som acima do limiar (indicador visual)

void SDLCALL CaptureCb(void*, Uint8* stream, int len) {
    const float* in = reinterpret_cast<const float*>(stream);
    int n = len / static_cast<int>(sizeof(float));
    std::lock_guard<std::mutex> lk(gMutex);
    for (int i = 0; i < n; i++) {
        gRing[gWrite] = in[i];
        gWrite = (gWrite + 1) % gRing.size();
    }
}

// YIN simplificado. Retorna Hz, ou 0 se não houver nota clara.
float DetectPitch(const std::vector<float>& x, float sr) {
    const int W = kWindow - kMaxTau;
    std::vector<float> d(kMaxTau, 0.0f);
    for (int tau = 1; tau < kMaxTau; tau++) {
        float sum = 0.0f;
        for (int i = 0; i < W; i++) {
            float diff = x[i] - x[i + tau];
            sum += diff * diff;
        }
        d[tau] = sum;
    }
    std::vector<float> cm(kMaxTau, 1.0f);
    float running = 0.0f;
    for (int tau = 1; tau < kMaxTau; tau++) {
        running += d[tau];
        cm[tau] = running > 0.0f ? d[tau] * tau / running : 1.0f;
    }
    int best = -1;
    for (int tau = kMinTau; tau < kMaxTau - 1; tau++) {
        if (cm[tau] < kYinThresh) {
            while (tau + 1 < kMaxTau - 1 && cm[tau + 1] < cm[tau]) {
                tau++;
            }
            best = tau;
            break;
        }
    }
    if (best < 1) {
        // Fallback: sem mergulho abaixo do limiar, usa o melhor mínimo se razoável
        float minVal = 1.0f;
        for (int tau = kMinTau; tau < kMaxTau - 1; tau++) {
            if (cm[tau] < minVal) {
                minVal = cm[tau];
                best = tau;
            }
        }
        if (best < 1 || minVal > 0.25f) {
            return 0.0f;
        }
    }
    float s0 = cm[best - 1], s1 = cm[best], s2 = cm[best + 1];
    float denom = s0 + s2 - 2.0f * s1;
    float shift = denom != 0.0f ? 0.5f * (s0 - s2) / denom : 0.0f;
    return sr / (static_cast<float>(best) + shift);
}

// Frequência -> índice da nota (0=D4, 1=F4, 2=A4, 3=B4, 4=D5), ou -1.
// 'held' é a nota atualmente sustentada (histerese: tolera mais desvio enquanto segura).
int FreqToNote(float hz, int held) {
    if (hz <= 0.0f) {
        return -1;
    }
    float midi = 69.0f + 12.0f * std::log2(hz / 440.0f) - gTuningCents.load() / 100.0f +
                 12.0f * static_cast<float>(gOctaveShift.load());
    static const int pcs[4] = { 2, 5, 9, 11 }; // D, F, A, B

    float pc = std::fmod(midi, 12.0f);
    if (pc < 0.0f) {
        pc += 12.0f;
    }
    auto pcDist = [&](int i) {
        float dist = std::fabs(pc - static_cast<float>(pcs[i]));
        return std::min(dist, 12.0f - dist);
    };

    int bestIdx = -1;
    float bestDist = 1e9f;
    for (int i = 0; i < 4; i++) {
        float dist = pcDist(i);
        if (dist < bestDist) {
            bestDist = dist;
            bestIdx = i;
        }
    }

    // Histerese: se já está segurando uma nota e o som continua perto dela, mantém.
    if (held >= 0) {
        int heldPc = held == 4 ? 0 : held; // D5 usa a mesma classe de altura do D4
        if (pcDist(heldPc) <= 1.0f) {
            bestIdx = heldPc;
            bestDist = 0.0f;
        }
    }
    // Fora de ~70 cents de qualquer nota válida: ignora, a não ser no modo tolerante (aceita a mais próxima).
    if (bestDist > 0.7f && !gTolerant.load()) {
        return -1;
    }
    if (bestIdx == 0 && midi >= 72.0f) { // D agudo -> D5 (C-cima)
        return 4;
    }
    return bestIdx;
}

void Worker() {
    std::vector<float> window(kWindow);
    std::array<int, kHist> hist;
    hist.fill(-1);
    size_t hpos = 0;
    int silenceCount = 0;
    int attackSkip = kAttackSkip;
    int stable = -1;

    auto push = [&](int v) {
        hist[hpos] = v;
        hpos = (hpos + 1) % kHist;
    };

    while (gRun) {
        std::this_thread::sleep_for(std::chrono::milliseconds(kHopMs));

        if (SDL_GetTicks() - gLastPoll.load() > kIdleTimeoutMs) {
            break; // ocarina não está em uso: fecha o microfone
        }
        {
            std::lock_guard<std::mutex> lk(gMutex);
            size_t start = (gWrite + gRing.size() - kWindow) % gRing.size();
            for (int i = 0; i < kWindow; i++) {
                window[i] = gRing[(start + i) % gRing.size()];
            }
        }
        float rms = 0.0f;
        for (float v : window) {
            rms += v * v;
        }
        rms = std::sqrt(rms / kWindow);
        {
            const float thr = gRmsThreshold.load();
            gLevel.store(rms > thr ? std::min(1.0f, std::max(0.25f, rms / (thr * 4.0f))) : 0.0f);
        }

        int now = -1;
        float hz = 0.0f;
        if (rms > gRmsThreshold.load()) {
            hz = DetectPitch(window, static_cast<float>(kSampleRate));
            now = FreqToNote(hz, stable);
        }
        gDebugHz.store(hz);

        if (now < 0) {
            push(-1);
            if (++silenceCount >= kNeedSilence) {
                stable = -1;
                hist.fill(-1);
                attackSkip = kAttackSkip;
            }
        } else {
            silenceCount = 0;
            if (stable < 0 && attackSkip > 0) {
                attackSkip--; // ignora o ataque instável do começo da nota
            } else {
                push(now);
                int counts[5] = { 0, 0, 0, 0, 0 };
                for (int h : hist) {
                    if (h >= 0) {
                        counts[h]++;
                    }
                }
                int best = 0;
                for (int i = 1; i < 5; i++) {
                    if (counts[i] > counts[best]) {
                        best = i;
                    }
                }
                const int last = hist[(hpos + kHist - 1) % kHist];
                if (stable < 0) {
                    if (counts[best] >= kAcquire && last == best) {
                        stable = best;
                    }
                } else if (best != stable && counts[best] >= kSwitch && counts[best] > counts[stable] &&
                           last == best) {
                    stable = best;
                }
            }
        }
        gNote.store(stable);
    }

    gNote.store(-1);
    gLevel.store(0.0f);
    if (gDev) {
        SDL_CloseAudioDevice(gDev);
        gDev = 0;
    }
    gRun = false;
}

void StartInternal() {
    if (gThread.joinable()) {
        gThread.join(); // thread anterior já terminou (timeout)
    }
    if (!SDL_WasInit(SDL_INIT_AUDIO)) {
        SDL_InitSubSystem(SDL_INIT_AUDIO);
    }
    SDL_AudioSpec want{}, have{};
    want.freq = kSampleRate;
    want.format = AUDIO_F32SYS;
    want.channels = 1;
    want.samples = 512;
    want.callback = CaptureCb;
    // Sem SDL_AUDIO_ALLOW_*_CHANGE: o SDL converte para o formato pedido.
    gDev = SDL_OpenAudioDevice(nullptr, 1 /*captura*/, &want, &have, 0);
    if (!gDev) {
        return; // sem microfone: o jogo segue normal com os botões
    }
    SDL_PauseAudioDevice(gDev, 0);
    gRun = true;
    gThread = std::thread(Worker);
    if (!gAtExitRegistered) {
        std::atexit(MicOcarina_Stop);
        gAtExitRegistered = true;
    }
}
} // namespace

extern "C" void MicOcarina_Poll(int octaveShift, int sensitivity, int tuningCents) {
    sensitivity = std::min(100, std::max(0, sensitivity));
    // 0 -> 0.1 (pouco sensível) ... 100 -> 0.001 (muito sensível)
    gRmsThreshold.store(0.1f * std::pow(0.01f, static_cast<float>(sensitivity) / 100.0f));
    gTuningCents.store(static_cast<float>(std::min(50, std::max(-50, tuningCents))));
    gOctaveShift.store(std::min(3, std::max(-3, octaveShift)));
    gLastPoll.store(SDL_GetTicks());
    if (!gRun) {
        StartInternal();
    }
}

extern "C" void MicOcarina_SetTolerant(int enabled) {
    gTolerant.store(enabled != 0);
}

extern "C" int MicOcarina_GetNote(void) {
    return gNote.load();
}

extern "C" float MicOcarina_GetDebugHz(void) {
    return gDebugHz.load();
}

extern "C" float MicOcarina_GetLevel(void) {
    return gLevel.load();
}

extern "C" void MicOcarina_Stop(void) {
    gLastPoll.store(0);
    gRun = false;
    if (gThread.joinable()) {
        gThread.join();
    }
    if (gDev) {
        SDL_CloseAudioDevice(gDev);
        gDev = 0;
    }
    gNote.store(-1);
    gDebugHz.store(0.0f);
    gLevel.store(0.0f);
}
