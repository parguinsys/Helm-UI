\
#include "Pages.h"
#include <cstring>

// -------- label → action map --------
Action mapLabelToAction(const char* s) {
  if (!s || !*s) return Action::NONE;
  if (!strcmp(s, "BTN_LEFT"))     return Action::LEFT;
  if (!strcmp(s, "BTN_RIGHT"))    return Action::RIGHT;
  if (!strcmp(s, "BTN_UP"))       return Action::UP;
  if (!strcmp(s, "BTN_DOWN"))     return Action::DOWN;
  if (!strcmp(s, "BTN_ENTER"))    return Action::ENTER;
  if (!strcmp(s, "BTN_CANCEL"))   return Action::CANCEL;
  if (!strcmp(s, "BTN_SETTINGS")) return Action::ENTER;
  if (!strcmp(s, "BTN_FX"))       return Action::ENTER;
  return Action::NONE;
}

// -------- PageManager --------
void PageManager::begin(Display_SSD1322* disp) { _d = disp; }
uint8_t PageManager::add(IPage* p) {
  if (_count < (uint8_t)(sizeof(_pages)/sizeof(_pages[0]))) {
    _pages[_count] = p;
    return _count++;
  }
  return 0;
}
void PageManager::bind(const char* label, uint8_t pageIndex) {
  if (_bindCount < (uint8_t)(sizeof(_binds)/sizeof(_binds[0]))) {
    _binds[_bindCount++] = Bind{label, pageIndex};
  }
}
void PageManager::show(uint8_t index) {
  if (_count == 0) return;
  if (index >= _count) index = 0;
  _idx = index;
  render();
}
void PageManager::next() { if (_count) { _idx = (uint8_t)((_idx + 1) % _count); render(); } }
void PageManager::prev() { if (_count) { _idx = (uint8_t)((_idx + _count - 1) % _count); render(); } }

void PageManager::render() {
  if (!_d || !_count) return;
  _d->clear();
  _d->printCenter(_pages[_idx]->title());
  _pages[_idx]->render(*_d);
  _d->present();  // flush once per render
}

void PageManager::onKeyLabel(const char* label) {
  if (!_d || !_count) return;

  // 1) hotkeys: label → page
  for (uint8_t i=0; i<_bindCount; ++i) {
    if (_binds[i].label && !strcmp(_binds[i].label, label)) {
      show(_binds[i].page);
      return;
    }
  }

  // 2) fallback to actions
  Action a = mapLabelToAction(label);
  if (a == Action::LEFT)  { prev();  return; }
  if (a == Action::RIGHT) { next();  return; }
  _pages[_idx]->onAction(a, *_d);
}

// -------- HomePage --------
void HomePage::setLastLabel(const char* s) {
  if (!s) { last_[0] = 0; return; }
  strncpy(last_, s, sizeof(last_) - 1);
  last_[sizeof(last_)-1] = 0;
}
void HomePage::render(Display_SSD1322& d) {
  d.printLine("LEFT/RIGHT: switch page");
  d.printLine("Press a section key to jump");
}
void HomePage::onAction(Action a, Display_SSD1322& d) {
  switch (a) {
    case Action::ENTER:  d.printLine("Enter pressed"); break;
    case Action::CANCEL: d.printLine("Cancel pressed"); break;
    default: break;
  }
}

// -------- SettingsPage --------
void SettingsPage::render(Display_SSD1322& d) {
  for (uint8_t i = 0; i < 3; ++i) {
    char line[32];
    snprintf(line, sizeof(line), "%c %s", (i == sel_) ? '>' : ' ', items_[i]);
    d.printLine(line);
  }
}
void SettingsPage::onAction(Action a, Display_SSD1322& d) {
  switch (a) {
    case Action::UP:   if (sel_ > 0) sel_--; break;
    case Action::DOWN: if (sel_ < 2) sel_++; break;
    case Action::ENTER:
      if (sel_ == 1) items_[1] = (!strcmp(items_[1], "FX: Off")) ? "FX: On" : "FX: Off";
      break;
    case Action::CANCEL: /* ignore */ break;
    default: break;
  }
  d.clear();
  d.printCenter(title());
  render(d);
  d.present();
}

// -------- TitlePage (stub) --------
void TitlePage::render(Display_SSD1322& d) {
  d.printLine("Page under construction");
  d.printLine("Use LEFT/RIGHT to navigate");
}
