#pragma once

#include <libultraship/libultraship.h>

// Overlay inspirado no menu de músicas do remake de Ocarina of Time:
// botão fixo embaixo da tela, arco de bolinhas com as músicas e pergaminho com a partitura.
class OcarinaSongMenuWindow final : public Ship::GuiWindow {
  public:
    using Ship::GuiWindow::GuiWindow;
    ~OcarinaSongMenuWindow() override = default;

  protected:
    void InitElement() override;
    void DrawElement() override;
    void UpdateElement() override {
    }
};
