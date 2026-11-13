/* =============================================================================
 *  PROJECT:    ZVD SOVEREIGN CONSTRUCTOR ROOT & COGNITIVE PROXY
 *              (zvd_local_ai_proxy.cpp - V2.0-CONSTRUCTOR-ROOT-C20)
 *  FILENAME:   zvd_local_ai_proxy.cpp
 *  VERSION:    2026.11-V2.0-CONSTRUCTOR-ROOT-C20
 *
 *  BUILD (Linux/macOS): g++ -std=c++20 -O3 -fopenmp zvd_local_ai_proxy.cpp -o zvd_proxy -lm
 *  BUILD (Windows/MSVC): cl /std:c++20 /O2 /openmp zvd_local_ai_proxy.cpp /Fe:zvd_proxy.exe ws2_32.lib
 *
 *  AUTHOR:     Vladimir Zavodiuk (Владимир Заводюк)
 *              Root-Architect / Varna, Bulgaria
 *
 *  COPYRIGHT:  Copyright (c) 2026 Vladimir Zavodiuk (Владимир Заводюк)
 *              ALL RIGHTS RESERVED / ВСЕ ПРАВА ЗАЩИЩЕНЫ
 *
 *  LICENSE:    Proprietary. All rights reserved.
 * =============================================================================
 */

#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <thread>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <fstream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#endif

// =====================================================================
// ПУНКТ 1: КАСКАДНОЕ АВТОПОДКЛЮЧЕНИЕ МОДУЛЕЙ-КАРТРИДЖЕЙ (__has_include)
// =====================================================================

#if defined(__has_include)
  // СЛОТ 1 — Ядро Дата-центра (C17)
  #if __has_include("zvd_datacenter_node_v28_lite.c")
    #define ZVD_HAS_DATACENTER_CORE 1
    #define ZVD_CORE_FILENAME "zvd_datacenter_node_v28_lite.c"
  #elif __has_include("13.11_zvd_datacenter_node_v28_lite_v2.c")
    #define ZVD_HAS_DATACENTER_CORE 1
    #define ZVD_CORE_FILENAME "13.11_zvd_datacenter_node_v28_lite_v2.c"
  #endif

  #if defined(ZVD_HAS_DATACENTER_CORE)
    #define ZVD_NO_MAIN
    #ifndef restrict
      #if defined(__GNUC__) || defined(__clang__)
        #define restrict __restrict__
      #elif defined(_MSC_VER)
        #define restrict __restrict
      #else
        #define restrict
      #endif
    #endif
    extern "C" {
      #include ZVD_CORE_FILENAME
    }
  #endif

  // СЛОТ 2 — Когнитивный ИИ-модуль (12 масок / Театр / Монолит)
  #if __has_include("zvd_cognitive_theatre.hpp")
    #define ZVD_HAS_COGNITIVE_AI 1
    #define ZVD_AI_FILENAME "zvd_cognitive_theatre.hpp"
    #include ZVD_AI_FILENAME
  #elif __has_include("zvd_cognitive_ai_node.hpp")
    #define ZVD_HAS_COGNITIVE_AI 1
    #define ZVD_AI_FILENAME "zvd_cognitive_ai_node.hpp"
    #include ZVD_AI_FILENAME
  #endif

  // СЛОТ 3 — Открытый слот для сторонних плагинов
  #if __has_include("zvd_custom_plugin.hpp")
    #define ZVD_HAS_CUSTOM_PLUGIN 1
    #include "zvd_custom_plugin.hpp"
  #endif
#endif

// Заглушки для СЛОТА 2 и СЛОТА 3 на случай их отсутствия в папке
#ifndef ZVD_HAS_COGNITIVE_AI
inline int zvd_ai_module_evaluate(std::string_view, int8_t, double) { return 0; }
inline bool zvd_ai_module_verify_response(std::string_view, std::string_view) { return true; }
#endif

#ifndef ZVD_HAS_CUSTOM_PLUGIN
inline int zvd_custom_plugin_hook(std::string_view) { return 0; }
#endif

// --- Потокобезопасный кольцевой кэш дедупликации ---
static std::mutex g_dedup_mutex;
static constexpr size_t DEDUP_CACHE_SIZE = 256;
static uint64_t g_seen_hashes[DEDUP_CACHE_SIZE] = {0};
static size_t g_dedup_head = 0;
static uint32_t g_loop_counter[DEDUP_CACHE_SIZE] = {0};

// Быстрый 64-битный хэш
static uint64_t zvd_proxy_fast_hash64(std::string_view sv) {
    uint64_t h = 0x9E3779B97F4A7C15ULL ^ sv.size();
    for (char c : sv) {
        h ^= (uint8_t)c;
        h *= 0xBF58476D1CE4E5B9ULL;
        h ^= h >> 31;
    }
    h ^= h >> 33;
    h *= 0xFF51AFD7ED558CCDULL;
    h ^= h >> 33;
    return h;
}

// Потокобезопасная проверка на зацикленность
static bool zvd_check_loop_and_dedup(std::string_view prompt) {
    uint64_t h = zvd_proxy_fast_hash64(prompt);
    std::lock_guard<std::mutex> lock(g_dedup_mutex);
    
    for (size_t i = 0; i < DEDUP_CACHE_SIZE; i++) {
        if (g_seen_hashes[i] == h) {
            g_loop_counter[i]++;
            if (g_loop_counter[i] >= 3) {
                return true; // Шторм / Зацикливание заблокировано
            }
            return false;
        }
    }
    g_seen_hashes[g_dedup_head] = h;
    g_loop_counter[g_dedup_head] = 1;
    g_dedup_head = (g_dedup_head + 1) % DEDUP_CACHE_SIZE;
    return false;
}

// =====================================================================
// ПУНКТ 2: ЕДИНЫЙ КОНВЕЙЕР ОБРАБОТКИ ЗАПРОСА (IZvdCognitiveBridge)
// =====================================================================

class IZvdCognitiveBridge {
public:
    enum class Trity { PLUS, ZERO, MINUS };

    struct Decision {
        Trity trity;
        std::string action_hint;
    };

    static Decision evaluate_prompt(std::string_view prompt) {
        // ШАГ 1 (Базовый): Проверка на зацикливание и спам
        if (zvd_check_loop_and_dedup(prompt)) {
            return { Trity::MINUS, "[ZVD-GUARD] Loop detected & blocked. CPU/Battery saved." };
        }

        int8_t gl_inv = 0;
        double prompt_entropy = 0.0;

        // ШАГ 2 (СЛОТ 1): Ядро дата-центра
#if defined(ZVD_HAS_DATACENTER_CORE)
        static thread_local bool luts_inited = false;
        if (!luts_inited) {
            zvd_ensure_luts();
            luts_inited = true;
        }

        ZVD_BiCore120D bicore;
        zvd_bicore_init(&bicore, 0, 1);
        ZVD_DatacenterPacket pkt;

        if (zvd_node_ingest_monolith_only(reinterpret_cast<const uint8_t*>(prompt.data()), prompt.size(), &bicore, &pkt)) {
            gl_inv = pkt.gl_invariant;
            prompt_entropy = pkt.entropy;

            if (pkt.entropy > 7.2 || pkt.gl_invariant == -2) {
                return { Trity::MINUS, "[ZVD-120D-GUARD] High entropy or Godel-Lukasiewicz VETO triggered." };
            }

            static thread_local int udp_sock = -1;
            if (udp_sock < 0) {
                udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
#ifndef _WIN32
                int flags = fcntl(udp_sock, F_GETFL, 0);
                fcntl(udp_sock, F_SETFL, flags | O_NONBLOCK);
#endif
            }

            if (udp_sock >= 0) {
                const char* env_port = std::getenv("ZVD_DC_PORT");
                int dc_port = env_port ? std::atoi(env_port) : 9000;
                sockaddr_in dc_addr{};
                dc_addr.sin_family = AF_INET;
                dc_addr.sin_port = htons((uint16_t)dc_port);
                inet_pton(AF_INET, "127.0.0.1", &dc_addr.sin_addr);

                alignas(64) uint8_t optic_frame[sizeof(ZVD_OpticDatacenterHeader) + sizeof(ZVD_Monolith334)];
                size_t optic_sz = 0;
                const char* master_key = std::getenv("ZVD_MASTER_KEY");
                if (!master_key) master_key = "ZVD-SOVEREIGN-ENTERPRISE-2026";

                if (zvd_optic_frame_pack(101, 202, (const uint8_t*)&pkt.monolith, sizeof(ZVD_Monolith334), master_key, optic_frame, sizeof(optic_frame), &optic_sz)) {
                    sendto(udp_sock, (const char*)optic_frame, optic_sz, 0, (struct sockaddr*)&dc_addr, sizeof(dc_addr));
                }
            }
        }
#endif

        // ШАГ 3 (СЛОТ 2): Когнитивный ИИ-модуль (12 масок)
#if defined(ZVD_HAS_COGNITIVE_AI)
        int ai_verdict = zvd_ai_module_evaluate(prompt, gl_inv, prompt_entropy);
        if (ai_verdict == -1) {
            return { Trity::MINUS, "[ZVD-THEATRE-VETO] Prompt rejected by 12-Mask Cognitive Arbitration." };
        } else if (ai_verdict == 1) {
            return { Trity::PLUS, "COGNITIVE_FAST_PATH" };
        }
#endif

        // ШАГ 4 (СЛОТ 3): Пользовательский плагин
#if defined(ZVD_HAS_CUSTOM_PLUGIN)
        int plugin_verdict = zvd_custom_plugin_hook(prompt);
        if (plugin_verdict < 0) {
            return { Trity::MINUS, "[ZVD-PLUGIN-VETO] Blocked by custom user plugin." };
        } else if (plugin_verdict > 0) {
            return { Trity::PLUS, "PLUGIN_BYPASS" };
        }
#endif

        // ШАГ 5 (Fallback): Базовая эвристика
        if (prompt.size() < 40) {
            return { Trity::PLUS, "FAST_BYPASS" };
        }
        return { Trity::ZERO, "DEEP_REASONING_REQUIRED" };
    }
};

// --- Потоковый экстрактор JSON без лишних пробелов в литералах ---
static std::string extract_json_content(std::string_view body) {
    size_t pos = body.find("\"content\"");
    if (pos == std::string_view::npos) {
        pos = body.find("\"role\":\"user\"");
        if (pos != std::string_view::npos) {
            pos = body.find("\"content\"", pos);
        }
    }
    if (pos == std::string_view::npos) return "";

    size_t colon = body.find(':', pos);
    if (colon == std::string_view::npos) return "";

    size_t first_quote = body.find('"', colon);
    if (first_quote == std::string_view::npos) return "";

    size_t current = first_quote + 1;
    std::string result;
    while (current < body.size()) {
        char c = body[current];
        if (c == '\\' && current + 1 < body.size()) {
            result.push_back(body[current + 1]);
            current += 2;
            continue;
        }
        if (c == '"') break;
        result.push_back(c);
        current++;
    }
    return result;
}

// --- Пересылка запроса в локальный движок (llama.cpp / Ollama) ---
static std::string forward_to_local_llm(const std::string &target_ip, int target_port, const std::string &raw_request) {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return "HTTP/1.1 502 Bad Gateway\r\n\r\n{\"error\":\"Socket creation failed\"}";

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)target_port);
    inet_pton(AF_INET, target_ip.c_str(), &addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
#ifdef _WIN32
        closesocket(sock); WSACleanup();
#else
        close(sock);
#endif
        return "HTTP/1.1 502 Bad Gateway\r\n\r\n{\"error\":\"Cannot connect to local LLM engine\"}";
    }

    send(sock, raw_request.data(), raw_request.size(), 0);

    std::string response;
    char buffer[4096];
    int bytes = 0;
    while ((bytes = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes] = '\0';
        response.append(buffer, bytes);
    }

#ifdef _WIN32
    closesocket(sock); WSACleanup();
#else
    close(sock);
#endif

    return response;
}

// =====================================================================
// ПУНКТ 3: ДВУХСТОРОННЯЯ ПРОВЕРКА ОТВЕТА LLM (verify_response)
// =====================================================================

static void handle_client(int client_sock, std::string target_ip, int target_port) {
    std::vector<char> buffer(65536);
    int received = recv(client_sock, buffer.data(), buffer.size() - 1, 0);
    if (received <= 0) {
#ifdef _WIN32
        closesocket(client_sock);
#else
        close(client_sock);
#endif
        return;
    }

    std::string_view req(buffer.data(), received);
    size_t body_pos = req.find("\r\n\r\n");
    std::string_view body = (body_pos != std::string_view::npos) ? req.substr(body_pos + 4) : "";

    std::string user_prompt = extract_json_content(body);
    auto decision = IZvdCognitiveBridge::evaluate_prompt(user_prompt);

    std::string http_response;

    if (decision.trity == IZvdCognitiveBridge::Trity::MINUS) {
        std::fprintf(stderr, "[ZVD-GUARD] %s | Prompt len: %zu\n", decision.action_hint.c_str(), user_prompt.size());
        std::string json_reply = "{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":\"" + decision.action_hint + "\"}}]}";
        http_response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + 
                        std::to_string(json_reply.size()) + "\r\nConnection: close\r\n\r\n" + json_reply;
    } else {
        http_response = forward_to_local_llm(target_ip, target_port, std::string(req));

        // Двухсторонняя проверка ответа LLM через когнитивный арбитр
#if defined(ZVD_HAS_COGNITIVE_AI)
        if (!zvd_ai_module_verify_response(user_prompt, http_response)) {
            std::fprintf(stderr, "[ZVD-CAPITANO-VETO] Response rejected by 12-Mask Arbitration.\n");
            std::string veto_reply = "{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":\"[ZVD-CAPITANO-VETO] Response rejected by 12-Mask Arbitration.\"}}]}";
            http_response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + 
                            std::to_string(veto_reply.size()) + "\r\nConnection: close\r\n\r\n" + veto_reply;
        }
#endif
    }

    send(client_sock, http_response.data(), http_response.size(), 0);

#ifdef _WIN32
    closesocket(client_sock);
#else
    close(client_sock);
#endif
}

// =====================================================================
// ПУНКТ 4: ИНТЕРАКТИВНАЯ КАРТА КОНСТРУКТОРА В КОНСОЛИ
// =====================================================================

int main(int argc, char **argv) {
    int listen_port = (argc > 1) ? std::atoi(argv[1]) : 9120;
    std::string target_ip = (argc > 2) ? argv[2] : "127.0.0.1";
    int target_port = (argc > 3) ? std::atoi(argv[3]) : 8080;

#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        std::fprintf(stderr, "[!] Failed to create server socket\n");
        return 1;
    }

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons((uint16_t)listen_port);

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::fprintf(stderr, "[!] Bind failed on port %d\n", listen_port);
        return 1;
    }

    if (listen(server_sock, 128) < 0) {
        std::fprintf(stderr, "[!] Listen failed\n");
        return 1;
    }

    std::printf("======================================================================\n");
    std::printf("ZVD SOVEREIGN CONSTRUCTOR ROOT & COGNITIVE PROXY (V2.0-C20)\n");
    std::printf("Listening on 127.0.0.1:%d -> Forwarding to %s:%d\n", listen_port, target_ip.c_str(), target_port);
    std::printf("Root-Architect: Vladimir Zavodiuk (Владимир Заводюк)\n");
    std::printf("======================================================================\n");

    // Вывод интерактивной карты подключенных слотов
    std::printf("[ROOT]    ZVD Constructor Bus V2.0 ..................... [ONLINE]\n");

#if defined(ZVD_HAS_DATACENTER_CORE)
    std::printf("[SLOT 1]  Datacenter Core 120D & Optic ZVDO ............ [ACTIVE] ('%s')\n", ZVD_CORE_FILENAME);
#else
    std::printf("[SLOT 1]  Datacenter Core 120D & Optic ZVDO ............ [NOT FOUND]\n");
    std::printf("          -> Подсказка: положите 'zvd_datacenter_node_v28_lite.c' в эту папку.\n");
#endif

#if defined(ZVD_HAS_COGNITIVE_AI)
    std::printf("[SLOT 2]  Cognitive AI Theatre (12 Masks 3-4-5) ........ [ACTIVE] ('%s')\n", ZVD_AI_FILENAME);
#else
    std::printf("[SLOT 2]  Cognitive AI Theatre (12 Masks 3-4-5) ........ [NOT FOUND]\n");
    std::printf("          -> Подсказка: положите 'zvd_cognitive_theatre.hpp' в эту папку.\n");
#endif

#if defined(ZVD_HAS_CUSTOM_PLUGIN)
    std::printf("[SLOT 3]  External / Custom User Plugin ................ [ACTIVE] ('zvd_custom_plugin.hpp')\n");
#else
    std::printf("[SLOT 3]  External / Custom User Plugin ................ [NOT FOUND]\n");
    std::printf("          -> Подсказка: положите 'zvd_custom_plugin.hpp' для расширения логики.\n");
#endif

    std::printf("======================================================================\n");

    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) continue;

        std::thread(handle_client, client_sock, target_ip, target_port).detach();
    }

#ifdef _WIN32
    closesocket(server_sock);
    WSACleanup();
#else
    close(server_sock);
#endif

    return 0;
}
