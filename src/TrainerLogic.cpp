#include "TrainerLogic.h"
#include "imgui.h"
#include <sstream>
#include <iomanip>
#include <fstream>
#include <deque>

// For on-screen debug overlay
static std::deque<std::string> g_debugLog;
static void AddDebugLog(const std::string& msg) {
    g_debugLog.push_back(msg);
    if (g_debugLog.size() > 10) g_debugLog.pop_front();
}
void RenderDebugOverlay() {
    ImGui::SetNextWindowBgAlpha(0.7f);
    if (ImGui::Begin("Debug Log", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse)) {
        for (const auto& line : g_debugLog) {
            ImGui::TextWrapped("%s", line.c_str());
        }
    }
    ImGui::End();
}

static std::chrono::high_resolution_clock::time_point g_lastCrouchTime;
static bool g_crouchBeforeJump = false;

TrainerLogic::TrainerLogic()
    : m_state(State::Ready), m_attempt(0), m_cumulativePercent(0.0), m_lastDeltaMs(0.0), m_lastDeltaFrames(0.0), m_lastChance(0.0), m_feedback(""), m_targetFPS(0.0), m_frameTime(0.0), m_awaitingFPS(true), m_ignoreNextJump(true), m_superglideCount(0)
{
}

void TrainerLogic::Update(InputHandler& input) {
    if (m_awaitingFPS) return;
    KeyEvent evt;
    while (input.PopEvent(evt)) {
        using namespace std::chrono;
        ULONGLONG sysTime = GetTickCount64();
        auto now = high_resolution_clock::now();
        // Log every event
        {
            std::ofstream log("build/logic_debug.log", std::ios::app);
            log << "State: " << (int)m_state << ", vkCode: " << evt.vkCode << ", pressed: " << evt.pressed << ", sysTime: " << sysTime << std::endl;
        }
        std::ostringstream dbg;
        dbg << "State: " << (int)m_state << ", vkCode: " << evt.vkCode << ", pressed: " << evt.pressed << ", sysTime: " << sysTime;
        AddDebugLog(dbg.str());
        if (m_state == State::Ready) {
            if (evt.vkCode == input.GetCrouchKey()) {
                g_lastCrouchTime = evt.timestamp;
                g_crouchBeforeJump = true;
                m_feedback = "Crouch pressed before jump. Try to press jump first, then crouch.";
                // Log feedback
                {
                    std::ofstream log("build/logic_debug.log", std::ios::app);
                    log << "Feedback: " << m_feedback << std::endl;
                }
                AddDebugLog(m_feedback);
                // Immediately reset state so user can try again
                m_state = State::Ready;
                g_crouchBeforeJump = false;
            } else if (evt.vkCode == input.GetJumpKey()) {
                if (m_ignoreNextJump) {
                    m_ignoreNextJump = false;
                    m_feedback = "Climb started. Now attempt the superglide!";
                    // Log feedback
                    {
                        std::ofstream log("build/logic_debug.log", std::ios::app);
                        log << "Feedback: " << m_feedback << std::endl;
                    }
                    AddDebugLog(m_feedback);
                    continue;
                }
                // If crouch was pressed before jump and is still held, show timing feedback
                if (g_crouchBeforeJump && input.IsCrouchKeyHeld()) {
                    auto delta = std::chrono::duration<double>(evt.timestamp - g_lastCrouchTime).count();
                    int ms = static_cast<int>(delta * 1000.0);
                    m_feedback = std::string("Crouched ") + std::to_string(ms) + " ms before jump. Try to press jump first, then crouch.";
                    // Log feedback
                    {
                        std::ofstream log("build/logic_debug.log", std::ios::app);
                        log << "Feedback: " << m_feedback << std::endl;
                    }
                    AddDebugLog(m_feedback);
                    g_crouchBeforeJump = false;
                    // Immediately reset state so user can try again
                    m_state = State::Ready;
                    continue;
                }
                g_crouchBeforeJump = false;
                m_jumpTime = evt.timestamp;
                m_state = State::Jump;
                m_feedback = "Awaiting Crouch...";
                // Log feedback
                {
                    std::ofstream log("build/logic_debug.log", std::ios::app);
                    log << "Feedback: " << m_feedback << std::endl;
                }
                AddDebugLog(m_feedback);
                // If crouch is already held, process crouch immediately
                if (input.IsCrouchKeyHeld()) {
                    m_crouchTime = evt.timestamp;
                    auto delta = std::chrono::duration<double>(m_crouchTime - m_jumpTime).count();
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
                    if (m_lastDeltaFrames < 1.0) {
                        m_lastChance = m_lastDeltaFrames * 100.0;
                        double difference = m_frameTime - delta;
                        oss << std::setprecision(5) << "Crouch slightly *later* by " << difference << " seconds to improve.";
                    } else if (m_lastDeltaFrames < 2.0) {
                        m_lastChance = (2.0 - m_lastDeltaFrames) * 100.0;
                        double difference = delta - m_frameTime;
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
                    m_superglideCount++;
                    // Log feedback
                    {
                        std::ofstream log("build/logic_debug.log", std::ios::app);
                        log << "Feedback: " << m_feedback << std::endl;
                    }
                    AddDebugLog(m_feedback);
                }
            }
        } else if (m_state == State::Jump) {
            if (evt.vkCode == input.GetCrouchKey()) {
                m_crouchTime = evt.timestamp;
                auto delta = std::chrono::duration<double>(m_crouchTime - m_jumpTime).count();
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
                if (m_lastDeltaFrames < 1.0) {
                    m_lastChance = m_lastDeltaFrames * 100.0;
                    double difference = m_frameTime - delta;
                    oss << std::setprecision(5) << "Crouch slightly *later* by " << difference << " seconds to improve.";
                } else if (m_lastDeltaFrames < 2.0) {
                    m_lastChance = (2.0 - m_lastDeltaFrames) * 100.0;
                    double difference = delta - m_frameTime;
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
                m_superglideCount++;
                // Log feedback
                {
                    std::ofstream log("build/logic_debug.log", std::ios::app);
                    log << "Feedback: " << m_feedback << std::endl;
                }
                AddDebugLog(m_feedback);
            } else if (evt.vkCode == input.GetJumpKey()) {
                m_state = State::JumpWarned;
                m_feedback = "Warning: Multiple jumps detected, results may not reflect ingame behavior.";
                // Log feedback
                {
                    std::ofstream log("build/logic_debug.log", std::ios::app);
                    log << "Feedback: " << m_feedback << std::endl;
                }
                AddDebugLog(m_feedback);
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
                // Log feedback
                {
                    std::ofstream log("build/logic_debug.log", std::ios::app);
                    log << "Feedback: " << m_feedback << std::endl;
                }
                AddDebugLog(m_feedback);
            }
        }
    }
}

void TrainerLogic::RenderUI() {
    ImGui::SetNextWindowBgAlpha(0.8f);
    ImGui::Begin("Superglide Trainer", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("Superglide Count: %d", m_superglideCount);
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