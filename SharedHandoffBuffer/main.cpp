#include "SharedHandoffBuffer.hpp"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

// Helper to generate a unique buffer prefix (for demo)
std::string GenerateUniquePrefix() {
    return "HandoffBuffer" + std::to_string(GetTickCount64());
}

void RunSource(const std::string& sz_prefix) {
    SharedHandoffBuffer oBuffer(/*b_create_ipc=*/true, sz_prefix);
    std::cout << "[Source] Waiting for target to be ready..." << std::endl;
    oBuffer.WaitForTargetReady();
    std::cout << "[Source] Target ready. Starting commands..." << std::endl;

    // Send a few data commands
    for (int i = 0; i < 3; ++i) {
        std::string sz_payload = "Message #" + std::to_string(i);
        oBuffer.SendFromSource(eHandoffCommand::eData, sz_payload);

        eHandoffResponse e_resp;
        std::string sz_response;
        if (oBuffer.WaitForTarget(e_resp, sz_response, 3000)) {
            if (e_resp == eHandoffResponse::ePayload) {
                std::cout << "[Source] Target responded: " << sz_response << std::endl;
            }
            else {
                std::cout << "[Source] Unexpected response type.\n";
            }
        }
        else {
            std::cout << "[Source] No response from target!\n";
        }
    }

    // Idle loop with liveness polling
    for (int i = 0; i < 3; ++i) {
        std::cout << "[Source] Polling target for liveness..." << std::endl;
        oBuffer.SendFromSource(eHandoffCommand::eLivenessCheck, "");
        eHandoffResponse e_resp;
        std::string sz_response;
        if (oBuffer.WaitForTarget(e_resp, sz_response, 2000)) {
            if (e_resp == eHandoffResponse::eAlive) {
                std::cout << "[Source] Target is alive." << std::endl;
            }
            else {
                std::cout << "[Source] Liveness: Unexpected response type.\n";
            }
        }
        else {
            std::cout << "[Source] Target did not respond to liveness check. Exiting." << std::endl;
            break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

void RunTarget(const std::string& sz_prefix) {
    SharedHandoffBuffer oBuffer(/*b_create_ipc=*/false, sz_prefix);
    oBuffer.SignalTargetReady();
    std::cout << "[Target] Ready and waiting for commands..." << std::endl;

    while (true) {
        std::string sz_payload;
        eHandoffCommand e_cmd = oBuffer.WaitForSource(sz_payload);
        if (e_cmd == eHandoffCommand::eData) {
            std::string sz_reply = "Processed: " + sz_payload;
            oBuffer.SendFromTarget(eHandoffResponse::ePayload, sz_reply);
            std::cout << "[Target] Responded to Data command: " << sz_reply << std::endl;
        }
        else if (e_cmd == eHandoffCommand::eLivenessCheck) {
            oBuffer.SendFromTarget(eHandoffResponse::eAlive, "");
            std::cout << "[Target] Responded to Liveness Check." << std::endl;
        }
        else {
            oBuffer.SendFromTarget(eHandoffResponse::eNone, "");
            std::cout << "[Target] Unknown command received." << std::endl;
        }
    }
}

int main(int argc, char* argv[]) {
    // Usage:
    //   Parent: main.exe
    //   Source: main.exe source <prefix>
    //   Target: main.exe target <prefix>

    if (argc == 1) {
        // Launch both children with a unique prefix
        std::string sz_prefix = GenerateUniquePrefix();
        std::string sz_exe = argv[0];
        std::string sz_src_cmd = "\"" + sz_exe + "\" source " + sz_prefix;
        std::string sz_tgt_cmd = "\"" + sz_exe + "\" target " + sz_prefix;

        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi_src, pi_tgt;

        if (!CreateProcessA(NULL, (LPSTR)sz_src_cmd.c_str(), NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi_src)) {
            std::cerr << "[Parent] Failed to launch Source process\n";
            return 1;
        }
        if (!CreateProcessA(NULL, (LPSTR)sz_tgt_cmd.c_str(), NULL, NULL, TRUE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi_tgt)) {
            std::cerr << "[Parent] Failed to launch Target process\n";
            CloseHandle(pi_src.hProcess);
            CloseHandle(pi_src.hThread);
            return 1;
        }
        std::cout << "[Parent] Both processes launched with prefix: " << sz_prefix << std::endl;

        HANDLE h_children[2] = { pi_src.hProcess, pi_tgt.hProcess };
        WaitForMultipleObjects(2, h_children, TRUE, INFINITE);

        CloseHandle(pi_src.hProcess);
        CloseHandle(pi_src.hThread);
        CloseHandle(pi_tgt.hProcess);
        CloseHandle(pi_tgt.hThread);

        std::cout << "[Parent] Both children exited. Done.\n";
        return 0;
    }

    if (argc == 3 && std::string(argv[1]) == "source") {
        RunSource(argv[2]);
        return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "target") {
        RunTarget(argv[2]);
        return 0;
    }

    std::cout << "Usage:\n";
    std::cout << "  Parent: main.exe\n";
    std::cout << "  Source: main.exe source <prefix>\n";
    std::cout << "  Target: main.exe target <prefix>\n";
    return 1;
}