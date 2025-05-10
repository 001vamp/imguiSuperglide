#include "TrainerLogic.h"
#include "imgui.h"
#include <sstream>
#include <iomanip>

TrainerLogic::TrainerLogic()
    : m_state(State::Ready), m_attempt(0), m_cumulativePercent(0.0), m_lastDeltaMs(0.0), m_lastDeltaFrames(0.0), m_lastChance(0.0), m_feedback(""), m_targetFPS(0.0), m_frameTime(0.0), m_awaitingFPS(true)
{
}

void TrainerLogic::Update(InputHandler& input) {
    if (m_awaitingFPS) return;
    KeyEvent evt;
    while (input.PopEvent(evt)) {
        if (m_state == State::Ready) {
            if (evt.vkCode == input.GetJumpKey()) {
                m_jumpTime = evt.timestamp;
                m_state = State::Jump;
                m_feedback = "Awaiting Crouch...";
            }
        } else if (m_state == State::Jump) {
            if (evt.vkCode == input.GetCrouchKey()) {
                m_crouchTime = evt.timestamp;
                auto delta = std::chrono::duration<double, std::milli>(m_crouchTime - m_jumpTime).count();
                m_lastDeltaMs = delta;
                m_lastDeltaFrames = delta / (m_frameTime * 1000.0);
                // Calculate chance %
                if (m_lastDeltaFrames < 1.0) {
                    m_lastChance = m_lastDeltaFrames * 100.0;
                    m_feedback = "Crouch slightly later to improve.";
                } else if (m_lastDeltaFrames < 2.0) {
                    m_lastChance = (2.0 - m_lastDeltaFrames) * 100.0;
                    m_feedback = "Crouch slightly sooner to improve.";
                } else {
                    m_lastChance = 0.0;
                    m_feedback = "Crouched too late.";
                }
                m_attempt++;
                m_cumulativePercent += m_lastChance;
                m_state = State::Ready;
            } else if (evt.vkCode == input.GetJumpKey()) {
                m_state = State::JumpWarned;
                m_feedback = "Warning: Multiple jumps detected, results may not reflect ingame behavior.";
            }
        } else if (m_state == State::JumpWarned) {
            if (evt.vkCode == input.GetCrouchKey()) {
                m_crouchTime = evt.timestamp;
                m_lastDeltaMs = std::chrono::duration<double, std::milli>(m_crouchTime - m_jumpTime).count();
                m_lastDeltaFrames = m_lastDeltaMs / (m_frameTime * 1000.0);
                m_lastChance = 0.0;
                m_feedback = "Double jump input, resetting.";
                m_state = State::Ready;
            }
        }
    }
}

void TrainerLogic::RenderUI() {
    ImGui::SetNextWindowBgAlpha(0.8f);
    ImGui::Begin("Superglide Trainer", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    if (m_awaitingFPS) {
        static char fpsBuf[16] = "";
        ImGui::Text("Enter your target FPS (e.g. 144):");
        ImGui::InputText("##fps", fpsBuf, sizeof(fpsBuf), ImGuiInputTextFlags_CharsDecimal);
        if (ImGui::Button("Set FPS") && atof(fpsBuf) > 0.0) {
            m_targetFPS = atof(fpsBuf);
            m_frameTime = 1.0 / m_targetFPS;
            m_awaitingFPS = false;
        }
        ImGui::End();
        return;
    }
    ImGui::Text("Attempt: %d", m_attempt);
    ImGui::Text("Frame time: %.3f ms (%.2f FPS)", m_frameTime * 1000.0, m_targetFPS);
    ImGui::Separator();
    ImGui::Text("Last delay: %.2f ms (%.2f frames)", m_lastDeltaMs, m_lastDeltaFrames);
    ImGui::Text("Chance: %.1f%%", m_lastChance);
    ImGui::Text("Feedback: %s", m_feedback.c_str());
    if (m_attempt > 0)
        ImGui::Text("Average chance: %.1f%%", m_cumulativePercent / m_attempt);
    ImGui::ProgressBar((float)(m_lastDeltaFrames / 2.0), ImVec2(200, 20));
    ImGui::End();
}

std::string TrainerLogic::KeyName(WPARAM vk) const {
    char name[128] = {0};
    if (GetKeyNameTextA(MapVirtualKeyA((UINT)vk, 0) << 16, name, sizeof(name)))
        return name;
    std::ostringstream oss;
    oss << "VK_" << std::hex << vk;
    return oss.str();
} 