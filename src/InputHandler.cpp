#include "InputHandler.h"
#include <chrono>
#include <cstdio>
#include <windows.h>
#include <fstream>

HHOOK InputHandler::s_keyboardHook = nullptr;
InputHandler* InputHandler::s_instance = nullptr;

InputHandler::InputHandler(HWND hwnd)
    : m_hwnd(hwnd), m_binding(false), m_jumpKey(0), m_crouchKey(0), m_waitingForJump(false), m_waitingForCrouch(false)
{
    s_instance = this;
    s_keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, nullptr, 0);
}

InputHandler::~InputHandler()
{
    if (s_keyboardHook) UnhookWindowsHookEx(s_keyboardHook);
    s_keyboardHook = nullptr;
    s_instance = nullptr;
}

void InputHandler::BeginKeyBinding()
{
    m_binding = true;
    m_waitingForJump = true;
    m_waitingForCrouch = false;
    m_jumpKey = 0;
    m_crouchKey = 0;
}

bool InputHandler::IsBinding() const { return m_binding; }
WPARAM InputHandler::GetJumpKey() const { return m_jumpKey; }
WPARAM InputHandler::GetCrouchKey() const { return m_crouchKey; }

void InputHandler::Update() {
    // No-op for now; could be used for polling in future
}

bool InputHandler::PopEvent(KeyEvent& evt) {
    std::lock_guard<std::mutex> lock(m_queueMutex);
    if (m_eventQueue.empty()) return false;
    evt = m_eventQueue.front();
    m_eventQueue.pop();
    return true;
}

void InputHandler::HandleKey(WPARAM vkCode, bool pressed) {
    using namespace std::chrono;
    auto now = high_resolution_clock::now();
    ULONGLONG sysTime = GetTickCount64();
    // Log every key event
    {
        std::ofstream log("input_debug.log", std::ios::app);
        log << "vkCode: " << vkCode << ", pressed: " << pressed << ", sysTime: " << sysTime << std::endl;
    }
    if (m_binding) {
        if (m_waitingForJump && pressed) {
            m_jumpKey = vkCode;
            m_waitingForJump = false;
            m_waitingForCrouch = true;
        } else if (m_waitingForCrouch && pressed) {
            if (vkCode != m_jumpKey) {
                m_crouchKey = vkCode;
                m_binding = false;
                m_waitingForCrouch = false;
            }
        }
        return;
    }
    // Only push jump/crouch events
    if (vkCode == m_jumpKey) {
        if (pressed) {
            if (!m_jumpKeyHeld) {
                m_jumpKeyHeld = true;
                std::lock_guard<std::mutex> lock(m_queueMutex);
                m_eventQueue.push({ vkCode, pressed, now });
            }
        } else {
            m_jumpKeyHeld = false;
        }
    } else if (vkCode == m_crouchKey) {
        if (pressed) {
            if (!m_crouchKeyHeld) {
                m_crouchKeyHeld = true;
                std::lock_guard<std::mutex> lock(m_queueMutex);
                m_eventQueue.push({ vkCode, pressed, now });
            }
        } else {
            m_crouchKeyHeld = false;
        }
    }
}

LRESULT CALLBACK InputHandler::LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && s_instance) {
        KBDLLHOOKSTRUCT* p = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        bool pressed = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
        s_instance->HandleKey(p->vkCode, pressed);
    }
    return CallNextHookEx(s_keyboardHook, nCode, wParam, lParam);
} 