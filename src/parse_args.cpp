#include "parse_args.hpp"
#include "utilities.hpp"
#include "constants.hpp"

#include <string.h>
#include <utility>


// For brevity
using namespace boost::program_options;


/*********************************************************/
/*                                                       */
/*	  				Helper functions					 */
/*                                                       */
/*********************************************************/


// Returns a variables map containing the parsed arguments
// The second argument is a pointer to a vector to store the target arguments in
variables_map parse_args_helper(	const int argc, const char * const argv[], 
									std::vector<std::string> * const target_args ) {

	// Usage options (i.e. how the user wants to use the program)
	// For example, do they want help, version info, to load a config file, or normal use?
	options_description usage_options("Generic options");
	usage_options.add_options() 
		("help,h", "Produce help message")
		("version,v", "produce version information");

	// Configuration options
	options_description config_options("Configuration");
	config_options.add_options()
		(MODE, value<std::string>()->required(),
					"The mode in which the shadow stack is used"
					"\n\t" INTERNAL_MODE_FLAG " -- internal shadow stack mode"
					"\n\t" EXTERNAL_MODE_FLAG " -- external shadow stack mode" )
		(TARGET, value<std::string>()->required(), "The target executable")
		(TARGET_ARGS, value<std::vector<std::string> >(),
					"The target executable's arguments" );

	// Declare the target and its arguments as positional
	positional_options_description target_options;
	target_options
		.add(TARGET, 1)
		.add(TARGET_ARGS, -1);

	// Encapsulate arg parsing
	try {

		// Parse usage options
		variables_map args;
		auto raw_parsed = command_line_parser(argc, argv)
			.options(usage_options)
			.allow_unregistered()
			.run();
		store(raw_parsed, args);

		// If help was asked for
		if ( args.count("help") ) {
			std::stringstream help;
			help << usage_options << '\n' << config_options;
			Utilities::message("\n%s", help.str().c_str());
			exit(EXIT_SUCCESS);
		}

		// If version was asked for
		else if ( args.count("version") ) {
			Utilities::message("version %s", VERSION);
			exit(EXIT_SUCCESS);
		}

		// Parse configuration options
		raw_parsed = command_line_parser(argc, argv)
			.options(config_options)
			.positional(target_options)
			.allow_unregistered()
			.run();
		store( raw_parsed, args );

		// Done parsing, check for errors
		notify(args);

		// Collect all unregistered and positional 
		// arguments to create the target's argument list
		*target_args = collect_unrecognized(raw_parsed.options, include_positional);
		Utilities::assert((*target_args)[0] == args[TARGET].as<std::string>(),
			"Incorrect usage\nFor usage information, use the --help flag");
		target_args->erase(target_args->begin());

		// Return the variable map		
		return std::move(args);
	}

	// If an error occured, note so
	catch (...) {
		Utilities::err("Incorrect usage\nFor usage information, use the --help flag");
		exit(EXIT_FAILURE);
	}
}


/*********************************************************/
/*                                                       */
/*	  					 From header					 */
/*                                                       */
/*********************************************************/


// Args constructor
Args::Args(	const bool is_int, const std::string & targ, 
			std::vector<std::string> & targ_args ) : is_internal(is_int), 
			target(targ), target_args(std::move(targ_args)) {}


// Returns an args_t containing the parsed arguments
Args parse_args(const int argc, const char * const argv[]) {

	// Parse the arguments
	std::vector<std::string> target_args;
	const auto vm = parse_args_helper(argc, argv, & target_args);

	// Extract the arguments and return the result
	return std::move( Args( 
		(strcmp(vm[MODE].as<std::string>().c_str(), INTERNAL_MODE_FLAG) == 0),
		vm[TARGET].as<std::string>(),
		target_args
	));
}