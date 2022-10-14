#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
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
    int  number;
    bool is_number_pipe;
    bool is_error_pipe;
    bool has_pipe;
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

void debug_number_pipes() {
    for (size_t i = 0; i < number_pipes.size(); i++) {
        cout << "Index: " << i 
             << "\tNumber: " << number_pipes[i].number
             << "\tIn: " << number_pipes[i].in
             << "\tOut: " << number_pipes[i].out << endl;
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

void decrement_and_cleanup_number_pipes() {
    // cout << "Decrement" << endl;

    vector<int> index;
    for (size_t i = 0; i < number_pipes.size(); i++) {
        if( --number_pipes[i].number < 0 ) index.push_back(i);
    }
    for (int i = index.size()-1; i >= 0; --i) {
        number_pipes.erase(number_pipes.begin() + index[i]);
    }
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

bool handle_builtin(string cmd) {
    istringstream iss(cmd);
    string prog;

    getline(iss, prog, ' ');

    if (prog == "setenv") {
        string var, value;

        getline(iss, var, ' ');
        getline(iss, value, ' ');
        my_setenv(var, value);

        return true;
    } else if (prog == "printenv") {
        string var;

        getline(iss, var, ' ');
        my_printenv(var);

        return true;
    } else if (prog == "exit") {
        exit(0);

        return true;
    }
    return false;
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
    regex pattern("[|!][1-9]\\d?\\d?[0]?");
    smatch result;

    while(regex_search(input, result, pattern)) {
        Command command;

        command.cmd = input.substr(0, result.position() + result.length());
        command.number = atoi(input.substr(result.position() + 1, result.length() - 1).c_str());
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

    if (input.length() != 0) {
        Command command{cmd: input, is_number_pipe: false, is_error_pipe: false};
        
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
    #if 0
    for (size_t i = 0; i < lines.size(); i++) {
        cout << "Line " << i << ": " << lines[i].cmd << endl;
        cout << "\tCommand: " << lines[i].cmd << endl;
        cout << "\tCommand Size: " << lines[i].cmds.size() << endl;
        cout << "\tis_number_pipe: " << (lines[i].is_number_pipe ? "True" : "False") << endl;
        cout << "\tis_error_pipe: " << (lines[i].is_error_pipe ? "True" : "False") << endl;
        cout << "\tNumber: " << lines[i].number << endl;
    }
    #endif
        
    return lines;
}

void execute_command(vector<string> args) {
    int fd;
    bool need_file_redirection = false;
    const char *prog = args[0].c_str();
    const char **c_args = new const char* [args.size()+1];  // Reserve one location for NULL

    #if 0
    cout << "Execute Command Args: ";
    for (size_t i = 0; i < args.size(); i++) {
        cout << args[i] << " ";
    }
    cout << endl;
    #endif

    /* Parse Arguments */
    for (size_t i = 0; i < args.size(); i++) {
        if (args[i] == ">") {
            need_file_redirection = true;
            fd = open(args[i+1].c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
            args.pop_back();  // Remove file name
            args.pop_back();  // Remove redirect symbol
            break;
        }
        c_args[i] = args[i].c_str();
    }
    c_args[args.size()] = NULL;

    // Check if need file redirection
    if (need_file_redirection) {
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }

    // Execute Command
    if (execvp(prog, (char **)c_args) == -1) {
        cerr << "Unknown command: [" << args[0] << "]." << endl;
        exit(1);
    }
}

void main_handler(Command &command) {
    /* Pre-Process */
    decrement_and_cleanup_number_pipes();
    if (command.cmds.size() == 1) {
        if (handle_builtin(command.cmds[0]))    return;
    }

    vector<string> args;
    string error_pipe_symbol = "!", pipe_symbol = "|";
    string arg;
    bool is_error_pipe = false, is_number_pipe = false;

    // Handle a command per loop
    for (size_t i = 0; i < command.cmds.size(); i++) {
        istringstream iss(command.cmds[i]);
        vector<string> args;
        pid_t pid;
        int pipefd[2];
        bool is_first_cmd = false, is_final_cmd = false;

        if (i == 0)                        is_first_cmd = true;
        if (i == command.cmds.size() - 1)  is_final_cmd = true;

        /* Parse Command to Args */
        while (getline(iss, arg, ' ')) {
            bool ignore_arg = false;

            if (is_white_char(arg)) continue;

            // cout << "Parse: " << arg << endl;
            if (is_final_cmd) {
                if ((is_number_pipe = (arg.find("|") != string::npos)) ||
                    (is_error_pipe  = (arg.find("!") != string::npos))) {
                    bool is_add = false;
                    ignore_arg = true;

                    for (int x=0; x < number_pipes.size(); ++x) {
                        if (number_pipes[x].number == command.number) {
                            number_pipes.push_back(NumberPipe{
                                in:     number_pipes[x].in,
                                out:    number_pipes[x].out,
                                number: command.number
                            });
                            is_add = true;
                            break;
                        }
                    }
                    if (!is_add) {
                        pipe(pipefd);
                        number_pipes.push_back(NumberPipe{
                            in: pipefd[0],
                            out: pipefd[1],
                            number: command.number});
                    }
                }
            }
            if (!ignore_arg) {
                args.push_back(arg);
            }
        }
        #if 0
        debug_number_pipes();
        #endif

        /* Create Normal Pipe */
        if (!is_error_pipe && !is_number_pipe) {
            if(!is_final_cmd && command.cmds.size() > 1) {
                // Create Pipe
                pipe(pipefd);
                pipes.push_back(Pipe{in: pipefd[0], out: pipefd[1]});
            }
        }

        // cout << "Start Fork" << endl;
        do {
            pid = fork();
            usleep(5000);
        } while (pid < 0);

        if (pid > 0) {
            /* Parent Process */
            #if 0
                cout << "Parent PID: " << getpid() << endl;
                cout << "\tNumber of Pipes: " << pipes.size() << endl;
                cout << "\tNumber of N Pipes: " << number_pipes.size() << endl;
            #endif
            /* Close Pipe */
            // Normal Pipe
            if (i != 0) {
                // cout << "Parent Close pipe: " << i-1 << endl;
                close(pipes[i-1].in);
                close(pipes[i-1].out);
            }

            // Number Pipe
            // cout << "Parent Close number pipe" << endl;
            for (int x=0; x < number_pipes.size(); ++x) {
                if (number_pipes[x].number == 0) {
                    close(number_pipes[x].in);
                    close(number_pipes[x].out);
                }
            }

            if (is_final_cmd) {
                // Final process, wait
                // cout << "Parent Wait" << endl;
                int st;
                waitpid(pid, &st, 0);
            }
        } else {
            /* Child Process */
            #if 0
            usleep(2000);
            cout << "Child PID: " << getpid() << endl;
            cout << "\tFirst? " << (is_first_cmd ? "True" : "False") << endl;
            cout << "\tFinal? " << (is_final_cmd ? "True" : "False") << endl;
            cout << "\tNumber? " << (is_number_pipe ? "True" : "False") << endl;
            cout << "\tError? " << (is_error_pipe ? "True" : "False") << endl;
            #endif

            // Duplicate pipe
            if (is_first_cmd) {
                // Receive input from number pipe
                for (size_t x = 0; x < number_pipes.size(); x++) {
                    if (number_pipes[x].number == 0) {
                        dup2(number_pipes[x].in, STDIN_FILENO);
                        break;
                    }
                }

                if (pipes.size() > 0) {
                    // Normal pipe output
                    dup2(pipes[i].out, STDOUT_FILENO);
                }
            }

            if (!is_first_cmd && !is_final_cmd) {
                dup2(pipes[i-1].in, STDIN_FILENO);
                dup2(pipes[i].out, STDOUT_FILENO);
            }
            if (is_final_cmd) {
                if (is_number_pipe) {
                    // cout << "Last Command Number Pipe" << endl;
                    for (size_t x = 0; x < number_pipes.size(); x++) {
                        if (number_pipes[x].number == command.number) {
                            dup2(number_pipes[x].out, STDOUT_FILENO);
                            // cout << number_pipes[x].out << " to " << STDOUT_FILENO << endl;
                            break;
                        }
                    }
                } else if (is_error_pipe) {
                    for (size_t x = 0; x < number_pipes.size(); x++) {
                        if (number_pipes[x].number == command.number) {
                            dup2(number_pipes[x].out, STDOUT_FILENO);
                            dup2(number_pipes[x].out, STDERR_FILENO);
                            break;
                        }
                    }
                } else {
                    if (pipes.size() > 0) {
                        dup2(pipes[i-1].in, STDIN_FILENO);
                    }
                }
            }

            // Close pipe
            for (int ci = 0; ci < pipes.size(); ci++) {
                close(pipes[ci].in);
                close(pipes[ci].out);
            }
            for (int ci = 0; ci < number_pipes.size(); ci++) {
                close(number_pipes[ci].in);
                close(number_pipes[ci].out);
            }

            execute_command(args);
        }
    }
    pipes.clear();
}

void parse_command(string input) {
    // cout << "parse_command: " << input << endl;

    vector<Command> lines;

    lines = parse_number_pipe(input);

    for (size_t i = 0; i < lines.size(); i++) {
        main_handler(lines[i]);
    }
}

int main() {
    string input;

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
