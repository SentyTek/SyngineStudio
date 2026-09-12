// ╒════════════════════════ Process.hpp ═╕
// │ Syngine Studio                       │
// │ Created 2026-09-07                   │
// ├──────────────────────────────────────┤
// │ Copyright (c) SentyTek 2025-2026     │
// │ Licensed under the MIT License       │
// ╰──────────────────────────────────────╯
#pragma once
#include <string>
#include <memory>

#include "../lib/tpl/process.hpp"

namespace SynEditor {
class Process {
    std::unique_ptr<TinyProcessLib::Process> m_process;
    std::string                              m_output;

  public:
    void Start(std::string command, std::string workingDirectory = "");
    void Kill();

    bool IsRunning() const;
    int  GetExitCode() const;
    void Wait();

    const std::string& Output() const;
};

} // namespace SynEditor
