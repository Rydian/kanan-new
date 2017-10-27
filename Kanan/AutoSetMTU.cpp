#include <algorithm>

#include <Windows.h>

#include <imgui.h>

#include <Scan.hpp>
#include <String.hpp>

#include "Log.hpp"
#include "AutoSetMTU.hpp"

using namespace std;

namespace kanan {
    static AutoSetMTU* g_autoSetMTU{ nullptr };

    AutoSetMTU::AutoSetMTU()
        : m_isEnabled{ false },
        m_interface{ "" },
        m_lowMTU{ 0 },
        m_normalMTU{ 0 },
        m_hook{}
    {
        g_log << "Entering AutoSetMTU constructor." << endl;
        g_log << "Looking for connection address..." << endl;

        auto connectionAddress = scan("client.exe", "55 8B EC 83 EC 08 56 57 8B 7D 0C 8B CF");

        if (connectionAddress) {
            g_log << "Got connection address " << hex << *connectionAddress << endl;

            g_autoSetMTU = this;
            m_hook = make_unique<FunctionHook>(*connectionAddress, (uintptr_t)&AutoSetMTU::createConnection);

            if (m_hook->isValid()) {
                g_log << "Hooked the connection function." << endl;
            }
            else {
                g_log << "Failed to hook the connection function." << endl;
            }

        }
        else{
            g_log << "Failed to find connection address." << endl;
        }

        g_log << "Leaving AutoSetMTU constructor." << endl;
    }

    AutoSetMTU::~AutoSetMTU() {
        g_autoSetMTU = nullptr;
    }

    void AutoSetMTU::onUI() {
        if (ImGui::CollapsingHeader("Auto Set MTU")) {
            ImGui::Text("This mod automatically sets your MTU when you login or change channels.");
            ImGui::Text("This can result in a more responsive connection with the game server, reducing apparent lag.");
            ImGui::Dummy(ImVec2{ 10.0f, 10.0f });
            ImGui::Text("To see what your interfaces are called, open a command prompt and type:");
            ImGui::Text("    netsh interface ipv4 show config");
            ImGui::Dummy(ImVec2{ 10.0f, 10.0f });
            ImGui::Checkbox("Enable Auto Set MTU", &m_isEnabled);
            ImGui::InputText("Interface", m_interface.data(), m_interface.size());
            ImGui::SliderInt("Lowered MTU", &m_lowMTU, 0, 1500);
            ImGui::SliderInt("Normal MTU", &m_normalMTU, 0, 1500);
        }
    }

    void AutoSetMTU::onConfigLoad(ConfigPtr cfg) {
        m_isEnabled = cfg->get_qualified_as<bool>("AutoSetMTU.Enabled").value_or(false);
        auto interface1 = cfg->get_qualified_as<string>("AutoSetMTU.Interface").value_or("Ethernet");
        m_lowMTU = cfg->get_qualified_as<int>("AutoSetMTU.LoweredMTU").value_or(386);
        m_normalMTU = cfg->get_qualified_as<int>("AutoSetMTU.NormalMTU").value_or(1500);
        
        strcpy_s(m_interface.data(), m_interface.size(), interface1.c_str());
    }

    void AutoSetMTU::onConfigSave(ConfigPtr cfg) {
        auto tbl = cpptoml::make_table();

        tbl->insert("Enabled", m_isEnabled);
        tbl->insert("Interface", string{ m_interface.data() });
        tbl->insert("LoweredMTU", m_lowMTU);
        tbl->insert("NormalMTU", m_normalMTU);

        cfg->insert("AutoSetMTU", tbl);
    }

    optional<DWORD> AutoSetMTU::runProcess(const string& name, const string& params) {
        auto commandLine = widen(name + " " + params);
        STARTUPINFO startupInfo{ 0 };
        PROCESS_INFORMATION processInfo{ 0 };

        startupInfo.cb = sizeof(startupInfo);

        if (CreateProcess(nullptr, (LPWSTR)commandLine.c_str(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW | NORMAL_PRIORITY_CLASS, nullptr, nullptr, &startupInfo, &processInfo) == FALSE) {
            g_log << "Failed to CreateProcess for " << name << " " << params << endl;
            return {};
        }

        auto processHandle = processInfo.hProcess;
        DWORD exitCode = 0;

        if (WaitForSingleObject(processHandle, 5000) != WAIT_OBJECT_0) {
            g_log << "Failed to wait for " << name << " " << params << " to end." << endl;
            return {};
        }

        if (GetExitCodeProcess(processHandle, &exitCode) == FALSE) {
            g_log << "Failed to get the exit code for " << name << " " << params << endl;
            return {};
        }

        return exitCode;
    }

    char AutoSetMTU::createConnection(int a1, int a2) {
        auto mtu = g_autoSetMTU;

        // Lower the MTU if we are enabled.
        if (mtu->m_isEnabled) {
            auto lowMTU = to_string(mtu->m_lowMTU);
            auto interface1 = string{ mtu->m_interface.data() };

            if (mtu->runProcess("netsh.exe", "interface ipv4 set subinterface \"" + interface1 + "\" mtu=" + lowMTU + " store=persistent")) {
                g_log << "Lowered MTU successfully." << endl;
            }
        }
        
        // Call the original connection function to actually create the connection.
        auto originalConnection = (decltype(createConnection)*)mtu->m_hook->getOriginal();
        auto result = originalConnection(a1, a2);
        
        // Restore the MTU to normal now that the connection has been created.
        if (mtu->m_isEnabled) {
            auto normalMTU = to_string(mtu->m_normalMTU);
            auto interface1 = string{ mtu->m_interface.data() };

            if (mtu->runProcess("netsh.exe", "interface ipv4 set subinterface \"" + interface1 + "\" mtu=" + normalMTU + " store=persistent")) {
                g_log << "Restored MTU successfully." << endl;
            }
        }

        return result;
    }
}