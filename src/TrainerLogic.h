#pragma once
#include <chrono>
#include <string>
#include "InputHandler.h"

class TrainerLogic {
public:
    TrainerLogic();
    void Update(InputHandler& input);
    void RenderUI();

    int GetAttempt() const { return m_attempt; }
    double GetFrameTime() const { return m_frameTime; }
    double GetTargetFPS() const { return m_targetFPS; }
    double GetLastDeltaMs() const { return m_lastDeltaMs; }
    double GetLastDeltaFrames() const { return m_lastDeltaFrames; }
    double GetLastChance() const { return m_lastChance; }
    const std::string& GetFeedback() const { return m_feedback; }
    double GetCumulativePercent() const { return m_cumulativePercent; }
    bool IsAwaitingFPS() const { return m_awaitingFPS; }
    void SetTargetFPS(double fps) {
        m_targetFPS = fps;
        m_frameTime = 1.0 / m_targetFPS;
        m_awaitingFPS = false;
    }

    enum class State {
        Ready,
        Jump,
        JumpWarned,
        Crouch
    };

private:
    State m_state;
    std::chrono::high_resolution_clock::time_point m_jumpTime;
    std::chrono::high_resolution_clock::time_point m_crouchTime;
    int m_attempt;
    double m_cumulativePercent;
    double m_lastDeltaMs;
    double m_lastDeltaFrames;
    double m_lastChance;
    std::string m_feedback;
    double m_targetFPS;
    double m_frameTime;
    bool m_awaitingFPS;

    // Key display
    std::string KeyName(WPARAM vk) const;
}; 