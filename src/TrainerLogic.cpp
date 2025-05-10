#include "TrainerLogic.h"
#include "imgui.h"
#include <sstream>
#include <iomanip>
#include <fstream>

TrainerLogic::TrainerLogic()
    : m_state(State::Ready), m_attempt(0), m_cumulativePercent(0.0), m_lastDeltaMs(0.0), m_lastDeltaFrames(0.0), m_lastChance(0.0), m_feedback(""), m_targetFPS(0.0), m_frameTime(0.0), m_awaitingFPS(true), m_ignoreNextJump(true)
{
}

void TrainerLogic::Update(InputHandler& input) {
    if (m_awaitingFPS) return;
    KeyEvent evt;
    while (input.PopEvent(evt)) {
        // Print debug info for event processing
        using namespace std::chrono;
        ULONGLONG sysTime = GetTickCount64();
        auto now = high_resolution_clock::now();
        // std::ofstream log("debug_log.txt", std::ios::app);
        if (m_state == State::Ready) {
            if (evt.vkCode == input.GetJumpKey()) {
                if (m_ignoreNextJump) {
                    // Ignore the first jump (starts the climb)
                    m_ignoreNextJump = false;
                    m_feedback = "Climb started. Now attempt the superglide!";
                    continue;
                }
                m_jumpTime = evt.timestamp;
                m_state = State::Jump;
                m_feedback = "Awaiting Crouch...";
            } else if (evt.vkCode == input.GetCrouchKey()) {
                // If crouch is pressed while in Ready, check if jump is also held
                if (input.GetJumpKey() && input.IsJumpKeyHeld()) {
                    m_feedback = "Jump and Crouch pressed together! Try to press jump first, then crouch.";
                } else {
                    m_feedback = "Crouch pressed before jump. Try to press jump first, then crouch.";
                }
            }
        } else if (m_state == State::Jump) {
            if (evt.vkCode == input.GetCrouchKey()) {
                m_crouchTime = evt.timestamp;
                auto delta = std::chrono::duration<double>(m_crouchTime - m_jumpTime).count(); // seconds
                m_lastDeltaMs = delta * 1000.0;
                m_lastDeltaFrames = delta / m_frameTime;
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(6);
                double chance = 0.0;
                if (m_lastDeltaFrames < 1.0) {
                    chance = m_lastDeltaFrames * 100.0;
                } else if (m_lastDeltaFrames < 2.0) {
                    chance = (2.0 - m_lastDeltaFrames) * 100.0;
                }
                // Calculate chance and feedback
                if (m_lastDeltaFrames < 1.0) {
                    m_lastChance = m_lastDeltaFrames * 100.0;
                    double difference = m_frameTime - delta; // positive if crouch was too early
                    oss << std::setprecision(5) << "Crouch slightly *later* by " << difference << " seconds to improve.";
                } else if (m_lastDeltaFrames < 2.0) {
                    m_lastChance = (2.0 - m_lastDeltaFrames) * 100.0;
                    double difference = delta - m_frameTime; // positive if crouch was too late
                    oss << std::setprecision(5) << "Crouch slightly *sooner* by " << difference << " seconds to improve.";
                } else {
                    m_lastChance = 0.0;
                    double difference = delta - m_frameTime;
                    oss << std::setprecision(5) << "Crouched too late by " << difference << " seconds.";
                }
                m_feedback = oss.str();
                m_attempt++;
                m_cumulativePercent += m_lastChance;
                m_state = State::Ready;
                m_ignoreNextJump = true;
            } else if (evt.vkCode == input.GetJumpKey()) {
                m_state = State::JumpWarned;
                m_feedback = "Warning: Multiple jumps detected, results may not reflect ingame behavior.";
            }
        } else if (m_state == State::JumpWarned) {
            if (evt.vkCode == input.GetCrouchKey()) {
                m_crouchTime = evt.timestamp;
                m_lastDeltaMs = std::chrono::duration<double>(m_crouchTime - m_jumpTime).count() * 1000.0;
                m_lastDeltaFrames = m_lastDeltaMs / (m_frameTime * 1000.0);
                m_lastChance = 0.0;
                m_feedback = "Double jump input, resetting.";
                m_state = State::Ready;
                m_ignoreNextJump = true;
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
            m_ignoreNextJump = true;
        }
        ImGui::End();
        return;
    }
    ImGui::Text("Attempt: %d", m_attempt);
    ImGui::Text("Frame time: %.3f ms (%.2f FPS)", m_frameTime * 1000.0, m_targetFPS);
    ImGui::Separator();
    ImGui::Text("Last delay: %.6f ms (%.6f frames)", m_lastDeltaMs, m_lastDeltaFrames);
    // Show chance with color
    if (m_lastChance > 0.0)
        ImGui::TextColored(ImVec4(0,1,0,1), "Chance to hit: %.6f%%", m_lastChance);
    else
        ImGui::TextColored(ImVec4(1,0,0,1), "Chance to hit: 0%%");
    // Show feedback (frames passed, sooner/later, etc)
    ImGui::TextWrapped("%s", m_feedback.c_str());
    if (m_attempt > 0)
        ImGui::Text("Average: %.1f%%", m_cumulativePercent / m_attempt);
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

void TrainerLogic::SetTargetFPS(double fps) {
    m_targetFPS = fps;
    m_frameTime = 1.0 / m_targetFPS;
    m_awaitingFPS = false;
    m_ignoreNextJump = true;
} 