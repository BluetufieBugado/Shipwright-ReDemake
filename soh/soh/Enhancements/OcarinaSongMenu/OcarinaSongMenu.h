#pragma once

#include <ship/window/gui/GuiWindow.h>

// Menu de músicas da Ocarina, aberto/fechado com D-Pad para baixo enquanto a Ocarina está em uso.
// Sobrescreve Draw() para NÃO desenhar a moldura padrão de janela (mesmo padrão do InputViewer).
class OcarinaSongMenuWindow final : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;

    void Draw() override;
    void InitElement() override{};
    void DrawElement() override;
    void UpdateElement() override{};
};
