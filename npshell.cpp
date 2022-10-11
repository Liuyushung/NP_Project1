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

void handle_command(string input) {
#ifdef DEBUG
        cout << "handle_command: " << input << endl;
#endif
    handle_builtin(input);
}

int main() {
    string input;

    // Clear and Set environment
    // clearenv();
    my_setenv("PATH", "bin:.");

    while (1) {
        cout << "% ";
        getline(cin, input);

        if (cin.eof()) {
            exit(0);
        }

        if ( input.empty() || is_white_char(input) ) {
            cout << "X" <<endl;
            continue;
        }


        handle_command(input);
    }
    

    return 0;
}