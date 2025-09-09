\
#pragma once
#include <Arduino.h>
#include "Display_SSD1322.h"

// Actions we care about (triggered on key-down)
enum class Action : uint8_t {
  NONE, LEFT, RIGHT, UP, DOWN, ENTER, CANCEL
};

// Map your button labels to actions (others = NONE)
Action mapLabelToAction(const char* label);

// Simple page interface
class IPage {
public:
  virtual ~IPage() = default;
  virtual const char* title() const = 0;
  virtual void render(Display_SSD1322& d) = 0;
  virtual void onAction(Action a, Display_SSD1322& d) { (void)a; (void)d; }
};

// Minimal PageManager for fixed pages
class PageManager {
public:
  void begin(Display_SSD1322* disp);
  uint8_t add(IPage* p);            // returns index
  void show(uint8_t index);         // switch page
  void next();                      // RIGHT
  void prev();                      // LEFT
  void onKeyLabel(const char* label);// call on key-down with your label
  void bind(const char* label, uint8_t pageIndex); // label → page

private:
  void render();
  Display_SSD1322* _d = nullptr;
  IPage* _pages[24] = {nullptr};
  uint8_t _count = 0;
  uint8_t _idx = 0;

  struct Bind { const char* label; uint8_t page; };
  Bind _binds[32] = {};
  uint8_t _bindCount = 0;
};

// Built-in example pages
class HomePage : public IPage {
public:
  const char* title() const override { return "Home"; }
  void render(Display_SSD1322& d) override;
  void onAction(Action a, Display_SSD1322& d) override;
  void setLastLabel(const char* s);
private:
  char last_[32] = "—";
};

class SettingsPage : public IPage {
public:
  const char* title() const override { return "Settings"; }
  void render(Display_SSD1322& d) override;
  void onAction(Action a, Display_SSD1322& d) override;
private:
  const char* items_[3] = {"Engine: A", "FX: Off", "Amp: 50%"};
  uint8_t sel_ = 0;
};

// Generic title-only stub page (for quick expansion)
class TitlePage : public IPage {
public:
  explicit TitlePage(const char* t) : t_(t) {}
  const char* title() const override { return t_; }
  void render(Display_SSD1322& d) override;
private:
  const char* t_;
};
