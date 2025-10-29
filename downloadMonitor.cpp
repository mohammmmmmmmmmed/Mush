#include <gtk-3.0/gtk/gtk.h>
#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sstream>
#include <json/json.h>
#include <signal.h>
#include <vector>
#include <cmath>
#include <iomanip>

// Forward declaration for global access
class DownloadMonitorGUI;
DownloadMonitorGUI* g_app = nullptr;

struct NetworkInterface {
    std::string interface;
    double latency;
    int quality;
    double score;
    int signal_strength;
    double speed;
    std::string ssid;
    std::string type;
};

class DownloadMonitorGUI {
private:
    GtkWidget *window;
    GtkWidget *url_entry;
    GtkWidget *output_entry;
    GtkWidget *start_btn;
    GtkWidget *stop_btn;
    GtkWidget *terminal_view;
    GtkWidget *status_bar;
    GtkWidget *interfaces_grid;
    GtkTextBuffer *terminal_buffer;

    pid_t script_pid;
    bool is_running;
    std::string script_path;
    std::vector<NetworkInterface> networks;

public:
    DownloadMonitorGUI() : script_pid(-1), is_running(false) {}

    void create_window() {
        // Main window
        window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(window), "Multi-Interface Download Monitor");
        gtk_window_set_default_size(GTK_WINDOW(window), 900, 700);
        gtk_container_set_border_width(GTK_CONTAINER(window), 10);

        g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

        GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
        gtk_container_add(GTK_CONTAINER(window), vbox);

        GtkWidget *title = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(title),
            "<span font='20' weight='bold'>Multi-Interface Download Monitor</span>");
        gtk_box_pack_start(GTK_BOX(vbox), title, FALSE, FALSE, 10);

        // Configuration frame
        GtkWidget *config_frame = gtk_frame_new("Configuration");
        gtk_box_pack_start(GTK_BOX(vbox), config_frame, FALSE, FALSE, 5);

        GtkWidget *config_grid = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(config_grid), 8);
        gtk_grid_set_column_spacing(GTK_GRID(config_grid), 10);
        gtk_container_set_border_width(GTK_CONTAINER(config_grid), 10);
        gtk_container_add(GTK_CONTAINER(config_frame), config_grid);

        // URL
        gtk_grid_attach(GTK_GRID(config_grid), gtk_label_new("Download URL:"), 0, 0, 1, 1);
        url_entry = gtk_entry_new();
        gtk_entry_set_width_chars(GTK_ENTRY(url_entry), 70);
        gtk_grid_attach(GTK_GRID(config_grid), url_entry, 1, 0, 1, 1);

        // Output file
        gtk_grid_attach(GTK_GRID(config_grid), gtk_label_new("Output File:"), 0, 1, 1, 1);
        output_entry = gtk_entry_new();
        gtk_entry_set_text(GTK_ENTRY(output_entry), "download");
        gtk_entry_set_width_chars(GTK_ENTRY(output_entry), 70);
        gtk_grid_attach(GTK_GRID(config_grid), output_entry, 1, 1, 1, 1);

        // Interfaces frame
        GtkWidget *ifaces_frame = gtk_frame_new("Network Interfaces (from networks.json)");
        gtk_box_pack_start(GTK_BOX(vbox), ifaces_frame, FALSE, FALSE, 5);

        GtkWidget *scrolled_ifaces = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_ifaces),
                                       GTK_POLICY_AUTOMATIC,
                                       GTK_POLICY_AUTOMATIC);
        gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scrolled_ifaces), 100);
        gtk_container_add(GTK_CONTAINER(ifaces_frame), scrolled_ifaces);

        interfaces_grid = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(interfaces_grid), 5);
        gtk_grid_set_column_spacing(GTK_GRID(interfaces_grid), 10);
        gtk_container_set_border_width(GTK_CONTAINER(interfaces_grid), 10);
        gtk_container_add(GTK_CONTAINER(scrolled_ifaces), interfaces_grid);

        // Buttons
        GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_box_set_homogeneous(GTK_BOX(btn_box), TRUE);
        gtk_box_pack_start(GTK_BOX(vbox), btn_box, FALSE, FALSE, 5);

        start_btn = gtk_button_new_with_label("▶ Start Download");
        g_signal_connect(start_btn, "clicked", G_CALLBACK(on_start_clicked_static), this);
        gtk_box_pack_start(GTK_BOX(btn_box), start_btn, TRUE, TRUE, 0);

        stop_btn = gtk_button_new_with_label("⏹ Stop");
        gtk_widget_set_sensitive(stop_btn, FALSE);
        g_signal_connect(stop_btn, "clicked", G_CALLBACK(on_stop_clicked_static), this);
        gtk_box_pack_start(GTK_BOX(btn_box), stop_btn, TRUE, TRUE, 0);

        // Terminal frame
        GtkWidget *terminal_frame = gtk_frame_new("📟 Live Terminal Output");
        gtk_box_pack_start(GTK_BOX(vbox), terminal_frame, TRUE, TRUE, 5);

        GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                       GTK_POLICY_AUTOMATIC,
                                       GTK_POLICY_AUTOMATIC);
        gtk_container_add(GTK_CONTAINER(terminal_frame), scrolled);

        terminal_view = gtk_text_view_new();
        gtk_text_view_set_editable(GTK_TEXT_VIEW(terminal_view), FALSE);
        gtk_text_view_set_monospace(GTK_TEXT_VIEW(terminal_view), TRUE);
        gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(terminal_view), GTK_WRAP_WORD);

        GdkRGBA bg_color = {0.04, 0.04, 0.04, 1.0};
        GdkRGBA fg_color = {0.0, 1.0, 0.0, 1.0};
        gtk_widget_override_background_color(terminal_view, GTK_STATE_FLAG_NORMAL, &bg_color);
        gtk_widget_override_color(terminal_view, GTK_STATE_FLAG_NORMAL, &fg_color);

        terminal_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(terminal_view));
        gtk_container_add(GTK_CONTAINER(scrolled), terminal_view);

        status_bar = gtk_label_new("Ready");
        gtk_widget_set_halign(status_bar, GTK_ALIGN_START);
        GdkRGBA status_bg = {0.06, 0.2, 0.37, 1.0};
        gtk_widget_override_background_color(status_bar, GTK_STATE_FLAG_NORMAL, &status_bg);
        gtk_box_pack_start(GTK_BOX(vbox), status_bar, FALSE, FALSE, 0);

        // Load interfaces from networks.json and display them
        load_networks_from_json();
        display_interfaces();

        gtk_widget_show_all(window);
    }

    void load_networks_from_json() {
        std::ifstream ifs("networks.json");
        if (!ifs.is_open()) {
            std::cerr << "Warning: Could not open networks.json" << std::endl;
            return;
        }

        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(ifs, root)) {
            std::cerr << "Warning: Could not parse networks.json" << std::endl;
            return;
        }

        if (root.isMember("networks") && root["networks"].isArray()) {
            for (const auto& network : root["networks"]) {
                NetworkInterface iface;
                iface.interface = network["interface"].asString();
                iface.latency = network["latency"].asDouble();
                iface.quality = network["quality"].asInt();
                iface.score = network["score"].asDouble();
                iface.signal_strength = network["signal_strength"].asInt();
                iface.speed = network["speed"].asDouble();
                iface.ssid = network["ssid"].asString();
                iface.type = network["type"].asString();
                networks.push_back(iface);
            }
        }

        std::cout << "Loaded " << networks.size() << " network interface(s) from networks.json" << std::endl;
    }

    void display_interfaces() {
        // Headers
        gtk_grid_attach(GTK_GRID(interfaces_grid), gtk_label_new("Interface"), 0, 0, 1, 1);
        gtk_grid_attach(GTK_GRID(interfaces_grid), gtk_label_new("Type"), 1, 0, 1, 1);
        gtk_grid_attach(GTK_GRID(interfaces_grid), gtk_label_new("Speed (MB/s)"), 2, 0, 1, 1);
        gtk_grid_attach(GTK_GRID(interfaces_grid), gtk_label_new("Latency (ms)"), 3, 0, 1, 1);
        gtk_grid_attach(GTK_GRID(interfaces_grid), gtk_label_new("Score"), 4, 0, 1, 1);
        gtk_grid_attach(GTK_GRID(interfaces_grid), gtk_label_new("Allocation %"), 5, 0, 1, 1);

        // Calculate total score (using absolute values for better scores)
        double total_score = 0;
        for (const auto& net : networks) {
            total_score += std::abs(net.score);
        }

        // Display each interface
        for (size_t i = 0; i < networks.size(); i++) {
            const auto& net = networks[i];
            double allocation = total_score > 0 ? (std::abs(net.score) / total_score) * 100 : 0;

            gtk_grid_attach(GTK_GRID(interfaces_grid), 
                gtk_label_new(net.interface.c_str()), 0, i + 1, 1, 1);
            gtk_grid_attach(GTK_GRID(interfaces_grid), 
                gtk_label_new(net.type.c_str()), 1, i + 1, 1, 1);
            
            std::ostringstream speed_str;
            speed_str << std::fixed << std::setprecision(4) << net.speed;
            gtk_grid_attach(GTK_GRID(interfaces_grid), 
                gtk_label_new(speed_str.str().c_str()), 2, i + 1, 1, 1);
            
            std::ostringstream latency_str;
            latency_str << std::fixed << std::setprecision(2) << net.latency;
            gtk_grid_attach(GTK_GRID(interfaces_grid), 
                gtk_label_new(latency_str.str().c_str()), 3, i + 1, 1, 1);
            
            std::ostringstream score_str;
            score_str << std::fixed << std::setprecision(2) << net.score;
            gtk_grid_attach(GTK_GRID(interfaces_grid), 
                gtk_label_new(score_str.str().c_str()), 4, i + 1, 1, 1);
            
            std::ostringstream alloc_str;
            alloc_str << std::fixed << std::setprecision(2) << allocation << "%";
            gtk_grid_attach(GTK_GRID(interfaces_grid), 
                gtk_label_new(alloc_str.str().c_str()), 5, i + 1, 1, 1);
        }
    }

    void append_terminal(const std::string& text) {
        GtkTextIter end;
        gtk_text_buffer_get_end_iter(terminal_buffer, &end);
        gtk_text_buffer_insert(terminal_buffer, &end, text.c_str(), -1);

        GtkTextMark *mark = gtk_text_buffer_get_insert(terminal_buffer);
        gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(terminal_view), mark, 0.0, TRUE, 0.0, 1.0);
    }

    std::string generate_bash_script(const std::string& url, const std::string& output) {
        std::ostringstream script;
        script << "#!/bin/bash\n";
        script << "set -e\n\n";
        script << "URL='" << url << "'\n";
        script << "OUTPUT=\"" << output << "\"\n\n";

        script << "TMP_DIR=$(mktemp -d)\n";
        script << "trap 'rm -rf \"$TMP_DIR\"' EXIT\n\n";

        script << "echo \"Getting file size...\"\n";
        script << "FILE_SIZE=$(curl --globoff -sI \"${URL}\" | grep -i '^Content-Length:' | tail -1 | awk '{print $2}' | tr -d '\\r\\n')\n";
        script << "if [ -z \"$FILE_SIZE\" ] || [ \"$FILE_SIZE\" -eq 0 ] 2>/dev/null; then\n";
        script << "    echo \"Could not get Content-Length, trying HEAD request...\"\n";
        script << "    FILE_SIZE=$(curl --globoff -sL -D - \"${URL}\" -o /dev/null | grep -i '^Content-Length:' | tail -1 | awk '{print $2}' | tr -d '\\r\\n')\n";
        script << "fi\n";
        script << "if [ -z \"$FILE_SIZE\" ] || ! [[ \"$FILE_SIZE\" =~ ^[0-9]+$ ]]; then\n";
        script << "    echo \"Error: Could not determine file size. Server may not support range requests.\"\n";
        script << "    echo \"Attempting single connection download...\"\n";
        script << "    curl --globoff -L \"${URL}\" -o \"$OUTPUT\"\n";
        script << "    echo \"Download complete (single connection). Saved as $OUTPUT\"\n";
        script << "    exit 0\n";
        script << "fi\n";
        script << "echo \"File size: $FILE_SIZE bytes\"\n\n";

        // Calculate total score
        double total_score = 0;
        for (const auto& net : networks) {
            total_score += std::abs(net.score);
        }

        // Generate download commands for each interface
        for (size_t i = 0; i < networks.size(); i++) {
            const auto& net = networks[i];
            double allocation = std::abs(net.score) / total_score;
            
            script << "# Interface " << (i + 1) << ": " << net.interface 
                   << " (Score: " << net.score << ", Allocation: " 
                   << std::fixed << std::setprecision(2) << (allocation * 100) << "%)\n";
            
            script << "IFACE" << (i + 1) << "=\"" << net.interface << "\"\n";
            script << "FILE" << (i + 1) << "=\"$TMP_DIR/part" << (i + 1) << "\"\n";
            
            if (i == 0) {
                // First interface
                script << "CHUNK_SIZE" << (i + 1) << "=$(( $FILE_SIZE * " 
                       << (int)(allocation * 100) << " / 100 ))\n";
                script << "START" << (i + 1) << "=0\n";
                script << "END" << (i + 1) << "=$(( $CHUNK_SIZE" << (i + 1) << " - 1 ))\n";
            } else if (i < networks.size() - 1) {
                // Middle interfaces
                script << "CHUNK_SIZE" << (i + 1) << "=$(( $FILE_SIZE * " 
                       << (int)(allocation * 100) << " / 100 ))\n";
                script << "START" << (i + 1) << "=$(( $END" << i << " + 1 ))\n";
                script << "END" << (i + 1) << "=$(( $START" << (i + 1) 
                       << " + $CHUNK_SIZE" << (i + 1) << " - 1 ))\n";
            } else {
                // Last interface gets the remainder
                script << "START" << (i + 1) << "=$(( $END" << i << " + 1 ))\n";
                script << "END" << (i + 1) << "=$(( $FILE_SIZE - 1 ))\n";
            }
            
            script << "echo \"Interface " << net.interface << " downloading bytes $START" 
                   << (i + 1) << " to $END" << (i + 1) << " ($(( $END" << (i + 1) 
                   << " - $START" << (i + 1) << " + 1 )) bytes)\"\n\n";
            
            // Bring interface up and get IP
            script << "ip link set $IFACE" << (i + 1) << " up 2>/dev/null || true\n";
            script << "sleep 1\n";
            script << "IP" << (i + 1) << "=$(ip addr show $IFACE" << (i + 1) 
                   << " | grep 'inet ' | head -1 | awk '{print $2}' | cut -d/ -f1)\n";
            
            // Add routing table and rules for this interface
            script << "if [ -n \"$IP" << (i + 1) << "\" ]; then\n";
            script << "    GATEWAY" << (i + 1) << "=$(ip route | grep \"default.*$IFACE" 
                   << (i + 1) << "\" | awk '{print $3}')\n";
            script << "    if [ -n \"$GATEWAY" << (i + 1) << "\" ]; then\n";
            script << "        TABLE_ID=" << (100 + i) << "\n";
            script << "        ip route add default via $GATEWAY" << (i + 1) 
                   << " dev $IFACE" << (i + 1) << " table $TABLE_ID 2>/dev/null || true\n";
            script << "        ip rule add from $IP" << (i + 1) 
                   << " table $TABLE_ID 2>/dev/null || true\n";
            script << "        echo \"Added routing rule for $IFACE" << (i + 1) 
                   << " via $GATEWAY" << (i + 1) << "\"\n";
            script << "    fi\n";
            script << "fi\n\n";
            
            script << "if [ -z \"$IP" << (i + 1) << "\" ]; then\n";
            script << "    echo \"Warning: Could not get IP for $IFACE" << (i + 1) << "\"\n";
            script << "    echo \"Attempting download without binding...\"\n";
            script << "    curl --globoff -L -r $START" << (i + 1) << "-$END" << (i + 1) 
                   << " --max-time 0 --connect-timeout 30"
                   << " \"${URL}\" -o \"$FILE" << (i + 1) << "\" 2>&1 &\n";
            script << "else\n";
            script << "    echo \"Using IP: $IP" << (i + 1) << " for $IFACE" << (i + 1) << "\"\n";
            script << "    curl --globoff -L --interface $IP" << (i + 1) 
                   << " -r $START" << (i + 1) << "-$END" << (i + 1) 
                   << " --max-time 0 --connect-timeout 30"
                   << " \"${URL}\" -o \"$FILE" << (i + 1) << "\" 2>&1 &\n";
            script << "fi\n";
            script << "PID" << (i + 1) << "=$!\n\n";
        }

        // Wait for all downloads
        script << "echo \"Waiting for all downloads to complete...\"\n";
        for (size_t i = 0; i < networks.size(); i++) {
            script << "wait $PID" << (i + 1) << "\n";
            script << "EXITCODE" << (i + 1) << "=$?\n";
            script << "if [ $EXITCODE" << (i + 1) << " -eq 0 ]; then\n";
            script << "    echo \"Interface " << (i + 1) << " download complete\"\n";
            script << "else\n";
            script << "    echo \"Interface " << (i + 1) << " download failed with code $EXITCODE" << (i + 1) << "\"\n";
            script << "fi\n";
        }
        script << "\n";

        // Cleanup routing rules
        script << "# Cleanup routing rules\n";
        for (size_t i = 0; i < networks.size(); i++) {
            script << "if [ -n \"$IP" << (i + 1) << "\" ]; then\n";
            script << "    TABLE_ID=" << (100 + i) << "\n";
            script << "    sudo ip rule del from $IP" << (i + 1) << " table $TABLE_ID 2>/dev/null || true\n";
            script << "    sudo ip route flush table $TABLE_ID 2>/dev/null || true\n";
            script << "fi\n";
        }
        script << "\n";

        // Merge files
        script << "echo \"Merging files...\"\n";
        script << "cat";
        for (size_t i = 0; i < networks.size(); i++) {
            script << " \"$FILE" << (i + 1) << "\"";
        }
        script << " > \"$OUTPUT\"\n";
        script << "echo \"Download complete. Saved as $OUTPUT\"\n";
        script << "ls -lh \"$OUTPUT\"\n";

        return script.str();
    }

    static gboolean update_terminal_idle(gpointer data) {
        auto* pair = static_cast<std::pair<DownloadMonitorGUI*, std::string*>*>(data);
        pair->first->append_terminal(*pair->second);
        delete pair->second;
        delete pair;
        return FALSE;
    }

    void start_download() {
        if (is_running) return;

        if (networks.empty()) {
            GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                GTK_DIALOG_MODAL,
                GTK_MESSAGE_ERROR,
                GTK_BUTTONS_OK,
                "No network interfaces found in networks.json!");
            gtk_dialog_run(GTK_DIALOG(dialog));
            gtk_widget_destroy(dialog);
            return;
        }

        const char* url = gtk_entry_get_text(GTK_ENTRY(url_entry));
        const char* output = gtk_entry_get_text(GTK_ENTRY(output_entry));

        if (strlen(url) == 0 || strlen(output) == 0) {
            GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                GTK_DIALOG_MODAL,
                GTK_MESSAGE_ERROR,
                GTK_BUTTONS_OK,
                "Please fill in all fields!");
            gtk_dialog_run(GTK_DIALOG(dialog));
            gtk_widget_destroy(dialog);
            return;
        }

        std::string script = generate_bash_script(url, output);

        char temp_template[] = "/tmp/download_XXXXXX.sh";
        int fd = mkstemps(temp_template, 3);
        if (fd == -1) {
            append_terminal("Failed to create temporary script file.\n");
            return;
        }

        script_path = temp_template;
        write(fd, script.c_str(), script.size());
        close(fd);
        chmod(script_path.c_str(), 0755);

        append_terminal("Starting download script...\n");
        gtk_widget_set_sensitive(start_btn, FALSE);
        gtk_widget_set_sensitive(stop_btn, TRUE);
        is_running = true;

        std::thread([this]() {
            int pipefd[2];
            if (pipe(pipefd) == -1) {
                g_idle_add([](gpointer data) -> gboolean {
                    static_cast<DownloadMonitorGUI*>(data)->append_terminal("Failed to create pipe\n");
                    return FALSE;
                }, this);
                return;
            }

            script_pid = fork();
            if (script_pid == 0) {
                // Child process
                close(pipefd[0]); // Close read end
                dup2(pipefd[1], STDOUT_FILENO);
                dup2(pipefd[1], STDERR_FILENO);
                close(pipefd[1]);
                execl(script_path.c_str(), script_path.c_str(), NULL);
                _exit(1);
            } else {
                // Parent process
                close(pipefd[1]); // Close write end

                // Read output from pipe
                char buffer[1024];
                ssize_t count;
                while ((count = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
                    buffer[count] = '\0';
                    std::string* output = new std::string(buffer);
                    auto* pair = new std::pair<DownloadMonitorGUI*, std::string*>(this, output);
                    g_idle_add(update_terminal_idle, pair);
                }
                close(pipefd[0]);

                int status;
                waitpid(script_pid, &status, 0);
                
                g_idle_add([](gpointer data) -> gboolean {
                    DownloadMonitorGUI* app = static_cast<DownloadMonitorGUI*>(data);
                    gtk_widget_set_sensitive(app->start_btn, TRUE);
                    gtk_widget_set_sensitive(app->stop_btn, FALSE);
                    app->is_running = false;
                    app->append_terminal("\nDownload finished.\n");
                    return FALSE;
                }, this);
            }
        }).detach();
    }

    void stop_download() {
        if (!is_running) return;
        if (script_pid > 0) {
            kill(script_pid, SIGTERM);
            append_terminal("Download stopped.\n");
            gtk_widget_set_sensitive(start_btn, TRUE);
            gtk_widget_set_sensitive(stop_btn, FALSE);
            is_running = false;
        }
    }

    static void on_start_clicked_static(GtkWidget *widget, gpointer data) {
        static_cast<DownloadMonitorGUI*>(data)->start_download();
    }

    static void on_stop_clicked_static(GtkWidget *widget, gpointer data) {
        static_cast<DownloadMonitorGUI*>(data)->stop_download();
    }
};

int main(int argc, char* argv[]) {
    gtk_init(&argc, &argv);
    g_app = new DownloadMonitorGUI();
    g_app->create_window();
    gtk_main();
    delete g_app;
    return 0;
}