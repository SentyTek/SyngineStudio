// ╒════════════════════════ Process.cpp ═╕
// │ Syngine Studio                       │
// │ Created 2026-09-07                   │
// ├──────────────────────────────────────┤
// │ Copyright (c) SentyTek 2025-2026     │
// │ Licensed under the MIT License       │
// ╰──────────────────────────────────────╯

#include "Process.hpp"

namespace SynEditor {

void Process::Start(std::string command, std::string workingDirectory) {
    m_output.clear();
    m_process = std::make_unique<TinyProcessLib::Process>(
        command,
        workingDirectory,
        [this](const char* bytes, size_t n) { m_output.append(bytes, n); },
        [this](const char* bytes, size_t n) { m_output.append(bytes, n); });
}

void Process::Kill() {
    if (m_process) {
        m_process->kill();
    }
    m_process.reset();
}

bool Process::IsRunning() const {
    if (!m_process) {
        return false;
    }
    int exitStatus = 0;
    return !m_process->try_get_exit_status(exitStatus);
}

int Process::GetExitCode() const {
    if (!m_process) {
        return -1;
    }
    int exitStatus = 0;
    m_process->try_get_exit_status(exitStatus);
    return exitStatus;
}

void Process::Wait() {
    if (m_process) {
        m_process->get_exit_status();
    }
}

const std::string& Process::Output() const { return m_output; }

} // namespace SynEditor
