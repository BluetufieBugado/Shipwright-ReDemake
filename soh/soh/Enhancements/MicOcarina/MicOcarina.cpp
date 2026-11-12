// Ocarina por microfone: captura áudio (SDL2), detecta o pitch (YIN) e
// converte em uma das 5 notas do ocarina de Ocarina of Time.
#include "MicOcarina.h"

#if __has_include(<SDL2/SDL.h>)
#include <SDL2/SDL.h>
#else
#include <SDL.h>
#endif

#include <algorithm>
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
constexpr int kMinTau = 40;    // ~1100 Hz
constexpr float kYinThresh = 0.15f;
constexpr int kHopMs = 12;
constexpr int kNeedStable = 5;      // ~60 ms para aceitar uma nota
constexpr int kNeedSilence = 7;     // ~84 ms de silêncio para soltar
constexpr Uint32 kIdleTimeoutMs = 3000;

SDL_AudioDeviceID gDev = 0;
std::thread gThread;
std::atomic<bool> gRun{ false };
std::atomic<int> gNote{ -1 };
std::atomic<Uint32> gLastPoll{ 0 };
bool gAtExitRegistered = false;

std::mutex gMutex;
std::vector<float> gRing(kWindow * 4, 0.0f);
size_t gWrite = 0;

// Ajustes (futuramente ligue a CVars/sliders)
float gRmsThreshold = 0.015f; // sensibilidade
float gTuningCents = 0.0f;    // desafinação do instrumento

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
        return 0.0f;
    }
    float s0 = cm[best - 1], s1 = cm[best], s2 = cm[best + 1];
    float denom = s0 + s2 - 2.0f * s1;
    float shift = denom != 0.0f ? 0.5f * (s0 - s2) / denom : 0.0f;
    return sr / (static_cast<float>(best) + shift);
}

// Frequência -> índice da nota (0=D4, 1=F4, 2=A4, 3=B4, 4=D5), ou -1.
int FreqToNote(float hz) {
    if (hz <= 0.0f) {
        return -1;
    }
    float midi = 69.0f + 12.0f * std::log2(hz / 440.0f) - gTuningCents / 100.0f;
    static const int pcs[4] = { 2, 5, 9, 11 }; // D, F, A, B
    static const int idx[4] = { 0, 1, 2, 3 };

    float pc = std::fmod(midi, 12.0f);
    if (pc < 0.0f) {
        pc += 12.0f;
    }
    float bestDist = 1e9f;
    int bestIdx = -1;
    for (int i = 0; i < 4; i++) {
        float dist = std::fabs(pc - static_cast<float>(pcs[i]));
        dist = std::min(dist, 12.0f - dist);
        if (dist < bestDist) {
            bestDist = dist;
            bestIdx = idx[i];
        }
    }
    if (bestDist > 0.7f) { // fora de ~70 cents de qualquer nota válida
        return -1;
    }
    if (bestIdx == 0 && midi >= 72.0f) { // D agudo -> D5
        return 4;
    }
    return bestIdx;
}

void Worker() {
    std::vector<float> window(kWindow);
    int candidate = -1, candCount = 0, silenceCount = 0;
    int stable = -1;

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

        int now = -1;
        if (rms > gRmsThreshold) {
            now = FreqToNote(DetectPitch(window, static_cast<float>(kSampleRate)));
        }

        if (now < 0) {
            if (++silenceCount >= kNeedSilence) {
                stable = -1;
                candidate = -1;
                candCount = 0;
            }
        } else {
            silenceCount = 0;
            if (now == candidate) {
                if (++candCount >= kNeedStable) {
                    stable = now;
                }
            } else {
                candidate = now;
                candCount = 1;
            }
        }
        gNote.store(stable);
    }

    gNote.store(-1);
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

extern "C" void MicOcarina_Poll(void) {
    gLastPoll.store(SDL_GetTicks());
    if (!gRun) {
        StartInternal();
    }
}

extern "C" int MicOcarina_GetNote(void) {
    return gNote.load();
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
}
