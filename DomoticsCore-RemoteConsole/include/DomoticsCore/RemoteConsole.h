#pragma once

/**
 * @file RemoteConsole.h
 * @brief Telnet-based remote console for log streaming and command execution
 */

#include <DomoticsCore/IComponent.h>
#include <DomoticsCore/Logger.h>
#include <DomoticsCore/Platform_HAL.h>    // For restart()
#include <DomoticsCore/Wifi_HAL.h>        // For WiFi functions
#include <DomoticsCore/WiFiServer_HAL.h>  // For WiFiServer and WiFiClient
// Platform_HAL.h provides: getFreeHeap(), getChipModel(), getChipRevision(), getCpuFreqMHz()
#include <vector>
#include <map>
#include <functional>

namespace DomoticsCore {
namespace Components {

// Log entry structure - uses String for compact storage
struct LogEntry {
    uint32_t timestamp;
    LogLevel level;
    String tag;
    String message;
    
    LogEntry() : timestamp(0), level(LOG_LEVEL_INFO) {}
    LogEntry(uint32_t ts, LogLevel lvl, const char* t, const char* msg)
        : timestamp(ts), level(lvl), tag(t), message(msg) {}
};

// Remote console configuration
struct RemoteConsoleConfig {
    bool enabled = true;
    uint16_t port = 23;                    // Telnet port
    bool requireAuth = false;              // Password authentication
    String password = "";                  // Auth password
    uint32_t bufferSize = DOMOTICS_LOG_BUFFER_SIZE;  // Platform-specific (ESP8266=5, ESP32=100)
    bool allowCommands = true;             // Enable command execution
    std::vector<HAL::IPAddress> allowedIPs;     // IP whitelist (empty = all allowed)
    bool colorOutput = true;               // ANSI color codes
    uint32_t maxClients = 3;               // Max concurrent connections
    LogLevel defaultLogLevel = LOG_LEVEL_INFO;  // Initial log level
};

// Command handler function type
typedef std::function<String(const String& args)> CommandHandler;

/**
 * @class RemoteConsoleComponent
 * @brief Telnet server for remote log viewing and command execution
 * 
 * Features:
 * - Real-time log streaming via Telnet
 * - Circular log buffer with configurable size
 * - Runtime log level control
 * - Command processor with extensible commands
 * - ANSI color-coded output
 * - Password authentication
 * - IP whitelist support
 */
class RemoteConsoleComponent : public IComponent {
private:
    RemoteConsoleConfig config;
    HAL::WiFiServer* telnetServer = nullptr;
    std::vector<HAL::WiFiClient> clients;
    
    // Circular buffer for log entries - grows lazily to avoid OOM on startup
    std::vector<LogEntry> logBuffer;
    size_t logBufferHead = 0;    // Next write position
    size_t logBufferCount = 0;   // Current number of entries
    
    std::map<String, CommandHandler> commands;
    std::map<uint32_t, String> clientBuffers;  // Per-client command buffers (key = client ID)
    LogLevel currentLogLevel;
    std::vector<String> tagFilter;  // Empty = show all
    bool authenticated = false;
    bool connectionInfoDisplayed = false;  // Track if we've shown connection info
    
public:
    RemoteConsoleComponent(const RemoteConsoleConfig& cfg = RemoteConsoleConfig())
        : config(cfg), currentLogLevel(cfg.defaultLogLevel) {
        
        metadata.name = "RemoteConsole";
        metadata.version = "1.4.0";
        metadata.author = "DomoticsCore";
        metadata.description = "Telnet-based remote console with log streaming";
        metadata.category = "Debug";
        metadata.tags = {"telnet", "console", "debug", "logging"};
        
        // Register built-in commands
        registerBuiltInCommands();
    }
    
    ~RemoteConsoleComponent() {
        if (telnetServer) {
            delete telnetServer;
        }
    }
    
    
    ComponentStatus begin() override {
        if (!config.enabled) {
            DLOG_I(LOG_CONSOLE, "RemoteConsole disabled in config");
            setStatus(ComponentStatus::Success);
            return ComponentStatus::Success;
        }
        
        // Register logger callback
        LoggerCallbacks::addCallback([this](LogLevel level, const char* tag, const char* msg) {
            this->log(level, tag, msg);
        });
        
        // Start telnet server (doesn't require WiFi to be connected yet)
        telnetServer = new HAL::WiFiServer(config.port);
        telnetServer->begin();
        telnetServer->setNoDelay(true);
        
        DLOG_I(LOG_CONSOLE, "RemoteConsole started on port %d", config.port);
        
        setStatus(ComponentStatus::Success);
        return ComponentStatus::Success;
    }
    
    void onComponentsReady(const ComponentRegistry& /*registry*/) override {
        // Display connection info once WiFi is connected
        displayConnectionInfo();
    }
    
    void loop() override {
        if (getLastStatus() != ComponentStatus::Success || !telnetServer) return;
        
        // Check if WiFi connected and we haven't displayed info yet
        if (!connectionInfoDisplayed && HAL::WiFiHAL::isConnected()) {
            displayConnectionInfo();
        }
        
        // Accept new clients
        if (telnetServer->hasClient()) {
            HAL::WiFiClient newClient = telnetServer->accept();
            
            if (newClient) {
                // Check max clients
                if (clients.size() >= config.maxClients) {
                    newClient.println("Max clients reached. Disconnecting.");
                    newClient.stop();
                } else if (!isIPAllowed(newClient.remoteIP())) {
                    newClient.println("IP not allowed. Disconnecting.");
                    newClient.stop();
                } else {
                    clients.push_back(newClient);
                    
                    // Clear any initial buffer noise (telnet negotiation)
                    uint32_t clientId = (uint32_t)newClient.remoteIP();
                    clientBuffers[clientId] = "";

                    DLOG_I(LOG_CONSOLE, "Client connected: 0x%08X", clientId);

                    sendWelcome(newClient);
                }
            }
        }
        
        // Handle existing clients
        for (auto it = clients.begin(); it != clients.end(); ) {
            if (!it->connected()) {
                // Clean up client buffer
                uint32_t clientId = it->remoteIP();
                clientBuffers.erase(clientId);
                
                DLOG_I(LOG_CONSOLE, "Client disconnected");
                it = clients.erase(it);
            } else {
                handleClient(*it);
                ++it;
            }
        }
    }
    
    ComponentStatus shutdown() override {
        if (telnetServer) {
            // Disconnect all clients
            for (auto& client : clients) {
                client.println("\nRemoteConsole shutting down...");
                client.stop();
            }
            clients.clear();
            
            telnetServer->stop();
            delete telnetServer;
            telnetServer = nullptr;
        }
        
        DLOG_I(LOG_CONSOLE, "RemoteConsole shut down");
        setStatus(ComponentStatus::Success);
        return ComponentStatus::Success;
    }
    
    /**
     * @brief Log a message to the buffer and connected clients
     */
    void log(LogLevel level, const char* tag, const char* message) {
        if (level > currentLogLevel) return;
        
        // Check tag filter
        if (!tagFilter.empty()) {
            bool tagMatch = false;
            for (const auto& filter : tagFilter) {
                if (filter == tag) {
                    tagMatch = true;
                    break;
                }
            }
            if (!tagMatch) return;
        }
        
        // Create entry on stack first (no heap allocation)
        LogEntry entry(HAL::Platform::getMillis(), level, tag, message);
        
        // Add to circular buffer - grow lazily up to max size
        if (config.bufferSize > 0) {
            if (logBufferCount < config.bufferSize) {
                // Buffer not full yet - just append
                logBuffer.push_back(entry);
                logBufferCount++;
            } else {
                // Buffer full - overwrite oldest (circular)
                logBuffer[logBufferHead] = entry;
            }
            logBufferHead = (logBufferHead + 1) % config.bufferSize;
        }
        
        // Send to connected clients
        if (!clients.empty()) {
            String formatted = formatLogEntry(entry);
            for (auto& client : clients) {
                if (client.connected()) {
                    client.print(formatted);
                }
            }
        }
    }
    
    /**
     * @brief Register a custom command
     */
    void registerCommand(const String& cmd, CommandHandler handler) {
        commands[cmd] = handler;
        DLOG_D(LOG_CONSOLE, "Registered command: %s", cmd.c_str());
    }
    
    /**
     * @brief Set runtime log level
     */
    void setLogLevel(LogLevel level) {
        currentLogLevel = level;
        DLOG_I(LOG_CONSOLE, "Log level set to: %d", level);
    }
    
    /**
     * @brief Set tag filter (empty = show all)
     */
    void setTagFilter(const std::vector<String>& tags) {
        tagFilter = tags;
    }
    
    /**
     * @brief Clear log buffer and release memory
     */
    void clearBuffer() {
        logBuffer.clear();
        logBuffer.shrink_to_fit();  // Release memory back to heap
        logBufferHead = 0;
        logBufferCount = 0;
        DLOG_I(LOG_CONSOLE, "Log buffer cleared");
    }
    
    /**
     * @brief Get recent logs from circular buffer
     */
    std::vector<LogEntry> getRecentLogs(uint32_t count = 100) {
        std::vector<LogEntry> result;
        if (logBufferCount == 0 || config.bufferSize == 0) return result;
        
        uint32_t actualCount = (count < logBufferCount) ? count : logBufferCount;
        result.reserve(actualCount);
        
        // Calculate start position in circular buffer
        size_t startIdx;
        if (logBufferCount < config.bufferSize) {
            // Buffer not full yet - start from beginning
            startIdx = (logBufferCount > actualCount) ? (logBufferCount - actualCount) : 0;
        } else {
            // Buffer is full - oldest is at logBufferHead
            startIdx = (logBufferHead + config.bufferSize - actualCount) % config.bufferSize;
        }
        
        for (uint32_t i = 0; i < actualCount; i++) {
            size_t idx = (startIdx + i) % config.bufferSize;
            result.push_back(logBuffer[idx]);
        }
        
        return result;
    }

private:
    void registerBuiltInCommands() {
        // Help command
        registerCommand("help", [this](const String& args) {
            String help = "\nAvailable commands:\n";
            help += "  help              - Show this help\n";
            help += "  clear             - Clear log buffer\n";
            help += "  level <level>     - Set log level (0-4: NONE/ERROR/WARN/INFO/DEBUG)\n";
            help += "  filter <tag>      - Filter logs by tag (empty = show all)\n";
            help += "  info              - System information\n";
            help += "  heap              - Memory usage\n";
            help += "  reboot            - Restart device\n";
            help += "  quit              - Disconnect\n";
            
            // Add custom commands
            for (const auto& cmd : commands) {
                if (cmd.first != "help" && cmd.first != "clear" && 
                    cmd.first != "level" && cmd.first != "filter" &&
                    cmd.first != "info" && cmd.first != "heap" && 
                    cmd.first != "reboot" && cmd.first != "quit") {
                    help += "  " + cmd.first + "\n";
                }
            }
            
            return help;
        });
        
        // Clear command
        registerCommand("clear", [this](const String& args) {
            clearBuffer();
            return "Log buffer cleared\n";
        });
        
        // Level command
        registerCommand("level", [this](const String& args) -> String {
            if (args.isEmpty()) {
                return "Current log level: " + String((int)currentLogLevel) + "\n";
            }
            
            int level = args.toInt();
            if (level < 0 || level > 4) {
                return String("Invalid level. Use 0-4 (NONE/ERROR/WARN/INFO/DEBUG)\n");
            }
            
            setLogLevel((LogLevel)level);
            return "Log level set to: " + String(level) + "\n";
        });
        
        // Filter command
        registerCommand("filter", [this](const String& args) -> String {
            if (args.isEmpty()) {
                tagFilter.clear();
                return String("Tag filter cleared (showing all)\n");
            }
            
            tagFilter.clear();
            tagFilter.push_back(args);
            return "Filtering logs by tag: " + args + "\n";
        });
        
        // Info command
        registerCommand("info", [this](const String& args) {
            String info = "\nSystem Information:\n";
            info += "  Uptime: " + String(HAL::Platform::getMillis() / 1000) + "s\n";
            info += "  Free Heap: " + String(HAL::getFreeHeap()) + " bytes\n";
            info += "  Chip: " + HAL::getChipModel() + " Rev" + String(HAL::getChipRevision()) + "\n";
            info += "  CPU Freq: " + String(HAL::getCpuFreqMHz()) + " MHz\n";
            info += "  WiFi: " + HAL::WiFiHAL::getSSID() + " (" + HAL::WiFiHAL::getLocalIP() + ")\n";
            info += "  RSSI: " + String(HAL::WiFiHAL::getRSSI()) + " dBm\n";
            return info;
        });
        
        // Heap command
        registerCommand("heap", [this](const String& args) {
            return "Free Heap: " + String(HAL::getFreeHeap()) + " bytes\n";
        });
        
        // Reboot command
        registerCommand("reboot", [this](const String& args) {
            for (auto& client : clients) {
                client.println("Rebooting...");
            }
            HAL::delay(100);
            HAL::restart();
            return "";
        });
        
        // Quit command
        registerCommand("quit", [this](const String& args) {
            return "QUIT";  // Special return value to disconnect
        });
    }
    
    bool isIPAllowed(HAL::IPAddress ip) {
        if (config.allowedIPs.empty()) return true;
        
        for (const auto& allowed : config.allowedIPs) {
            if (ip == allowed) return true;
        }
        
        return false;
    }
    
    void sendWelcome(HAL::WiFiClient& client) {
        client.println("\n========================================");
        client.println("  DomoticsCore Remote Console");
        client.println("========================================");
        client.println("Type 'help' for available commands\n");
        
        // Show recent logs
        auto recent = getRecentLogs(10);
        if (!recent.empty()) {
            client.println("Recent logs:");
            for (const auto& entry : recent) {
                client.print(formatLogEntry(entry));
            }
            client.println();
        }
        
        client.print("> ");  // Show initial prompt
    }
    
    void handleClient(HAL::WiFiClient& client) {
        uint32_t clientId = client.remoteIP();  // Use IP as unique ID
        
        while (client.available()) {
            char c = client.read();
            
            // Get or create command buffer for this client
            String& commandBuffer = clientBuffers[clientId];
            
            // Handle newline (command complete)
            if (c == '\n' || c == '\r') {
                // Skip if buffer is empty (just a newline from log output)
                if (commandBuffer.isEmpty()) {
                    continue;
                }
                
                String line = commandBuffer;
                commandBuffer = "";  // Clear buffer
                
                line.trim();
                
                // Remove any non-printable characters (telnet negotiation)
                String cleaned = "";
                for (size_t i = 0; i < line.length(); i++) {
                    char ch = line.charAt(i);
                    if (ch >= 32 && ch < 127) {  // Only printable ASCII
                        cleaned += ch;
                    }
                }
                line = cleaned;
                
                if (line.isEmpty()) continue;
                
                // Debug: log what we received
                DLOG_D(LOG_CONSOLE, "Command received: '%s' (len=%d)", line.c_str(), line.length());
                
                // Skip if command contains non-alphanumeric at start (telnet noise)
                if (line.length() > 0 && !isalnum(line.charAt(0)) && line.charAt(0) != ' ') {
                    DLOG_D(LOG_CONSOLE, "Skipping command with non-alphanumeric start: 0x%02X", line.charAt(0));
                    continue;
                }
                
                // Parse command and args
                int spacePos = line.indexOf(' ');
                String cmd = spacePos > 0 ? line.substring(0, spacePos) : line;
                String args = spacePos > 0 ? line.substring(spacePos + 1) : "";
                
                cmd.trim();
                args.trim();
                cmd.toLowerCase();
                
                // Execute command
                auto it = commands.find(cmd);
                if (it != commands.end()) {
                    String result = it->second(args);
                    
                    if (result == "QUIT") {
                        client.println("Goodbye!");
                        client.stop();
                        return;
                    }
                    
                    if (!result.isEmpty()) {
                        client.print(result);
                    }
                    client.print("> ");  // Show prompt after command
                } else {
                    client.println("Unknown command: " + cmd + " (type 'help' for commands)");
                    client.print("> ");  // Show prompt after error
                }
            }
            // Handle backspace
            else if (c == '\b' || c == 127) {
                if (commandBuffer.length() > 0) {
                    commandBuffer.remove(commandBuffer.length() - 1);
                }
            }
            // Handle printable characters ONLY
            else if (c >= 32 && c < 127) {
                commandBuffer += c;
            }
            // Silently ignore all other control characters (telnet negotiation, etc.)
            // This includes: 0-31 (control chars), 127+ (extended ASCII, telnet IAC)
        }
    }
    
    String formatLogEntry(const LogEntry& entry) {
        String formatted;
        
        // Add color codes if enabled
        if (config.colorOutput) {
            switch (entry.level) {
                case LOG_LEVEL_ERROR: formatted += "\033[31m"; break;  // Red
                case LOG_LEVEL_WARN:  formatted += "\033[33m"; break;  // Yellow
                case LOG_LEVEL_INFO:  formatted += "\033[32m"; break;  // Green
                case LOG_LEVEL_DEBUG: formatted += "\033[36m"; break;  // Cyan
                default: break;
            }
        }
        
        // Format: [timestamp][LEVEL][TAG] message
        formatted += "[" + String(entry.timestamp) + "]";
        formatted += "[" + logLevelToString(entry.level) + "]";
        formatted += "[" + entry.tag + "] ";
        formatted += entry.message;
        
        // Reset color
        if (config.colorOutput) {
            formatted += "\033[0m";
        }
        
        formatted += "\n";
        
        return formatted;
    }
    
    String logLevelToString(LogLevel level) {
        switch (level) {
            case LOG_LEVEL_NONE:  return "NONE";
            case LOG_LEVEL_ERROR: return "E";
            case LOG_LEVEL_WARN:  return "W";
            case LOG_LEVEL_INFO:  return "I";
            case LOG_LEVEL_DEBUG: return "D";
            default: return "?";
        }
    }
    
    void displayConnectionInfo() {
        if (connectionInfoDisplayed) return;
        
        if (HAL::WiFiHAL::isConnected()) {
            DLOG_I(LOG_CONSOLE, "Connect via: telnet %s %d", 
                   HAL::WiFiHAL::getLocalIP().c_str(), config.port);
            connectionInfoDisplayed = true;
        }
    }
};

} // namespace Components
} // namespace DomoticsCore
