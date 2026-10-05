#pragma once
class MappedInputManager {
 public:
  enum class Button { Back, Confirm, Left, Right, Up, Down, Power, PageBack, PageForward };
  struct Labels { const char* btn1; const char* btn2; const char* btn3; const char* btn4; };
  bool wasPressed(Button) const { return false; }
  bool wasReleased(Button) const { return false; }
  bool isPressed(Button) const { return false; }
  Labels mapLabels(const char* a, const char* b, const char* c, const char* d) const { return {a, b, c, d}; }
};
