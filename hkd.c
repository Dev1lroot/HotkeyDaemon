#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pwd.h>
#include <grp.h>
#include <linux/input.h>
#include <libevdev/libevdev.h>
#include <yaml.h>

typedef struct 
{
    int keys[8];
    int key_count;
    char command[256];
} Macro;

Macro macros[32];
int macro_count = 0;
int key_states[KEY_MAX] = {0};
int debug_mode = 0;

void load_config();
void run_as_user(const char *cmd);

void load_config() 
{
    FILE *fh = fopen("commands.yaml", "r");
    if (!fh) 
    { 
        perror("Config file missing"); 
        exit(1); 
    }

    yaml_parser_t parser;
    yaml_event_t event;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_file(&parser, fh);

    int in_keys = 0;
    while (yaml_parser_parse(&parser, &event)) 
    {
        if (event.type == YAML_SCALAR_EVENT) 
        {
            char *val = (char *)event.data.scalar.value;
            
            /* Check if we are currently parsing a list of scan-codes */
            if (in_keys && strcmp(val, "keys") != 0 && strcmp(val, "command") != 0) 
            {
                int key = atoi(val);
                if (key > 0) 
                {
                    macros[macro_count].keys[macros[macro_count].key_count++] = key;
                }
            } 
            /* Finalize the current macro once the command string is found */
            else if (strcmp(val, "command") == 0) 
            {
                yaml_parser_parse(&parser, &event);
                strncpy(macros[macro_count].command, (char *)event.data.scalar.value, 255);
                macro_count++;
            }
        } 
        else if (event.type == YAML_SEQUENCE_START_EVENT) 
        {
            in_keys = 1;
        } 
        else if (event.type == YAML_SEQUENCE_END_EVENT) 
        {
            in_keys = 0;
        }
        
        if (event.type == YAML_STREAM_END_EVENT) break;
        yaml_event_delete(&event);
    }
    
    printf("Macros loaded: %d\n", macro_count);
    for(int i = 0; i < macro_count; i++) 
    {
        printf("  [%d]: [", i);
        for(int j = 0; j < macros[i].key_count; j++) printf("%d ", macros[i].keys[j]);
        printf("] -> %s\n", macros[i].command);
    }
    fclose(fh);
}

void run_as_user(const char *cmd) 
{
    pid_t pid = fork();
    if (pid == 0) 
    {        
        /* Create a new session to detach from the terminal/parent TTY */
        setsid(); 

        /* Double fork trick to ensure the process cannot re-acquire a TTY */
        pid_t second_pid = fork();
        if (second_pid > 0) 
        {
            /* Exit the first child, making the second child an orphan 
               init (PID 1) will now adopt it */
            exit(0); 
        }

        if (second_pid == 0)
        {
            // THE SECOND AND NOW THE ONLY CHILD ALIVE ='(
            
            const char *username = getenv("SUDO_USER");
            if (!username) username = "dev1lroot";  // TODO: FIXME 
            
            struct passwd *pw = getpwnam(username);
            if (!pw) exit(1);

            // Securely drop privileges to the target user
            if (initgroups(pw->pw_name, pw->pw_gid) != 0) _exit(1);
            if (setgid(pw->pw_gid) != 0) _exit(1);
            if (setuid(pw->pw_uid) != 0) _exit(1);

            // Setup environment ... TODO: exact copy of user's envs
            setenv("USER", pw->pw_name, 1);
            setenv("HOME", pw->pw_dir, 1);
            setenv("DISPLAY", ":0", 1); 
            
            char runtime_path[64];
            snprintf(runtime_path, sizeof(runtime_path), "/run/user/%d", pw->pw_uid);
            setenv("XDG_RUNTIME_DIR", runtime_path, 1);

            char dbus_path[128];
            snprintf(dbus_path, sizeof(dbus_path), "unix:path=/run/user/%d/bus", pw->pw_uid);
            setenv("DBUS_SESSION_BUS_ADDRESS", dbus_path, 1);

            // Redirect standard streams to /dev/null to fully detach from parent terminal
            int fd = open("/dev/null", O_RDWR);
            dup2(fd, STDIN_FILENO);
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            close(fd);

            execl(pw->pw_shell, pw->pw_shell, "-c", cmd, (char *)NULL);
            _exit(0);
        }
    }
    
    // Parent process: wait for the first child to exit (it exits immediately)
    waitpid(pid, NULL, 0); 
}

int main(int argc, char **argv) // TODO: Refactor
{
    for (int i = 1; i < argc; i++) 
    {
        if (strcmp(argv[i], "--debug") == 0) debug_mode = 1;
    }

    load_config();

    // TODO: probe /proc/bus/input/devices instead of hard-coded input device path
    char *path = (argc > 1 && argv[1][0] == '/') ? argv[1] : "/dev/input/event3";
    int fd = open(path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) 
    { 
        perror("Failed to open input device"); 
        return 1; 
    }

    struct libevdev *dev = NULL;
    if (libevdev_new_from_fd(fd, &dev) < 0) 
    { 
        fprintf(stderr, "libevdev initialization failed\n"); 
        return 1; 
    }

    printf("HKD active. Debug: %s\n", debug_mode ? "ON" : "OFF");
    printf("Device: %s\n", libevdev_get_name(dev));

    struct input_event ev;
    while (1) 
    {
        int rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
        
        // Re-sync with the kernel if the internal buffer overflows during heavy input
        if (rc == LIBEVDEV_READ_STATUS_SYNC) 
        {
            while (rc == LIBEVDEV_READ_STATUS_SYNC) rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
        } 
        else if (rc == LIBEVDEV_READ_STATUS_SUCCESS) 
        {
            if (ev.type == EV_KEY) 
            {
                key_states[ev.code] = ev.value;

                if (debug_mode) 
                {
                    printf("[DEBUG] Key: %d | State: %d (%s)\n", 
                           ev.code, ev.value, ev.value == 1 ? "Pressed" : (ev.value == 0 ? "Released" : "Repeat"));
                }

                if (ev.value == 1) 
                { 
                    int best_match = -1;
                    int max_keys = 0;

                    // Search for the most specific macro to prioritize (the one with the longest key sequence)
                    for (int i = 0; i < macro_count; i++) 
                    {
                        int match = 1;
                        for (int j = 0; j < macros[i].key_count; j++) 
                        {
                            if (key_states[macros[i].keys[j]] == 0) 
                            {
                                match = 0;
                                break;
                            }
                        }
                        
                        if (match && macros[i].key_count > max_keys) 
                        {
                            max_keys = macros[i].key_count;
                            best_match = i;
                        }
                    }

                    if (best_match != -1) 
                    {
                        printf(">>> TRIGGERED: %s\n", macros[best_match].command);
                        run_as_user(macros[best_match].command);
                    }
                }
            }
        }
        // Throttle CPU usage during idle state
        usleep(5000);
    }
    return 0;
}