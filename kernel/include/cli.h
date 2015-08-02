#ifndef NOX_CLI_H
#define NOX_CLI_H

typedef void (*cmd_handler)(char**,size_t);

void cli_init();
void cli_run();
void cli_cmd_handler_register(char* cmd, cmd_handler handler);

#endif
