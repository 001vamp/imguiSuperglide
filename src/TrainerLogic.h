#pragma once
#include <chrono>
#include <string>
#include "InputHandler.h"

class TrainerLogic {
public:
    TrainerLogic();
    void Update(InputHandler& input);
    void RenderUI();

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