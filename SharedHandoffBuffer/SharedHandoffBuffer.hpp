#pragma once
#include <windows.h>
#include <string>
#include <stdexcept>

enum class eHandoffCommand : int {
    eNone = 0,
    eData,
    eLivenessCheck,
};

enum class eHandoffResponse : int {
    eNone = 0,
    ePayload,    // Target's response to Data
    eAlive,      // Target's response to LivenessCheck
};

struct HandoffMessage {
    eHandoffCommand e_cmd;
    eHandoffResponse e_resp;
    char pch_payload[256];
};

class SharedHandoffBuffer {
public:
    SharedHandoffBuffer(bool b_create_ipc, const std::string& sz_prefix)
        : m_hMap(nullptr), m_hMutex(nullptr), m_hSrcEvent(nullptr), m_hTgtEvent(nullptr), m_hTargetReadyEvent(nullptr), m_pMsg(nullptr)
    {
        std::string sz_buffer_name = "Local\\" + sz_prefix + "_Buffer";
        std::string sz_mutex_name = "Local\\" + sz_prefix + "_Mutex";
        std::string sz_src_event_name = "Local\\" + sz_prefix + "_SourceEvent";
        std::string sz_tgt_event_name = "Local\\" + sz_prefix + "_TargetEvent";
        std::string sz_target_ready_event = "Local\\" + sz_prefix + "_TargetReadyEvent";

        if (b_create_ipc) {
            m_hMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(HandoffMessage), sz_buffer_name.c_str());
            if (!m_hMap) throw std::runtime_error("Failed to create file mapping");
            m_pMsg = (HandoffMessage*)MapViewOfFile(m_hMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(HandoffMessage));
            if (!m_pMsg) throw std::runtime_error("Failed to map view of file");
            memset(m_pMsg, 0, sizeof(HandoffMessage));
            m_hMutex = CreateMutexA(NULL, FALSE, sz_mutex_name.c_str());
            m_hSrcEvent = CreateEventA(NULL, FALSE, FALSE, sz_src_event_name.c_str());
            m_hTgtEvent = CreateEventA(NULL, FALSE, FALSE, sz_tgt_event_name.c_str());
            m_hTargetReadyEvent = CreateEventA(NULL, TRUE, FALSE, sz_target_ready_event.c_str());
        }
        else {
            m_hTargetReadyEvent = OpenEventA(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, sz_target_ready_event.c_str());
            if (!m_hTargetReadyEvent) throw std::runtime_error("Failed to open target ready event");
            WaitForSingleObject(m_hTargetReadyEvent, INFINITE);

            m_hMap = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, sz_buffer_name.c_str());
            if (!m_hMap) throw std::runtime_error("Failed to open file mapping");
            m_pMsg = (HandoffMessage*)MapViewOfFile(m_hMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(HandoffMessage));
            if (!m_pMsg) throw std::runtime_error("Failed to map view of file");
            m_hMutex = OpenMutexA(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, sz_mutex_name.c_str());
            m_hSrcEvent = OpenEventA(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, sz_src_event_name.c_str());
            m_hTgtEvent = OpenEventA(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, sz_tgt_event_name.c_str());
        }
        if (!m_hMutex || !m_hSrcEvent || !m_hTgtEvent)
            throw std::runtime_error("Failed to create/open IPC objects");
    }

    ~SharedHandoffBuffer() {
        if (m_pMsg) UnmapViewOfFile(m_pMsg);
        if (m_hMap) CloseHandle(m_hMap);
        if (m_hMutex) CloseHandle(m_hMutex);
        if (m_hSrcEvent) CloseHandle(m_hSrcEvent);
        if (m_hTgtEvent) CloseHandle(m_hTgtEvent);
        if (m_hTargetReadyEvent) CloseHandle(m_hTargetReadyEvent);
    }

    // Source waits for target to signal ready
    void WaitForTargetReady() {
        WaitForSingleObject(m_hTargetReadyEvent, INFINITE);
    }

    // Target signals ready for commands
    void SignalTargetReady() {
        SetEvent(m_hTargetReadyEvent);
    }

    // Source sends a command and payload to target
    void SendFromSource(eHandoffCommand e_cmd, const std::string& sz_payload) {
        Lock();
        m_pMsg->e_cmd = e_cmd;
        m_pMsg->e_resp = eHandoffResponse::eNone;
        strncpy_s(m_pMsg->pch_payload, sz_payload.c_str(), sizeof(m_pMsg->pch_payload) - 1);
        m_pMsg->pch_payload[sizeof(m_pMsg->pch_payload) - 1] = 0;
        Unlock();
        SetEvent(m_hTgtEvent);
    }

    // Target waits and receives command from source
    eHandoffCommand WaitForSource(std::string& sz_out_payload) {
        WaitForSingleObject(m_hTgtEvent, INFINITE);
        Lock();
        eHandoffCommand e_cmd = m_pMsg->e_cmd;
        sz_out_payload = m_pMsg->pch_payload;
        Unlock();
        return e_cmd;
    }

    // Target sends response (Payload or Alive), overwrites payload
    void SendFromTarget(eHandoffResponse e_resp, const std::string& sz_payload) {
        Lock();
        m_pMsg->e_resp = e_resp;
        strncpy_s(m_pMsg->pch_payload, sz_payload.c_str(), sizeof(m_pMsg->pch_payload) - 1);
        m_pMsg->pch_payload[sizeof(m_pMsg->pch_payload) - 1] = 0;
        Unlock();
        SetEvent(m_hSrcEvent);
    }

    // Source waits and receives response from target
    // Returns true if response received within timeout, false otherwise
    bool WaitForTarget(eHandoffResponse& e_out_resp, std::string& sz_out_payload, DWORD dw_timeout = INFINITE) {
        DWORD dw_res = WaitForSingleObject(m_hSrcEvent, dw_timeout);
        if (dw_res == WAIT_OBJECT_0) {
            Lock();
            e_out_resp = m_pMsg->e_resp;
            sz_out_payload = m_pMsg->pch_payload;
            Unlock();
            return true;
        }
        return false;
    }

private:
    void Lock() { WaitForSingleObject(m_hMutex, INFINITE); }
    void Unlock() { ReleaseMutex(m_hMutex); }

    HANDLE m_hMap;
    HANDLE m_hMutex;
    HANDLE m_hSrcEvent, m_hTgtEvent;
    HANDLE m_hTargetReadyEvent;
    HandoffMessage* m_pMsg;
};