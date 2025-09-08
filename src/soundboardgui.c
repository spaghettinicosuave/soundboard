#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pango/pango.h>
// Global variable to track if we're waiting for a key
static gboolean waiting_for_key = FALSE;
static int pending_sound_id = 0;
// Dependency Checking
typedef struct {
    const char *command;
    const char *package_name;
    const char *description;
    int required;
} Dependency;
// Define required dependencies
static Dependency dependencies[] = {
    {"pactl", "pulseaudio-utils", "PulseAudio control utility", 1},
    {"paplay", "pulseaudio-utils", "PulseAudio playback utility", 1},
    {"xbindkeys", "xbindkeys", "Global hotkey daemon", 1},
    {"bash", "bash", "Bash shell", 1},
    {NULL, NULL, NULL, 0} // Sentinel
};
// Check if a command exists
int command_exists(const char *command) {
    char test_cmd[256];
    snprintf(test_cmd, sizeof(test_cmd), "command -v %s >/dev/null 2>&1", command);
    return system(test_cmd) == 0;
}
// Show dependency error dialog
// Show dependency error dialog (ASCII-ONLY VERSION)
void show_dependency_dialog(GtkWidget *parent, const char *missing_deps) {
    GtkWidget *dialog;
    char message[1024];
    snprintf(message, sizeof(message),
             "Missing Required Dependencies\n\n"
             "The following packages are required but not installed:\n%s\n\n"
             "Please install them using your package manager:\n"
             "- Ubuntu/Debian: sudo apt install %s\n"
             "- Fedora: sudo dnf install %s\n"
             "- Arch: sudo pacman -S %s\n\n"
             "Note: Most modern systems use PipeWire with PulseAudio compatibility.\n"
             "If you have PipeWire, make sure pipewire-pulse is installed.\n\n"
             "The application will now exit.",
             missing_deps, missing_deps, missing_deps, missing_deps);
    dialog = gtk_message_dialog_new(GTK_WINDOW(parent),
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_ERROR,
                                    GTK_BUTTONS_OK,
                                    "%s", message);
    gtk_window_set_title(GTK_WINDOW(dialog), "Dependency Error");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
}
// Main dependency check function
int check_dependencies(GtkWidget *parent_window) {
    char missing_deps[512] = "";
    int missing_count = 0;
    printf("Checking system dependencies...\n");
    for (int i = 0; dependencies[i].command != NULL; i++) {
        printf("Checking for %s... ", dependencies[i].command);
        if (command_exists(dependencies[i].command)) {
            printf("found\n");
        } else {
            printf("MISSING\n");
            if (dependencies[i].required) {
                if (missing_count > 0) {
                    strcat(missing_deps, " ");
                }
                strcat(missing_deps, dependencies[i].package_name);
                missing_count++;
            }
        }
    }
    if (missing_count > 0) {
        printf("\nERROR: Missing %d required dependencies\n", missing_count);
        show_dependency_dialog(parent_window, missing_deps);
        return 0; // Dependencies missing
    }
    printf("All dependencies satisfied!\n");
    return 1; // All good
}
int initialize_with_dependency_check() {
    // Check dependencies before creating GUI
    if (!check_dependencies(NULL)) {
        return 0; // Exit if dependencies missing
    }
    return 1;
}
// Structure to hold sound information
typedef struct {
    int id;
    char *filename;
    char *keybind;
    char *description;
} SoundInfo;
// Structure to hold all our GUI data
typedef struct {
    GtkWidget *window;
    GtkWidget *grid;
    GtkWidget *scrolled_window;
    SoundInfo *sounds;
    int sound_count;
    int grid_columns;
} AppData;
// Global app data
AppData app_data = {0};
// Function to free all allocated memory
void cleanup_sounds() {
    if (app_data.sounds) {
        for (int i = 0; i < app_data.sound_count; i++) {
            if (app_data.sounds[i].filename) {
                free(app_data.sounds[i].filename);
                app_data.sounds[i].filename = NULL;
            }
            if (app_data.sounds[i].keybind) {
                free(app_data.sounds[i].keybind);
                app_data.sounds[i].keybind = NULL;
            }
            if (app_data.sounds[i].description) {
                free(app_data.sounds[i].description);
                app_data.sounds[i].description = NULL;
            }
        }
        free(app_data.sounds);
        app_data.sounds = NULL;
        app_data.sound_count = 0;
    }
}
// Function to convert GDK key to string format that bash script expects
const char* gdk_key_to_string(guint keyval) {
    switch(keyval) {
        // Number pad keys
        case GDK_KEY_KP_0: return "KP_0";
        case GDK_KEY_KP_1: return "KP_1";
        case GDK_KEY_KP_2: return "KP_2";
        case GDK_KEY_KP_3: return "KP_3";
        case GDK_KEY_KP_4: return "KP_4";
        case GDK_KEY_KP_5: return "KP_5";
        case GDK_KEY_KP_6: return "KP_6";
        case GDK_KEY_KP_7: return "KP_7";
        case GDK_KEY_KP_8: return "KP_8";
        case GDK_KEY_KP_9: return "KP_9";
        // Regular number keys
        case GDK_KEY_0: return "0";
        case GDK_KEY_1: return "1";
        case GDK_KEY_2: return "2";
        case GDK_KEY_3: return "3";
        case GDK_KEY_4: return "4";
        case GDK_KEY_5: return "5";
        case GDK_KEY_6: return "6";
        case GDK_KEY_7: return "7";
        case GDK_KEY_8: return "8";
        case GDK_KEY_9: return "9";
        // Letters
        case GDK_KEY_a: case GDK_KEY_A: return "a";
        case GDK_KEY_b: case GDK_KEY_B: return "b";
        case GDK_KEY_c: case GDK_KEY_C: return "c";
        case GDK_KEY_d: case GDK_KEY_D: return "d";
        case GDK_KEY_e: case GDK_KEY_E: return "e";
        case GDK_KEY_f: case GDK_KEY_F: return "f";
        case GDK_KEY_g: case GDK_KEY_G: return "g";
        case GDK_KEY_h: case GDK_KEY_H: return "h";
        case GDK_KEY_i: case GDK_KEY_I: return "i";
        case GDK_KEY_j: case GDK_KEY_J: return "j";
        case GDK_KEY_k: case GDK_KEY_K: return "k";
        case GDK_KEY_l: case GDK_KEY_L: return "l";
        case GDK_KEY_m: case GDK_KEY_M: return "m";
        case GDK_KEY_n: case GDK_KEY_N: return "n";
        case GDK_KEY_o: case GDK_KEY_O: return "o";
        case GDK_KEY_p: case GDK_KEY_P: return "p";
        case GDK_KEY_q: case GDK_KEY_Q: return "q";
        case GDK_KEY_r: case GDK_KEY_R: return "r";
        case GDK_KEY_s: case GDK_KEY_S: return "s";
        case GDK_KEY_t: case GDK_KEY_T: return "t";
        case GDK_KEY_u: case GDK_KEY_U: return "u";
        case GDK_KEY_v: case GDK_KEY_V: return "v";
        case GDK_KEY_w: case GDK_KEY_W: return "w";
        case GDK_KEY_x: case GDK_KEY_X: return "x";
        case GDK_KEY_y: case GDK_KEY_Y: return "y";
        case GDK_KEY_z: case GDK_KEY_Z: return "z";
        //Weird keys
        case GDK_KEY_Page_Up: return "KP_PGUP";
        case GDK_KEY_Page_Down: return "KP_PGDOWN";
        case GDK_KEY_KP_Multiply: return "KP_Multiply";
        case GDK_KEY_KP_Divide: return "KP_Divide";
        case GDK_KEY_KP_Subtract: return "KP_Subtract";
        case GDK_KEY_KP_Add: return "KP_Add";
        case GDK_KEY_Delete: return "Delete";
        case GDK_KEY_KP_Decimal: return "KP_Decimal";
        // Function keys
        case GDK_KEY_F1: return "F1";
        case GDK_KEY_F2: return "F2";
        case GDK_KEY_F3: return "F3";
        case GDK_KEY_F4: return "F4";
        case GDK_KEY_F5: return "F5";
        case GDK_KEY_F6: return "F6";
        case GDK_KEY_F7: return "F7";
        case GDK_KEY_F8: return "F8";
        case GDK_KEY_F9: return "F9";
        case GDK_KEY_F10: return "F10";
        case GDK_KEY_F11: return "F11";
        case GDK_KEY_F12: return "F12";
        // Escape to cancel
        case GDK_KEY_Escape: return NULL; // Special case for canceling
        default: return NULL; // Unsupported key
    }
}
// Setup callback
void setup_callback(GtkWidget *widget, gpointer data) {
    printf("Setting up Soundboard\n");
    const char *home = getenv("HOME");
    if (!home) {
        printf("Error: HOME environment variable not set\n");
        return;
    }
    char command[1024];
    snprintf(command, sizeof(command), "%s/soundboard/soundboard.sh setup", home);
    printf("Executing: %s\n", command);
    int result = system(command);
    if (result == -1) {
        printf("Error: Failed to execute setup command\n");
    } else if (result != 0) {
        printf("Setup command failed with exit code: %d\n", WEXITSTATUS(result));
    } else {
        printf("Setup command executed successfully\n");
    }
}
// Key press event handler
static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    if (!waiting_for_key) {
        return FALSE; // Let other handlers process the key
    }
    const char *key_string = gdk_key_to_string(event->keyval);
    if (key_string == NULL && event->keyval == GDK_KEY_Escape) {
        // User pressed Escape to cancel
        g_print("Key binding canceled\n");
        waiting_for_key = FALSE;
        pending_sound_id = 0;
        setup_callback(NULL, NULL);
        return TRUE;
    }
    if (key_string == NULL) {
        g_print("Unsupported key pressed. Try again or press Escape to cancel.\n");
        return TRUE; // Consume the event but don't process it
    }
    // We got a valid key, now send the bind command
    char command[1024];
    const char *home = getenv("HOME");
    if (!home) {
        printf("Error: HOME environment variable not set\n");
        waiting_for_key = FALSE;
        return TRUE;
    }
    snprintf(command, sizeof(command), "%s/soundboard/soundboard.sh bind %d %s", home, pending_sound_id, key_string);
    printf("Binding key '%s' to sound ID %d\n", key_string, pending_sound_id);
    printf("Executing: %s\n", command);
    // Execute the command
    int result = system(command);
    if (result == -1) {
        printf("Error: Failed to execute command\n");
    } else if (result != 0) {
        printf("Command failed with exit code: %d\n", WEXITSTATUS(result));
    } else {
        printf("Key binding successful!\n");
    }
    // Call setup to restart xbindkeys with new configuration
    setup_callback(NULL, NULL);
    // Reset the waiting state
    waiting_for_key = FALSE;
    pending_sound_id = 0;

    return TRUE; // Consume the event
}
// Function to play soundboard by left clicking a sound
void play_sound_callback(GtkWidget *widget, gpointer data) {
    int sound_id = GPOINTER_TO_INT(data);
    char command[1024];  // Increased buffer size
    // Get the home directory
    const char *home = getenv("HOME");
    if (!home) {
        printf("Error: HOME environment variable not set\n");
        return;
    }
    // Build the full path to the script
    snprintf(command, sizeof(command), "%s/soundboard/soundboard.sh %d both", home, sound_id);
    printf("Executing: %s\n", command);
    // First check if the script exists and is executable
    char script_path[1024];  // Increased buffer size
    snprintf(script_path, sizeof(script_path), "%s/soundboard/soundboard.sh", home);
    if (access(script_path, F_OK) != 0) {
        printf("Error: Script not found at %s\n", script_path);
        return;
    }
    if (access(script_path, X_OK) != 0) {
        printf("Error: Script exists but is not executable. Run: chmod +x %s\n", script_path);
        return;
    }
    // Execute the command and capture the return value
    int result = system(command);

    if (result == -1) {
        printf("Error: Failed to execute command\n");
    } else if (result != 0) {
        printf("Command failed with exit code: %d\n", WEXITSTATUS(result));
    } else {
        printf("Command executed successfully\n");
    }
}
// Function to parse a single line from the config file
int parse_config_line(const char *line, SoundInfo *sound) {
    // Skip empty lines and comments
    if (line[0] == '#' || line[0] == '\n' || strlen(line) < 3) {
        return 0;
    }
    // Create a working copy of the line
    char *line_copy = strdup(line);
    // Remove newline characters from the end
    char *newline = strchr(line_copy, '\n');
    if (newline) *newline = '\0';
    // Initialize sound structure
    sound->id = 0;
    sound->filename = NULL;
    sound->keybind = NULL;
    sound->description = NULL;
    // Manual parsing to handle empty fields correctly
    char *start = line_copy;
    char *end;
    int field = 0;
    while (field < 4) {
        // Find the next pipe or end of string
        end = strchr(start, '|');

        if (end) {
            *end = '\0';  // Null terminate this field
        }
        switch (field) {
            case 0: // ID
                sound->id = atoi(start);
                if (sound->id == 0 && start[0] != '0') {
                    free(line_copy);
                    return 0;
                }
                break;
            case 1: // filename
                sound->filename = strdup(start);
                break;
            case 2: // keybind (can be empty)
                sound->keybind = strdup(start);
                break;
            case 3: // description
                sound->description = strdup(start);
                break;
        }

        field++;

        if (!end) {
            // No more pipes found
            break;
        }

        start = end + 1;  // Move past the pipe
    }
    free(line_copy);
    int success = (field >= 4);
    if (!success) {
        // Clean up any allocated memory if parsing failed
        free(sound->filename);
        free(sound->keybind);
        free(sound->description);
    }

    return success;
}
// Function to load sounds from config file
int load_sounds_from_config() {
    // Construct the config file path
    const char *home = getenv("HOME");
    if (!home) {
        printf("Error: HOME environment variable not set\n");
        return 0;
    }
    char config_path[1024];  // Increased buffer size
    snprintf(config_path, sizeof(config_path), "%s/soundboard/config.txt", home);

    FILE *file = fopen(config_path, "r");
    if (!file) {
        printf("Error: Could not open config file: %s\n", config_path);
        printf("Make sure to run 'soundboard scan' first to generate the config.\n");
        return 0;
    }
    // First pass: count valid lines
    char line[2048];  // Increased buffer size
    int count = 0;
    while (fgets(line, sizeof(line), file)) {
        SoundInfo temp_sound = {0};
        if (parse_config_line(line, &temp_sound)) {
            count++;
            // Free the temporary strings
            if (temp_sound.filename) free(temp_sound.filename);
            if (temp_sound.keybind) free(temp_sound.keybind);
            if (temp_sound.description) free(temp_sound.description);
        }
    }
    if (count == 0) {
        printf("No valid sounds found in config file.\n");
        fclose(file);
        return 0;
    }
    // Allocate memory for sounds
    app_data.sounds = calloc(count, sizeof(SoundInfo));
    if (!app_data.sounds) {
        printf("Error: Failed to allocate memory for sounds\n");
        fclose(file);
        return 0;
    }
    // Second pass: actually load the sounds
    rewind(file);
    app_data.sound_count = 0;
    while (fgets(line, sizeof(line), file) && app_data.sound_count < count) {
        if (parse_config_line(line, &app_data.sounds[app_data.sound_count])) {
            app_data.sound_count++;
        }
    }
    fclose(file);
    printf("Loaded %d sounds from config file\n", app_data.sound_count);
    return 1;
}
// Function to calculate optimal grid columns based on window width and sound count
int calculate_grid_columns(int window_width, int sound_count) {
    // Account for UI overhead: scrollbar, padding, borders, etc.
    int usable_width = window_width - 60; // Conservative estimate for UI overhead
    // Button dimensions including spacing
    int button_width = 140;  // Our button width
    int button_spacing = 5;  // Grid column spacing
    int total_button_width = button_width + button_spacing;
    // Calculate how many buttons can fit
    int max_cols_by_width = usable_width / total_button_width;
    if (max_cols_by_width < 1) max_cols_by_width = 1;
    // Set reasonable bounds
    int min_cols = 1;
    int max_cols = 16;
    // Clamp to bounds
    if (max_cols_by_width < min_cols) max_cols_by_width = min_cols;
    if (max_cols_by_width > max_cols) max_cols_by_width = max_cols;
    // If we have fewer sounds than calculated columns, use sound count
    if (sound_count > 0 && sound_count < max_cols_by_width) {
        return sound_count;
    }
    printf("Window width: %d, usable: %d, calculated columns: %d\n",
           window_width, usable_width, max_cols_by_width);
    return max_cols_by_width;
}
// Shutdown callback
void shutdown_callback(GtkWidget *widget, gpointer data) {
    printf("Shutting down + cleaning up Soundboard\n");
    const char *home = getenv("HOME");
    if (!home) {
        printf("Error: HOME environment variable not set\n");
        return;
    }
    char command[1024];
    snprintf(command, sizeof(command), "%s/soundboard/soundboard.sh cleanup", home);
    printf("Executing: %s\n", command);
    int result = system(command);
    if (result == -1) {
        printf("Error: Failed to execute shutdown command\n");
    } else if (result != 0) {
        printf("Shutdown command failed with exit code: %d\n", WEXITSTATUS(result));
    } else {
        printf("Shutdown command executed successfully\n");
    }
}

//function for middle click to pass new keybind to bash script
gboolean on_middle_click(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    if (event->type == GDK_BUTTON_PRESS && event->button == 2) {
        int sound_id = GPOINTER_TO_INT(data);
        g_print("Middle-click detected! Press a key to bind to sound %d (Escape to cancel)\n", sound_id);
        // Call shutdown to stop xbindkeys before rebinding
        shutdown_callback(NULL, NULL);
        // Set up waiting state
        waiting_for_key = TRUE;
        pending_sound_id = sound_id;
        // Call setup to restart xbindkeys after binding is complete
        // Ensure the window has focus for key detection
        gtk_widget_grab_focus(app_data.window);
        return TRUE;  // Consume the middle click event
    }
    return FALSE;  // Let other events pass through (including left clicks)
}
//function for right click to unbind a sound
gboolean on_right_click(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    const char *home = getenv("HOME");
    if (!home) {
        printf("Error: HOME environment variable not set\n");
        return FALSE;
    }
    if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
        int sound_id = GPOINTER_TO_INT(data);
        char command[1024];
        snprintf(command, sizeof(command), "%s/soundboard/soundboard.sh unbind %d", home, sound_id);
        printf("Executing: %s\n", command);
        int result = system(command);
        if (result == -1) {
            printf("Error: Failed to execute unbind command\n");
        } else if (result != 0) {
            printf("unbind command failed with exit code: %d\n", WEXITSTATUS(result));
        } else {
            printf("unbind command executed successfully\n");
        }
        return TRUE;
    }
    return FALSE;
}
// Function to create and populate the button grid
void create_button_grid() {
    if (app_data.sound_count == 0) {
        GtkWidget *label = gtk_label_new("No sounds found!\nRun 'soundboard scan' to populate sounds.");
        gtk_container_add(GTK_CONTAINER(app_data.scrolled_window), label);
        return;
    }
    // Create a new grid
    app_data.grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(app_data.grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(app_data.grid), 5);
    gtk_container_set_border_width(GTK_CONTAINER(app_data.grid), 10);
    // Calculate grid dimensions
    int window_width;
    gtk_window_get_size(GTK_WINDOW(app_data.window), &window_width, NULL);
    app_data.grid_columns = calculate_grid_columns(window_width, app_data.sound_count);
    // Create buttons for each sound
    for (int i = 0; i < app_data.sound_count; i++) {
        SoundInfo *sound = &app_data.sounds[i];
        // Simple fallback for non-ASCII descriptions
        char button_label[64];
        const char *desc = (sound->description && strlen(sound->description) > 0)
        ? sound->description : "Sound";
        // Check if description contains non-ASCII characters
        gboolean has_non_ascii = FALSE;
        for (const char *p = desc; *p; p++) {
            if ((unsigned char)*p > 127) {
                has_non_ascii = TRUE;
                break;
            }
        }
        if (has_non_ascii) {
            snprintf(button_label, sizeof(button_label), "Sound #%d", sound->id);
        } else {
            if (strlen(desc) > 28) {
                snprintf(button_label, sizeof(button_label), "%.25s...", desc);
            } else {
                snprintf(button_label, sizeof(button_label), "%s", desc);
            }
        }
        // Create button with truncated description
        GtkWidget *button = gtk_button_new_with_label(button_label);
        // Force exact button size
        gtk_widget_set_size_request(button, 140, 60);
        // Prevent the button from expanding beyond our set size
        // Set text wrapping and alignment for consistency
        GtkWidget *label = gtk_bin_get_child(GTK_BIN(button));
        if (GTK_IS_LABEL(label)) {
            gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
            gtk_label_set_line_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
            gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
            gtk_label_set_max_width_chars(GTK_LABEL(label), 15);
        }
        // Store sound ID in button data
        g_object_set_data(G_OBJECT(button), "sound_id", GINT_TO_POINTER(sound->id));
        // Connect button click signal
        g_signal_connect(button, "clicked", G_CALLBACK(play_sound_callback), GINT_TO_POINTER(sound->id));
        //Middle click detection for passing a new keybind to the bash script to handle
        g_signal_connect(button, "button-press-event", G_CALLBACK(on_middle_click), GINT_TO_POINTER(sound->id));
        //Right click detection for unbinds
        g_signal_connect(button, "button-press-event", G_CALLBACK(on_right_click), GINT_TO_POINTER(sound->id));
        // Calculate grid position
        int row = i / app_data.grid_columns;
        int col = i % app_data.grid_columns;
        // Add button to grid
        gtk_grid_attach(GTK_GRID(app_data.grid), button, col, row, 1, 1);
        // Create tooltip with full description and additional info
        char tooltip[512];  // Increased buffer size
        const char *kb = (sound->keybind && strlen(sound->keybind) > 0)
        ? sound->keybind : "none";
        snprintf(tooltip, sizeof(tooltip), "Sound #%d\n%s\nFile: %s\nKeybind: %s",
                 sound->id,
                 sound->description ? sound->description : "No description",
                 sound->filename ? sound->filename : "Unknown file",
                 kb);
        gtk_widget_set_tooltip_text(button, tooltip);
    }
    // Add grid to scrolled window
    gtk_container_add(GTK_CONTAINER(app_data.scrolled_window), app_data.grid);
}
// Function to refresh the grid (reload sounds and recreate buttons)
void refresh_grid() {
    // Get the current child of the scrolled window
    GtkWidget *child = gtk_bin_get_child(GTK_BIN(app_data.scrolled_window));
    // Remove the existing child if present
    if (child) {
        gtk_container_remove(GTK_CONTAINER(app_data.scrolled_window), child);
        // Don't destroy it manually - removing it will handle that
        app_data.grid = NULL;
    }
    // Clean up old sound data
    cleanup_sounds();
    // Load new sound data
    if (load_sounds_from_config()) {
        create_button_grid();
        gtk_widget_show_all(app_data.window);
    } else {
        // If no sounds found, show error message
        GtkWidget *error_label = gtk_label_new("No sounds found!\n\nMake sure to:\n1. Run 'soundboard scan' to find audio files\n2. Check that ~/soundboard/config.txt exists");
        gtk_container_add(GTK_CONTAINER(app_data.scrolled_window), error_label);
        gtk_widget_show_all(app_data.window);
    }
}
// Scan callback
void scan_callback(GtkWidget *widget, gpointer data) {
    printf("Scanning for sounds in soundboard folder\n");
    const char *home = getenv("HOME");
    if (!home) {
        printf("Error: HOME environment variable not set\n");
        return;
    }
    char command[1024];
    snprintf(command, sizeof(command), "%s/soundboard/soundboard.sh scan", home);
    printf("Executing: %s\n", command);
    int result = system(command);
    if (result == -1) {
        printf("Error: Failed to execute scan command\n");
    } else if (result != 0) {
        printf("Scan command failed with exit code: %d\n", WEXITSTATUS(result));
    } else {
        printf("Scan command executed successfully\n");
        // Automatically refresh the grid after scanning
        refresh_grid();
    }
}
// Callback for refresh button
void refresh_callback(GtkWidget *widget, gpointer data) {
    printf("Refreshing sound list...\n");
    refresh_grid();

}
// Callback for stop button with better error handling
void stop_callback(GtkWidget *widget, gpointer data) {
    printf("Stopping all sounds...\n");
    const char *home = getenv("HOME");
    if (!home) {
        printf("Error: HOME environment variable not set\n");
        return;
    }
    char command[1024];
    snprintf(command, sizeof(command), "%s/soundboard/soundboard.sh stop", home);
    printf("Executing: %s\n", command);
    int result = system(command);
    if (result == -1) {
        printf("Error: Failed to execute stop command\n");
    } else if (result != 0) {
        printf("Stop command failed with exit code: %d\n", WEXITSTATUS(result));
    } else {
        printf("Stop command executed successfully\n");
    }
}
// handle grid during resize
static guint resize_timeout_id = 0;
static gboolean resize_timeout_callback(gpointer user_data) {
    g_print("Refreshing grid after resize timeout\n");
    refresh_grid();
    resize_timeout_id = 0;  // Reset the timeout ID
    return FALSE; // Remove the timeout
}
static gboolean on_configure_event(GtkWidget *widget, GdkEventConfigure *event, gpointer user_data) {
    static gint last_width = 0, last_height = 0;
    // Only refresh if the size actually changed significantly
    if (abs(event->width - last_width) > 10 || abs(event->height - last_height) > 10) {
        g_print("Window configured to: width=%d, height=%d\n", event->width, event->height);
        last_width = event->width;
        last_height = event->height;

        // Cancel any existing timeout
        if (resize_timeout_id != 0) {
            g_source_remove(resize_timeout_id);
        }

        // Set a new timeout to refresh after 150ms of no more resize events
        resize_timeout_id = g_timeout_add(150, resize_timeout_callback, NULL);
    }

    return FALSE; // Continue normal event processing
}
// Custom cleanup function for window destroy
void cleanup_and_quit(GtkWidget *widget, gpointer data) {
    printf("Window closing - running cleanup...\n");

    // Call shutdown to cleanup xbindkeys
    shutdown_callback(NULL, NULL);

    // Then quit GTK
    gtk_main_quit();
}
// Function to create the main GUI window
void create_soundboard_gui() {
    setup_callback(NULL, NULL);
    // Create the main window
    app_data.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(app_data.window), "Soundboard GUI");
    // Set up key event handling
    gtk_widget_set_can_focus(app_data.window, TRUE);
    gtk_widget_set_events(app_data.window, gtk_widget_get_events(app_data.window) | GDK_KEY_PRESS_MASK);
    g_signal_connect(app_data.window, "key-press-event", G_CALLBACK(on_key_press), NULL);
    // Calculate a reasonable initial size based on expected button layout
    int initial_columns = 4; // Target 4 columns initially
    int button_width = 140;
    int button_spacing = 5;
    int ui_overhead = 80; // Padding, scrollbar, etc.
    int initial_width = (initial_columns * (button_width + button_spacing)) + ui_overhead;
    gtk_window_set_default_size(GTK_WINDOW(app_data.window), initial_width, 600);
    gtk_window_set_position(GTK_WINDOW(app_data.window), GTK_WIN_POS_CENTER);
    // Create main vertical box
    GtkWidget *main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(app_data.window), main_vbox);
    // Create header with title and control buttons
    GtkWidget *header_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(header_hbox), 10);
    gtk_box_pack_start(GTK_BOX(main_vbox), header_hbox, FALSE, FALSE, 0);
    // Title label
    GtkWidget *title_label = gtk_label_new("Nico's Soundboard");
    gtk_box_pack_start(GTK_BOX(header_hbox), title_label, TRUE, TRUE, 0);
    gtk_widget_set_halign(title_label, GTK_ALIGN_START);
    // Subtitle label
    // Subtitle label (FIXED VERSION)
    GtkWidget *subtitle_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(subtitle_hbox), 10);
    gtk_box_pack_start(GTK_BOX(main_vbox), subtitle_hbox, FALSE, FALSE, 0);
    GtkWidget *subtitle_label = gtk_label_new("Middle click sound then keypress to rebind sound to key. Right click to unbind a sound.");
    gtk_box_pack_start(GTK_BOX(subtitle_hbox), subtitle_label, TRUE, TRUE, 0);
    gtk_widget_set_halign(subtitle_label, GTK_ALIGN_START);
    // Control buttons - arranged from left to right
    // Scan button (leftmost after title)
    GtkWidget *scan_button = gtk_button_new_with_label("Scan");
    gtk_box_pack_start(GTK_BOX(header_hbox), scan_button, FALSE, FALSE, 0);
    g_signal_connect(scan_button, "clicked", G_CALLBACK(scan_callback), NULL);
    // Stop button (rightmost)
    GtkWidget *stop_button = gtk_button_new_with_label("Stop All");
    gtk_box_pack_end(GTK_BOX(header_hbox), stop_button, FALSE, FALSE, 0);
    g_signal_connect(stop_button, "clicked", G_CALLBACK(stop_callback), NULL);
    // Refresh button
    GtkWidget *refresh_button = gtk_button_new_with_label("Refresh");
    gtk_box_pack_end(GTK_BOX(header_hbox), refresh_button, FALSE, FALSE, 0);
    g_signal_connect(refresh_button, "clicked", G_CALLBACK(refresh_callback), NULL);
    // Shutdown button
    GtkWidget *shutdown_button = gtk_button_new_with_label("Shutdown");
    gtk_box_pack_end(GTK_BOX(header_hbox), shutdown_button, FALSE, FALSE, 0);
    g_signal_connect(shutdown_button, "clicked", G_CALLBACK(shutdown_callback), NULL);
    // Setup button
    GtkWidget *setup_button = gtk_button_new_with_label("Setup");
    gtk_box_pack_end(GTK_BOX(header_hbox), setup_button, FALSE, FALSE, 0);
    g_signal_connect(setup_button, "clicked", G_CALLBACK(setup_callback), NULL);
    // Create scrolled window for the button grid
    app_data.scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(app_data.scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(main_vbox), app_data.scrolled_window, TRUE, TRUE, 0);
    // CRITICAL: Connect window close signal
    g_signal_connect(app_data.window, "destroy", G_CALLBACK(cleanup_and_quit), NULL);
    // Load sounds and create initial grid
    if (load_sounds_from_config()) {
        create_button_grid();
        // handle grid resizing
        g_signal_connect(app_data.window, "configure-event", G_CALLBACK(on_configure_event), NULL);    } else {
            GtkWidget *error_label = gtk_label_new("No sounds found!\n\nMake sure to:\n1. Run 'soundboard scan' to find audio files\n2. Check that ~/soundboard/config.txt exists");
            gtk_container_add(GTK_CONTAINER(app_data.scrolled_window), error_label);
        }
        // Show the window and all its contents
        gtk_widget_show_all(app_data.window);
        gtk_widget_grab_focus(app_data.window);
}
// Cleanup function
void cleanup_app() {
    cleanup_sounds();
}
int main(int argc, char *argv[]) {
    // Initialize GTK
    gtk_init(&argc, &argv);
    // Check dependencies BEFORE doing anything else
    if (!initialize_with_dependency_check()) {
        return 1; // Exit if dependencies missing
    }
    // Register cleanup function
    atexit(cleanup_app);
    // Create the GUI
    create_soundboard_gui();
    // Run the main loop
    gtk_main();
    return 0;
}
