#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <regex>

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

struct my_command {
    string cmd;           // Cut by number|error pipe
    vector<string> cmds;  // Split by pipe
    bool is_number_pipe;
    bool is_error_pipe;
};

typedef struct mypipe Pipe;
typedef struct my_number_pipe NumberPipe;
typedef struct my_command Command;

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

        my_setenv(var, value);
    } else if (prog == "printenv") {
        string var;
        getline(iss, var, ' ');

        my_printenv(var);
    } else if (prog == "exit") {
        exit(0);
    }
}

vector<string> parse_pipe(string input) {
    /*
     * "a -n | b | c -x |2" -> ["a -n", "b", c -x |2"]
     */
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

vector<Command> parse_number_pipe(string input) {
    /*
     *    "removetag test.html |2 ls | number |1"
     * -> ["removetag test.html |2", "ls | number |1"]
     */
    vector<Command> lines;
    regex pattern("[|!][1-9]\\d?[0]?");
    smatch result;

    while(regex_search(input, result, pattern)) {
        Command command;

        command.cmd = input.substr(0, result.position() + result.length());
        input.erase(0, result.position() + result.length());

        if (command.cmd.find("|") != string::npos) {
            command.is_error_pipe  = false;
            command.is_number_pipe = true;
        } else {
            command.is_error_pipe  = true;
            command.is_number_pipe = false;
        }

        lines.push_back(command);
    }

    if (lines.size() == 0) {
        // Normal Pipe
        Command command{cmd: input, is_number_pipe: false, is_error_pipe: false};

        lines.push_back(command);
    }

    // Split by pipe
    for (size_t i = 0; i < lines.size(); i++) {
        lines[i].cmds = parse_pipe(lines[i].cmd); 
    }

    // Debug
    // for (size_t i = 0; i < lines.size(); i++) {
    //     cout << "Line " << i << ": " << lines[i].cmd << endl;
    // }
    
    
    return lines;
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

    // cout << "EXEC: " << prog << endl;

    for (size_t i = 0; i < args.size(); i++) {
        c_args[i] = args[i].c_str();
    }

    c_args[args.size()] = NULL;

    if (execvp(prog, (char **)c_args) == -1) {
        // perror("Unkown command: ");
        cerr << "Unkown command: [" << args[0] << "]." << endl;
        exit(1);
    }
}

void handle_pipe(Command command) {
    vector<string> args;
    string error_pipe_symbol = "!", pipe_symbol = "|";
    string arg;
    bool is_error_pipe = false, is_number_pipe = false;

    for (size_t i = 0; i < command.cmds.size(); i++) {
        istringstream iss(command.cmds[i]);
        vector<string> args;
        pid_t pid;
        int pipefd[2];
        bool is_first_cmd = false, is_final_cmd = false;

        if (i == 0)                 is_first_cmd = true;
        if (i == command.cmds.size() - 1)   is_final_cmd = true;

        /* Parse Command Start */
        while (getline(iss, arg, ' ')) {
            if (is_white_char(arg)) continue;

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
            if(!is_final_cmd && command.cmds.size() > 1) {
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
            // cout << "Child PID: " << getpid() << endl;

            // Duplicate pipe
            if (pipes.size() > 0) {
                if (is_first_cmd) {
                    dup2(pipes[i].out, STDOUT_FILENO);
                }
                if (!is_first_cmd && !is_final_cmd) {
                    dup2(pipes[i-1].in, STDIN_FILENO);
                    dup2(pipes[i].out, STDOUT_FILENO);
                }
                if (is_final_cmd) {
                    dup2(pipes[i-1].in, STDIN_FILENO);
                }

                // Close pipe
                for (int ci = 0; ci < pipes.size(); ci++) {
                    close(pipes[ci].in);
                    close(pipes[ci].out);
                }
            }
            execute_command(args);
        }
    }
    pipes.clear();
}

void parse_command(string input) {
    // cout << "parse_command: " << input << endl;

    vector<Command> lines;

    handle_builtin(input);

    lines = parse_number_pipe(input);

    for (size_t i = 0; i < lines.size(); i++) {
        handle_pipe(lines[i]);
    }
    
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

        parse_command(input);
    }

    return 0;
}