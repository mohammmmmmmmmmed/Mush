#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <stdexcept>
#include <json/json.h>

// Execute a shell command and return its output
std::string execCmd(const std::string& cmd) {
    std::array<char, 128> buffer;
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) throw std::runtime_error("popen() failed!");
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    return result;
}

// Get latency via ping (1 packet)
double getLatency(const std::string& iface) {
    try {
        std::string cmd = "ping -I " + iface + " -c 1 -w 1 8.8.8.8 2>/dev/null | grep 'time='";
        std::string output = execCmd(cmd);

        size_t pos = output.find("time=");
        if (pos == std::string::npos) return -1.0;

        size_t end = output.find(" ms", pos);
        std::string t = output.substr(pos + 5, end - (pos + 5));
        return std::stod(t);
    } catch (...) {
        return -1.0;
    }
}

// Get throughput in KB/s using /proc/net/dev delta over 8 seconds
double getThroughput(const std::string& iface) {
    auto readBytes = [&](const std::string& iface) -> uint64_t {
        std::ifstream file("/proc/net/dev");
        std::string line;
        while (std::getline(file, line)) {
            if (line.find(iface + ":") != std::string::npos) {
                std::istringstream ss(line.substr(line.find(":") + 1));
                uint64_t rx = 0, tx = 0;
                ss >> rx;
                for (int i = 0; i < 7; i++) ss >> std::ws >> rx; // skip to tx bytes
                ss >> tx;
                return rx + tx;
            }
        }
        return 0;
    };

    uint64_t b1 = readBytes(iface);
    std::this_thread::sleep_for(std::chrono::seconds(8)); // measure over 8s
    uint64_t b2 = readBytes(iface);

    return static_cast<double>(b2 - b1) / (1024.0 * 8.0); // KB/s average
}

int main() {
    Json::Value root;
    root["networks"] = Json::arrayValue;

    std::string ifaceList = execCmd("ls /sys/class/net");
    std::istringstream ss(ifaceList);
    std::string iface;

    while (ss >> iface) {
        if (iface == "lo") continue; // skip loopback
        if (iface.length() <= 6) {
            std::cerr << "Skipping short interface name: " << iface << std::endl;
            continue;
        }

        Json::Value net;
        net["interface"] = iface;
        net["ssid"] = "";
        net["type"] = (iface.find("wl") == 0) ? "wifi" : "ethernet";

        double latency = getLatency(iface);
        double speed = getThroughput(iface);

        if (latency < -0.5 && speed < 0.01) {
            std::cerr << "Skipping inactive/problematic interface: " << iface << std::endl;
            continue;
        }

        net["latency"] = latency;
        net["speed"] = speed;
        net["signal_strength"] = -1; // placeholder
        net["quality"] = 0;          // placeholder
        net["score"] = speed - latency;

        root["networks"].append(net);
    }

    std::ofstream file("networks.json");
    if (!file) {
        std::cerr << "Failed to open networks.json for writing!" << std::endl;
        return 1;
    }
    file << root.toStyledString();
    file.close();

    std::cout << "Network scan saved to networks.json" << std::endl;
    return 0;
}
