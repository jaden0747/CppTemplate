#pragma once

// ---------------------------------------------------------------------------
// lessons.hpp — entry points for each tutorial lesson, plus the table
// main.cpp's picker menu walks. Each RunLessonNN() owns its own
// ScreenInteractive and event loop; it returns once that lesson's own
// exit control (a "Back to menu" button/binding) is triggered.
// ---------------------------------------------------------------------------

#include <string>
#include <vector>

namespace tutorial
{

void RunLesson01();
void RunLesson02();
void RunLesson03();
void RunLesson04();
void RunLesson05();
void RunLesson06();
void RunLesson07();
void RunLesson08();
void RunLesson09();
void RunLesson10();

struct Lesson
{
    std::string title;
    void (*run)();
};

inline const std::vector<Lesson>& Lessons()
{
    static const std::vector<Lesson> lessons = {
        {"01. Hello World & the render loop", RunLesson01},
        {"02. Layout & styling", RunLesson02},
        {"03. Built-in components", RunLesson03},
        {"04. Events & focus", RunLesson04},
        {"05. Custom components", RunLesson05},
        {"06. Cross-thread updates (dc:: ports)", RunLesson06},
        {"07. Settings-driven panel (SettingsRegistry)", RunLesson07},
        {"08. Terminal log panel (Log::addSink)", RunLesson08},
        {"09. Capstone - tabs", RunLesson09},
        {"10. Capstone - resizable split", RunLesson10},
    };
    return lessons;
}

} // namespace tutorial
