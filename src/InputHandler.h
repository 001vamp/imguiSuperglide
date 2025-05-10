#pragma once
#include <windows.h>
#include <functional>
#include <mutex>
#include <queue>
#include <atomic>

// Key event type
struct KeyEvent {
    WPARAM vkCode;
    bool pressed;
    std::chrono::high_resolution_clock::time_point timestamp;
};

class InputHandler {
public:
    InputHandler(HWND hwnd);
    ~InputHandler();

    // Call every frame to process events
    void Update();

    // Key binding
    void BeginKeyBinding();
    bool IsBinding() const;
    WPARAM GetJumpKey() const;
    WPARAM GetCrouchKey() const;

    // Event queue
    bool PopEvent(KeyEvent& evt);

    bool IsJumpKeyHeld() const { return m_jumpKeyHeld; }
    bool IsCrouchKeyHeld() const { return m_crouchKeyHeld; }

private:
    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
    static HHOOK s_keyboardHook;
    static InputHandler* s_instance;

    HWND m_hwnd;
    std::atomic<bool> m_binding;
    WPARAM m_jumpKey;
    WPARAM m_crouchKey;
    bool m_waitingForJump;
    bool m_waitingForCrouch;

    std::mutex m_queueMutex;
    std::queue<KeyEvent> m_eventQueue;

    // Track held state to suppress repeated keydown events
    bool m_jumpKeyHeld = false;
    bool m_crouchKeyHeld = false;

    void HandleKey(WPARAM vkCode, bool pressed);
}; 