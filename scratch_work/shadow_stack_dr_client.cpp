#include "dr_api.h"
#include "drmgr.h"

#include "../constants.hpp"
#include "../utilities.hpp"
#include "../quick_socket.hpp"

#include <sys/socket.h>
#include <string.h>
#include <unistd.h>

#include <iostream>
#include <string>

int sock = -1;
static void ss_setup() {
	std::cout << "Enter unix socket abs path:" << std::endl;
	std::string line;
	std::getline(std::cin, line);
	sock = create_client(line.c_str());
}

int nc = 0;
static void cll(const void * const ret_to_addr) {
ss_log("Ca start");
	printf("------> %p\n", ret_to_addr);
	nc += 1;
	ss_log("%d", nc);
	if ( nc <= 0 ) ss_log("IMP");
	std::string s((char*)ret_to_addr, POINTER_SIZE);
	s = CALL + s;
	ss_assert( write(sock, s.c_str(), s.size()), "write() failed!");
ss_log("Ca end");
}

static void rtt(app_pc instr_addr, app_pc target_addr) {
ss_log("Rt start");
	nc -= 1;
	ss_log("%d", nc);
	if ( nc <= 0 ) ss_log("IMP");
	printf("<------ %p\n", (void *) target_addr);
	std::string s((char*)target_addr, POINTER_SIZE);
	s = RET + s;
	ss_assert( write(sock, s.c_str(), s.size()), "write() failed!");
ss_log("Rt mid");

	const int num_bytes = sizeof(CONTINUE);
	char buffer[num_bytes];

	const int bytes_recv = recv( sock, buffer, num_bytes, MSG_WAITALL );
	ss_assert( bytes_recv == num_bytes, "recv() failed" );
	ss_assert( memcmp(buffer, CONTINUE, num_bytes) == 0, "CONT is wrong!");
ss_log("Rt end");
}


static dr_emit_flags_t
event_app_instruction(void *drcontext, void *tag, instrlist_t *bb, instr_t *instr,
                      bool for_trace, bool translating, void *user_data) {
	// If a call, insert print 'I am a call'
	if ( instr_is_call(instr) ) {
		// include/dr_defines.h:typedef byte * app_pc;
		// tools/DRcontrol.c:typedef unsigned char byte;
		// Thus app_pc = unsigned char *
		const void * const xip = (char *) instr_get_app_pc(instr) 
								 + instr_length(drcontext, instr);
		dr_insert_clean_call(drcontext, bb, instr,
							(void *)cll, false /* save fpstate */, 1,
							OPND_CREATE_INTPTR(xip));
	}

	if ( instr_is_return(instr) ) {
		dr_insert_mbr_instrumentation(drcontext, bb, instr, 	
			(void *) rtt, SPILL_SLOT_1);
	}

    return DR_EMIT_DEFAULT;
}

/// The main client function
/** This function dynamically 'injects' the shadow stack */
DR_EXPORT void dr_client_main(client_id_t id, int argc, const char *argv[]) {	

	printf("Test\n");
	// Setup the client
    dr_set_client_name("ShadowStack DynamoRIO Client 'ShadowStack'", "http://github.com/zwimer/ShadowStack");
    drmgr_init();

    // Register events
    dr_register_exit_event(drmgr_exit);
    drmgr_register_bb_instrumentation_event(NULL, event_app_instruction, NULL);

    // Make it easy to tell, by looking at log file, which client executed
    dr_log(NULL, DR_LOG_ALL, 1, "Client 'ShadowStack' initializing\n");

	// TODO:
	ss_setup();
}
