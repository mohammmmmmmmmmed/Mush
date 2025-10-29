# Multi-Interface Download Manager

A high-performance download manager that utilizes multiple network interfaces simultaneously to accelerate downloads. The application intelligently distributes download chunks across available network interfaces based on their performance metrics.

## Features

- **Network Interface Analysis**: Automatically detects and analyzes all available network interfaces
- **Performance Metrics**: Measures latency and throughput for each interface
- **Intelligent Load Balancing**: Distributes download chunks based on interface performance
- **GTK-based GUI**: User-friendly interface for monitoring and controlling downloads
- **Bash Script Generation**: Dynamically generates optimized download scripts
- **Network Routing**: Automatically manages routing tables for optimal performance

## Prerequisites

- Linux-based operating system
- g++ (GNU C++ Compiler)
- GTK+ 3.0 development libraries
- libjsoncpp-dev
- pkg-config
- curl
- root/administrative privileges (for network configuration)

## Installation

1. Install required dependencies:

```bash
# On Debian/Ubuntu
sudo apt-get update
sudo apt-get install -y g++ libgtk-3-dev libjsoncpp-dev pkg-config curl

# On Fedora
sudo dnf install -y gcc-c++ gtk3-devel jsoncpp-devel pkg-config curl

# On Arch Linux
sudo pacman -S gcc gtk3 jsoncpp pkgconf curl
```

2. Clone the repository:
```bash
git clone https://github.com/mohammmmmmmmmmed/Mush
cd Mush
```

3. Compile the applications:
```bash
# Compile network monitor
g++ -o network networkMonitor.cpp `pkg-config --cflags --libs gtk+-3.0` -std=c++11 -pthread

# Compile download monitor UI
g++ -o downloadMonitor downloadMonitor.cpp `pkg-config --cflags --libs gtk+-3.0` -ljsoncpp -std=c++11 -pthread
```

## Usage

1. First, run the network monitor to analyze your network interfaces:
```bash
./networkMonitor
```
This will generate a `networks.json` file containing information about your network interfaces.

2. Then, run the download manager:
```bash
./downloadMonitor
```

3. Run the network monitor to analyze your interfaces:
   ```bash
   ./network
   ```
   This will generate a `networks.json` file with your network interface metrics.

4. Run the download manager (requires root for network configuration):
   ```bash
   sudo ./downloadMonitor
   ```

5. In the GUI:
   - Enter the download URL
   - Specify the output filename
   - Click "Start Download" to begin
   - Monitor progress in the terminal output section

## Demo

Here's a quick demo of the Multi-Interface Download Manager in action:

https://github.com/mohammmmmmmmmmed/Mush/raw/main/Sample.mp4

## How It Works

1. **Network Analysis**:
   - The `networkMonitor` scans all available network interfaces
   - Measures latency (ping to 8.8.8.8)
   - Calculates throughput by monitoring /proc/net/dev
   - Saves results to `networks.json`

2. **Download Management**:
   - The `downloadMonitor` reads network information from `networks.json`
   - Generates an optimized bash script for parallel downloads
   - Splits the file into chunks based on interface performance
   - Manages network routing for optimal performance
   - Merges downloaded chunks into the final file

## Configuration

The application uses a `networks.json` file with the following structure:
```json
{
  "networks": [
    {
      "interface": "eth0",
      "latency": 12.34,
      "quality": 0,
      "score": 1234.56,
      "signal_strength": -1,
      "speed": 12.3456,
      "ssid": "",
      "type": "ethernet"
    }
  ]
}
```

## Monitoring Interface Traffic

To monitor traffic on individual interfaces, you can use the `iftop` tool. This is particularly useful for verifying that the download is being distributed across multiple interfaces.

### Installing iftop:

```bash
# On Debian/Ubuntu
sudo apt-get install iftop

# On Fedora
sudo dnf install iftop

# On Arch Linux
sudo pacman -S iftop
```

### Example Usage:

Monitor traffic on a specific interface (e.g., eth0):
```bash
sudo iftop -i eth0
```

Monitor traffic on all interfaces:
```bash
sudo iftop -nNb
```

Key bindings in iftop:
- `n` - Toggle DNS hostname resolution
- `s` - Show source port
- `d` - Show destination port
- `t` - Toggle port display
- `q` - Quit

## Platform Compatibility

This project is primarily developed and tested on **Arch Linux**. While it should work on other Linux distributions, you may encounter some differences. We welcome:

- Testing on other Linux distributions
- Bug reports for compatibility issues
- Pull requests with fixes and improvements for other platforms

## Troubleshooting

- **Permission Issues**: Run with `sudo` if you encounter permission errors
- **Missing Dependencies**: Ensure all required packages are installed
- **Network Issues**: Check that all interfaces are properly configured and have internet access
- **Distribution-Specific Issues**: If you encounter issues on non-Arch distributions, please report them with details about your distribution and version

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome! Please follow these steps:
1. Fork the repository
2. Create a new branch for your feature
3. Commit your changes
4. Push to the branch
5. Create a new Pull Request
