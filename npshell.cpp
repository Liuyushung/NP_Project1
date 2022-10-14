#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include <iostream>
#include <sstream>
#include <vector>
#include <string>

using namespace std;

// #define DEBUG 0
#define DEBUG_CMD   0
#define DEBUG_ARG   1

struct mypipe {
    int in;
    int out;
};

struct my_number_pipe {
    int in;
    int out;
    int number;
};

typedef struct mypipe Pipe;
typedef struct my_number_pipe NumberPipe;

/* Global Variables */
vector<Pipe> pipes;
vector<NumberPipe> number_pipes;

void debug_vector(int type, vector<string> &cmds) {
    for (int i = 0; i < cmds.size(); i++) {
        switch (type)
        {
        case DEBUG_CMD:
            printf("CMD[%d]: %s\n", i, cmds[i].c_str());
            break;
        case DEBUG_ARG:
            printf("ARG[%d]: %s\n", i, cmds[i].c_str());
            break;
        default:
            break;
        }
    }
}

void interrupt_handler(int sig) {
    // Handle SIGINT
    return;
}

void child_handler(int sig) {
    // Handle SIGCHLD
    // Prevent the zombie process
    int stat;

    while(waitpid(-1, &stat, WNOHANG) > 0) {
        // Remove zombie process
    #if 0
        cout << "Child Handler: " << stat << endl;
    #endif
    }

    return;
}

bool is_white_char(string cmd) {
    for (size_t i = 0; i < cmd.length(); i++) {
        if(isspace(cmd[i]) == 0) {
            return false;
        }
    }
    return true;
}

void my_setenv(string var, string value) {
    /*
    Change or add an environment variable.
    If var does not exist in the environment, add var to the environment with the value
    If var already exists in the environment, change the value of var to value.
    */
    setenv(var.c_str(), value.c_str(), 1);
}

void my_printenv(string var) {
    /*
    Print the value of an environment variable.
    If var does not exist in the environment, show nothing
    */
    char *value = getenv(var.c_str());
    
    if (value)
        cout << value << endl;
}

void handle_builtin(string cmd) {
    istringstream iss(cmd);
    string prog;

    getline(iss, prog, ' ');
#ifdef DEBUG
    cout << "handle_builtin: " << "prog name: " << prog << endl;
#endif

    if (prog == "setenv") {
        string var, value;

        getline(iss, var, ' ');
        getline(iss, value, ' ');
#ifdef DEBUG
        cout << "handle_builtin: " << "var: " << var << endl;
        cout << "handle_builtin: " << "value: " << value << endl;
#endif
        my_setenv(var, value);
    } else if (prog == "printenv") {
        string var;
        getline(iss, var, ' ');
#ifdef DEBUG
        cout << "handle_builtin: " << "var: " << var << endl;
#endif
        my_printenv(var);
    } else if (prog == "exit") {
        exit(0);
    }
}

vector<string> parse_pipe(string input) {
    vector<string> cmds;
    string pipe_symbol("| ");
    int pos;

    // Parse pipe
    while ((pos = input.find(pipe_symbol)) != string::npos) {
        cmds.push_back(input.substr(0, pos));
        input.erase(0, pos + pipe_symbol.length());
    }
    cmds.push_back(input);

    // Remove space
    for (size_t i = 0; i < cmds.size(); i++) {
        int head = 0, tail = cmds[i].length() - 1;

        while(cmds[i][head] == ' ') ++head;
        while(cmds[i][tail] == ' ') --tail;

        if (head != 0 || tail != cmds[i].length() - 1) {
            cmds[i] = cmds[i].substr(head, tail + 1);
        }
    }

    return cmds;
}

vector<string> parse_args(string cmd) {
    vector<string> args;

    int next_head, pos;

    while( (pos = cmd.find(" ")) != string::npos) {
        next_head = pos;
        while (cmd[next_head] == ' ') ++next_head;

        args.push_back(cmd.substr(0, pos));
        cmd.erase(0, next_head + 1);
    }
    args.push_back(cmd);

    return args;
}

void execute_command(vector<string> args) {
    const char *prog = args[0].c_str();
    const char **c_args = new const char* [args.size()+1];  // Reserve one location for NULL

    #if 1
        cerr << "Execute: " << prog << endl;
    #endif

    for (size_t i = 0; i < args.size(); i++) {
        c_args[i] = args[i].c_str();
    }

    c_args[args.size()] = NULL;

    if (execvp(prog, (char **)c_args) == -1) {
        // perror("Unkown command: ");
        printf("Unkown command: [%s].\n", prog);
        exit(1);
    }
}

void handle_pipe(vector<string> cmds) {
    vector<string> args;
    string error_pipe_symbol = "!", pipe_symbol = "|";
    string arg;
    bool is_error_pipe = false, is_number_pipe = false;
    bool is_first_cmd = false, is_final_cmd = false;

    for (size_t i = 0; i < cmds.size(); i++) {
        istringstream iss(cmds[i]);
        vector<string> args;
        pid_t pid;
        int pipefd[2];

        if (i == 0) {
            is_first_cmd = true;
        }
        if (i == cmds.size() - 1) {
            // Final Command
            is_final_cmd = true;
        }

        /* Parse Command Start */
        while (getline(iss, arg, ' ')) {

            if (arg.find(error_pipe_symbol) != string::npos) {
                // Handle error pipe
                is_error_pipe = true;
            }
            else if (arg.find(pipe_symbol) != string::npos) {
                // Handle number pipe
                is_number_pipe = true;
            }

            args.push_back(arg); // Add arg to args
        }
        /* Parse Command End */

        /* Create Normal Pipe */
        if (!is_error_pipe && !is_number_pipe) {
            if(!is_final_cmd && cmds.size() > 1) {
                // Create Pipe
                pipe(pipefd);
                pipes.push_back(Pipe{in: pipefd[0], out: pipefd[1]});
            }
        }

        pid = fork();

        if (pid < 0) {
            perror("Fork Error");
            exit(1);
        }
        else if (pid > 0) {
            /* Parent Process */
            #if 0
                cout << "Parent PID: " << getpid() << endl;
            #endif
            /* Close Pipe */
            // Normal Pipe
            if (i != 0) {
                // cout << "Parent Close pipe: " << i-1 << endl;
                close(pipes[i-1].in);
                close(pipes[i-1].out);
            }


            if (is_final_cmd) {
                // Final process, wait
                // cout << "Parent Wait" << endl;
                int st;
                waitpid(pid, &st, 0);
            }
        }
        else {
            /* Child Process */
            #if 0
                cout << "Child PID: " << getpid() << endl;
            #endif

            // Duplicate pipe
            if (pipes.size() > 0) {
                if (is_first_cmd) {
                    dup2(pipes[i].out, STDOUT_FILENO);
                    
                    // close(pipes[i].in);
                    // close(pipes[i].out);
                }
                if (!is_first_cmd && !is_final_cmd) {
                    dup2(pipes[i-1].in, STDIN_FILENO);
                    dup2(pipes[i].out, STDOUT_FILENO);

                    // close(pipes[i-1].in);
                    // close(pipes[i-1].out);
                    // close(pipes[i].in);
                    // close(pipes[i].out);
                }
                if (is_final_cmd) {
                    dup2(pipes[i-1].in, STDIN_FILENO);

                    // close(pipes[i-1].in);
                    // close(pipes[i-1].out);
                }

                // Close pipe
                for (int ci = 0; ci < pipes.size(); ci++) {
                    // cout << getpid() << ": " << ci << endl;
                    close(pipes[ci].in);
                    close(pipes[ci].out);
                }
            }
            // cout << "Child Execute" << endl;
            execute_command(args);
        }
    }
    pipes.clear();
}

void handle_command(string input) {
#ifdef DEBUG
        cout << "handle_command: " << input << endl;
#endif
    vector<string> cmds;

    handle_builtin(input);
    cmds = parse_pipe(input);
    handle_pipe(cmds);

    // debug_vector(DEBUG_CMD, cmds);
}

int main() {
    string input;

    // Clear and Set environment
    // clearenv();
    my_setenv("PATH", "bin:.");
    signal(SIGINT, interrupt_handler);
    signal(SIGCHLD, child_handler);

    while (1) {
        cout << "% ";
        getline(cin, input);

        if (cin.eof()) {
            exit(0);
        }

        if ( input.empty() || is_white_char(input) ) {
            // cout << "X" <<endl;
            continue;
        }


        handle_command(input);
    }
    

    return 0;
}