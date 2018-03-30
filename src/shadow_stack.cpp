#include "shadow_stack.hpp"
#include "external_stack_server.hpp"
#include "quick_socket.hpp"
#include "constants.hpp"
#include "utilities.hpp"
#include "get_tid.hpp"
#include "group.hpp"

#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sstream>
#include <random>
#include <vector>
#include <string>


// Return a non-existent filename
std::string temp_name() {

	// The desired size file name
	static const int size = 23;

	// Check lenth of string. Note, bind will not accept 
	// file names longer than something like 108 characters
	static_assert( size < 100, "file name too long!" );

	// The desired location
	static char where[] = "/tmp/";
	static const int len_where = strlen(where);

	// Characters which may be used in the file name
	static char lib[] =	"abcdefghijklmnopqrstuvwxyz"
						"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
						"1234567890";
	static const int lib_size = strlen(lib);

	// Until an un-seen filename is generated, loop
	char fname[size + 1];
	std::random_device rd;
	while (true) {

		// Generate a randome filename
		strncpy(fname, where, len_where);
		for ( int i = len_where; i < size; ++i ) {
			fname[i] = lib[rd() % lib_size];
		}
		fname[size] = '\0';

		// We temprarily borrow errno below		
		int old_errno = errno;
		errno = 0;

		// If the file does not exists, restore errno and return it
		if ( access( fname, F_OK ) == -1 ) {
			Utilities::assert( errno == ENOENT, "access failed()." );
			errno = old_errno;
			return fname;
		}
	}
}

// Start's the program passed in via drrun
void start_program( char * drrun, char * a_out, char ** client_argv,
					const char * const mode, char * socket_path ) {

	// Construct args to give to exec
	std::vector<const char *> args;
	
	// Drrun a client
	args.push_back(drrun);
	args.push_back("-c");

	// ShadowStack dynamorio client + args
	args.push_back(DYNAMORIO_CLIENT_SO);
	args.push_back(mode);
	args.push_back(socket_path);

	// Specify target a.out
	args.push_back("--");
	args.push_back(a_out);

	// Add a.out's args
	for(int i = 0; client_argv[i] != NULL; ++i ) {
		args.push_back(client_argv[i]);
	}

	// Null terminate the array
	args.push_back(nullptr);

	// Log the action then flush the buffers
	std::string pnt = get_tid() + ": Starting dr_run\nCalling execvp on: ";
	for ( unsigned long i = 0; i < args.size() - 1; ++i ) {
		pnt += args[i];
		pnt += ' ';
	}
	Utilities::log(pnt.c_str());
	fflush(NULL);

	// Start drrun
	execvp(drrun, (char **) args.data());
	Utilities::err("execvp() failed.");
}

// Called if incorrect usage occured
[[ noreturn ]] static void incorrect_usage() {
	Utilities::message( "Usage: ./" PROGRAM_NAME ".out <Mode> <drrun> <a.out> ...\n"
						"Mode options:\n"
						"\t" INTERNAL_MODE_FLAG " -- internal shadow stack mode\n"
						"\t" EXTERNAL_MODE_FLAG " -- external shadow stack mode\n" );
	Utilities::assert( ! Group::is_setup(), "incorrect_usage() called after group setup");
	Utilities::log_error("Incorrect Usage");
	exit(EXIT_FAILURE);
}

// Main function
int main(int argc, char * argv[]) {

	// Check usage
	if ( argc < 4 ) {
		incorrect_usage();
	}
	const bool is_internal = (strcmp(argv[1], INTERNAL_MODE_FLAG) == 0 );
	const bool is_external = (strcmp(argv[1], EXTERNAL_MODE_FLAG) == 0 );
	if ( ! (is_internal || is_external) ) {
		incorrect_usage();
	}

	// Create a new process group by starting a new session
	// Many terminals will automatically do this, but just in case...
	// Also changes many default signal handlers to kill the process group
	Group::setup();

	// We check for the return statuses of functions, so ignore sigpipe
	Utilities::assert( signal(SIGCHLD, SIG_IGN) != SIG_ERR, "signal() failed." );
	Utilities::assert( signal(SIGPIPE, SIG_IGN) != SIG_ERR, "signal() failed." );
	Utilities::log("Sigpipe and Sigchild ignored");

	// If the shadow stack should be internal, start it
	if (is_internal) {
		char buf[10];
		start_program( argv[2], argv[3], & argv[4], argv[1], buf);
	}

	// Setup a unix server
	// Technically, between generating the name name and the
	// server starting, the file could have been created.
	// However, this is safe as the program will crash if so
	const std::string server_name = temp_name();
	const int sock = QS::create_server(server_name.c_str());

	// Just in case an exception occurs, setup a class
	// whose desctructor will terminate the group
	TerminateOnDestruction tod;

	// Fork
	// Note that the server belongs to the parent, the client is the child
	Utilities::log("Starting initial fork...");
	const pid_t pid = fork();
	Utilities::assert( pid != -1, "fork() failed" );
	
	// If this is the child process,
	// start the program to be protected
	if (pid == 0) {
		Utilities::log("%llu: Forming drrun args...", get_tid());
		start_program( argv[2], argv[3], & argv[4], argv[1],
						(char *) server_name.c_str() );
	}

	// Otherwise, this is the parent process
	else {

		// Wait for the client then start the shadow stack
		Utilities::log("%llu: waiting for client", get_tid());
		const int client_sock = QS::accept_client(sock);

		// Start the shadow stack server
		start_external_shadow_stack(client_sock);

		// If the program made it to this point, nothing
		// went wrong, gracefully exit
		tod.disable();
		exit(EXIT_SUCCESS);
	}
}
