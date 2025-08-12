#pragma once
#include <windows.h>
#include "SharedHandoffBuffer.hpp"
#include <iostream>
#include <chrono>
#include <string>
#include <stdexcept>

class MutexGuard {
public:
    explicit MutexGuard(HANDLE hMutex) noexcept
        : m_hMutex(hMutex)
    {
        if (!m_hMutex)
        {
            std::abort();
            DebugBreak();
        }

        DWORD dwResult = WaitForSingleObject(m_hMutex, INFINITE);
        if (dwResult != WAIT_OBJECT_0)
        {
            std::abort();
            DebugBreak();
        }
    }

	// Non-copyable and non-movable
    MutexGuard(const MutexGuard&) = delete;
    MutexGuard& operator=(const MutexGuard&) = delete;
    MutexGuard(MutexGuard&& other) noexcept = delete;
    MutexGuard& operator=(MutexGuard&& other) = delete;

    ~MutexGuard() noexcept {
        if (!m_hMutex)
            std::abort(); 
        ReleaseMutex(m_hMutex);
    }

private:
    HANDLE m_hMutex;
};

class HandleWrapper {
public:
    HandleWrapper() : handle_(nullptr) {}

    explicit HandleWrapper(const std::string& name, bool creator, bool isMutex) noexcept {

        std::cout << "[SharedHandoffBuffer::HandleWrapper()] 1..." << name << std::endl;

        if (isMutex)
        {
            if (creator) {
                handle_ = CreateMutexA(NULL, FALSE, name.c_str());
            }
            else {
                handle_ = OpenMutexA(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, name.c_str());
            }
        }
        else {
            if (creator) {
                handle_ = CreateEventA(NULL, FALSE, FALSE, name.c_str());
            }
            else {
                handle_ = OpenEventA(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, name.c_str());
            }
		}

        if(!valid()) {
            std::abort();
            DebugBreak();
		}
    }

	HandleWrapper(const HandleWrapper&) = delete;
    HandleWrapper& operator=(const HandleWrapper&) = delete;
    
    HandleWrapper(HandleWrapper&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }
    HandleWrapper& operator=(HandleWrapper&& other) noexcept {
        if (this != &other) {
            Close();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    ~HandleWrapper() {
        CloseHandle(handle_);
        handle_ = nullptr;
    }

    HANDLE get() const { return handle_; }
    operator HANDLE() const { return handle_; }

    bool valid() const { return handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE; }

private:

    void Close() {
        if (valid()) {
            CloseHandle(handle_);
            handle_ = nullptr;
        }
    }

    HANDLE handle_;
};

template<typename T>
class MappedBuffer {
public:
    MappedBuffer() noexcept
        : m_hMap(nullptr), m_ptr(nullptr)
    {}

    MappedBuffer(const std::string& mapName, bool creator) noexcept
        : m_hMap(nullptr), m_ptr(nullptr)
    {
        std::cout << "[SharedHandoffBuffer::MappedBuffer()] 1..." << mapName << "size: " << sizeof(T) << std::endl;

        if (creator) {
            
            std::cout << "[SharedHandoffBuffer::MappedBuffer()] 2 creator..." << std::endl;

            m_hMap = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(T), mapName.c_str());
            if (!m_hMap)
            {
                std::cout << "[SharedHandoffBuffer::MappedBuffer()] 2 creator..." << GetLastError() << std::endl;

                std::abort();
                DebugBreak();
            }

            std::cout << "[SharedHandoffBuffer::MappedBuffer()] 2 creator created..." << std::endl;

        }
        else {

            std::cout << "[SharedHandoffBuffer::MappedBuffer()] 2 non creator..." << std::endl;


            m_hMap = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, mapName.c_str());
            if (!m_hMap)
            {
                std::cout << "[SharedHandoffBuffer::MappedBuffer()] 2 non creator..." << GetLastError() << std::endl;
                std::abort();
                DebugBreak();
            }
                

            std::cout << "[SharedHandoffBuffer::MappedBuffer()] 2 non creator opened..." << std::endl;
        }


        m_ptr = reinterpret_cast<T*>(MapViewOfFile(m_hMap, FILE_MAP_ALL_ACCESS, 0, 0, 0));
        if (!m_ptr)
            std::abort();

        if(creator) {
            ZeroMemory(m_ptr, sizeof(T));
		}
    }

    ~MappedBuffer() noexcept  {

        std::cout << "[SharedHandoffBuffer::~MappedBuffer]" << std::endl;

        if (m_ptr)
            UnmapViewOfFile(m_ptr);
		if (m_hMap)
            CloseHandle(m_hMap);

        m_ptr = nullptr;
		m_hMap = nullptr;
    }
    
    MappedBuffer(const MappedBuffer&) = delete;
    MappedBuffer& operator=(const MappedBuffer&) = delete;

    MappedBuffer(MappedBuffer&& other) noexcept : m_ptr(other.m_ptr) {
        other.m_hMap = nullptr;
        other.m_ptr = nullptr;
    }
    MappedBuffer& operator=(MappedBuffer&& other) noexcept {
        if (this != &other) {
            if (m_ptr) UnmapViewOfFile(m_ptr);
            m_ptr = other.m_ptr;
            other.m_ptr = nullptr;

            if (m_hMap)
                CloseHandle(m_hMap);
			m_hMap = other.m_hMap;
			other.m_hMap = nullptr;
        }
        return *this;
    }
    T* get() const noexcept { return m_ptr; }
    T& operator*() const noexcept { return *m_ptr; }
    T* operator->() const noexcept { return m_ptr; }

private:
	HANDLE m_hMap;
    T* m_ptr;
};

class SharedHandoffBuffer {
public:
    static constexpr std::size_t BUFFER_SIZE = 4096;

    enum class HandoffCommand : uint8_t {
        GetIdentity,
        SetHandoffId,
        LivenessCheck,
    };

    enum class HandoffResponse : uint8_t {
        None = 0,
        Identity, 
        Acknowledged,
        Alive,
    };

    struct HandoffMessage {
        HandoffCommand cmd;
		HandoffResponse resp;
        uint16_t payload_size;
        char payload[BUFFER_SIZE - 4];
    };

    SharedHandoffBuffer(bool isSource, const std::string& sz_prefix) noexcept
    {
        std::string bufferName = "Local\\" + sz_prefix + "_Buffer";
        std::string mutexName = "Local\\" + sz_prefix + "_Mutex";
        std::string srcEventName = "Local\\" + sz_prefix + "_SourceEvent";
        std::string tgtEventName = "Local\\" + sz_prefix + "_TargetEvent";
        std::string tgtReadyEventName = "Local\\" + sz_prefix + "_TargetReadyEvent";

        std::cout << "[SharedHandoffBuffer] 1..." << std::endl;

        m_pMsg = MappedBuffer<HandoffMessage>(bufferName, isSource /* creator */);
        
        std::cout << "[SharedHandoffBuffer] 2..." << std::endl;
        m_hMutex = HandleWrapper(mutexName, isSource, true);
        std::cout << "[SharedHandoffBuffer] 3..." << std::endl;
        m_hSrcEvent = HandleWrapper(srcEventName, isSource, false);
        std::cout << "[SharedHandoffBuffer] 4..." << std::endl;
        m_hTgtEvent = HandleWrapper(tgtEventName, isSource, false);
        std::cout << "[SharedHandoffBuffer] 5..." << std::endl;
        m_hTargetReadyEvent = HandleWrapper(tgtReadyEventName, isSource, false);

        if (!m_hMutex || !m_hSrcEvent || !m_hTgtEvent)
            std::abort();
    }

    SharedHandoffBuffer(const SharedHandoffBuffer&) = delete;
    SharedHandoffBuffer& operator=(const SharedHandoffBuffer&) = delete;

    SharedHandoffBuffer(SharedHandoffBuffer&&) = delete;
    SharedHandoffBuffer& operator=(SharedHandoffBuffer&&) = delete;

    bool WaitForTargetReady() noexcept {
		DWORD timeout_ms = 3000000; // 30 seconds
        DWORD poll_interval_ms = 100;
        auto start = std::chrono::steady_clock::now();
        DWORD elapsed = 0;

        while (elapsed < timeout_ms) {
            DWORD wait_result = WaitForSingleObject(m_hTargetReadyEvent, poll_interval_ms);
            if (wait_result == WAIT_OBJECT_0) {
                std::cout << "[WaitForTargetReady] target ready" << std::endl;
                return true;
            }
            else if (wait_result != WAIT_TIMEOUT) {
                std::cout << "[WaitForTargetReady] !!" << std::endl;
                break;
            }

            elapsed = static_cast<DWORD>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start
                ).count());

        };

        std::cout << "[WaitForTargetReady] expired" << std::endl;
        return false;       
    }

    void SignalTargetReady() noexcept {
        SetEvent(m_hTargetReadyEvent);
    }

    // Source sends a command and payload to target
    void SendFromSource(HandoffCommand cmd, const std::string& payload) noexcept {
        {
            MutexGuard lock(m_hMutex);
            m_pMsg->cmd = cmd;
            m_pMsg->resp = HandoffResponse::None;
            strncpy_s(m_pMsg->payload, payload.c_str(), sizeof(m_pMsg->payload) - 1);
            m_pMsg->payload[sizeof(m_pMsg->payload) - 1] = 0;
        }

        SetEvent(m_hTgtEvent);
    }

    // Target waits and receives command from source
    HandoffCommand WaitForSource(std::string& sz_out_payload) noexcept {
        WaitForSingleObject(m_hTgtEvent, INFINITE);
        
        MutexGuard lock(m_hMutex);
        HandoffCommand cmd = m_pMsg->cmd;
        sz_out_payload = m_pMsg->payload;
        return cmd;        
    }

    // Target sends response (Payload or Alive), overwrites payload
    void SendFromTarget(HandoffResponse resp, const std::string& payload) noexcept {
        {
            MutexGuard lock(m_hMutex);
            m_pMsg->resp = resp;
            strncpy_s(m_pMsg->payload, payload.c_str(), sizeof(m_pMsg->payload) - 1);
            m_pMsg->payload[sizeof(m_pMsg->payload) - 1] = 0;
        }
        
        SetEvent(m_hSrcEvent);
    }

    // Source waits and receives response from target
    // Returns true if response received within timeout, false otherwise
    bool WaitForTarget(HandoffResponse& e_out_resp, std::string& sz_out_payload, DWORD dw_timeout = INFINITE) noexcept {
        DWORD dw_res = WaitForSingleObject(m_hSrcEvent, dw_timeout);
        if (dw_res == WAIT_OBJECT_0) {
            MutexGuard lock(m_hMutex);
            e_out_resp = m_pMsg->resp;
            sz_out_payload = m_pMsg->payload;
            return true;
        }
        return false;
    }

private:
    MappedBuffer <HandoffMessage> m_pMsg;
    HandleWrapper m_hMutex;
    HandleWrapper m_hSrcEvent, m_hTgtEvent;
    HandleWrapper m_hTargetReadyEvent;
    
};