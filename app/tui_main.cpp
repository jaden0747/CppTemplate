// ---------------------------------------------------------------------------
// tui_main.cpp — Vendored from FTXUI's own component gallery example
// (examples/component/gallery.cpp @ tag v5.0.0, matching the ftxui/5.0.0
// Conan package this project builds against), to demonstrate what the
// library can do rather than reimplement a settings editor. See
// https://github.com/ArthurSonzogni/FTXUI for the original.
// ---------------------------------------------------------------------------
// Copyright 2020 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include <atomic>      // for atomic
#include <chrono>      // for milliseconds, sleep_for
#include <functional>  // for function
#include <memory>      // for shared_ptr, allocator, __shared_ptr_access
#include <string>      // for string, basic_string
#include <thread>      // for thread
#include <vector>      // for vector

#include "ftxui/component/captured_mouse.hpp"  // for ftxui
#include "ftxui/component/component.hpp"  // for Slider, Checkbox, Vertical, Renderer, Button, Input, Menu, Radiobox, Toggle
#include "ftxui/component/component_base.hpp"  // for ComponentBase
#include "ftxui/component/screen_interactive.hpp"  // for Component, ScreenInteractive
#include "ftxui/dom/elements.hpp"  // for separator, operator|, Element, size, xflex, text, WIDTH, hbox, vbox, EQUAL, border, GREATER_THAN

using namespace ftxui;

// Display a component nicely with a title on the left.
Component Wrap(std::string name, Component component) {
  return Renderer(component, [name, component] {
    return hbox({
               text(name) | size(WIDTH, EQUAL, 8),
               separator(),
               component->Render() | xflex,
           }) |
           xflex;
  });
}

int main() {
  auto screen = ScreenInteractive::FitComponent();

  // -- Menu
  // ----------------------------------------------------------------------
  const std::vector<std::string> menu_entries = {
      "Menu 1",
      "Menu 2",
      "Menu 3",
      "Menu 4",
  };
  int menu_selected = 0;
  auto menu = Menu(&menu_entries, &menu_selected);
  menu = Wrap("Menu", menu);

  // -- Toggle------------------------------------------------------------------
  int toggle_selected = 0;
  std::vector<std::string> toggle_entries = {
      "Toggle_1",
      "Toggle_2",
  };
  auto toggle = Toggle(&toggle_entries, &toggle_selected);
  toggle = Wrap("Toggle", toggle);

  // -- Checkbox ---------------------------------------------------------------
  bool checkbox_1_selected = false;
  bool checkbox_2_selected = false;
  bool checkbox_3_selected = false;
  bool checkbox_4_selected = false;

  auto checkboxes = Container::Vertical({
      Checkbox("checkbox1", &checkbox_1_selected),
      Checkbox("checkbox2", &checkbox_2_selected),
      Checkbox("checkbox3", &checkbox_3_selected),
      Checkbox("checkbox4", &checkbox_4_selected),
  });
  checkboxes = Wrap("Checkbox", checkboxes);

  // -- Radiobox ---------------------------------------------------------------
  int radiobox_selected = 0;
  std::vector<std::string> radiobox_entries = {
      "Radiobox 1",
      "Radiobox 2",
      "Radiobox 3",
      "Radiobox 4",
  };
  auto radiobox = Radiobox(&radiobox_entries, &radiobox_selected);
  radiobox = Wrap("Radiobox", radiobox);

  // -- Input ------------------------------------------------------------------
  std::string input_label;
  auto input = Input(&input_label, "placeholder");
  input = Wrap("Input", input);

  // -- Button -----------------------------------------------------------------
  std::string button_label = "Quit";
  std::function<void()> on_button_clicked_;
  auto button = Button(&button_label, screen.ExitLoopClosure());
  button = Wrap("Button", button);

  // -- Slider -----------------------------------------------------------------
  int slider_value_1 = 12;
  int slider_value_2 = 56;
  int slider_value_3 = 128;
  auto sliders = Container::Vertical({
      Slider("R:", &slider_value_1, 0, 256, 1),
      Slider("G:", &slider_value_2, 0, 256, 1),
      Slider("B:", &slider_value_3, 0, 256, 1),
  });
  sliders = Wrap("Slider", sliders);

  // -- Layout -----------------------------------------------------------------
  auto layout = Container::Vertical({
      menu,
      toggle,
      checkboxes,
      radiobox,
      input,
      sliders,
      button,
  });

  // -- Loop counter -----------------------------------------------------------
  // Render at a fixed 30 fps and bump the counter exactly 30 times per second.
  constexpr int kFps = 30;
  constexpr auto kFrameDuration = std::chrono::milliseconds(1000 / kFps);
  std::atomic<bool> running{true};
  int loop_count = 0;

  // A timer thread posts a custom event every frame; the main loop wakes up on
  // each event, re-renders, and increments the counter.
  std::thread timer_thread([&] {
    while (running.load()) {
      screen.PostEvent(Event::Custom);
      std::this_thread::sleep_for(kFrameDuration);
    }
  });

  auto component = Renderer(layout, [&] {
    ++loop_count;
    return vbox({
               text("Loop count: " + std::to_string(loop_count)) | bold,
               separator(),
               menu->Render(),
               separator(),
               toggle->Render(),
               separator(),
               checkboxes->Render(),
               separator(),
               radiobox->Render(),
               separator(),
               input->Render(),
               separator(),
               sliders->Render(),
               separator(),
               button->Render(),
           }) |
           xflex | size(WIDTH, GREATER_THAN, 40) | border;
  });

  screen.Loop(component);

  running.store(false);
  timer_thread.join();

  return 0;
}
