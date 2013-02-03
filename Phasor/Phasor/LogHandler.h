#pragma once

#include "Commands.h"

namespace logging
{
	using namespace commands;
	using namespace halo;
	e_command_result sv_logname(void*, CArgParser& args, CCheckedStream& out);
	e_command_result sv_loglimit(void*, CArgParser& args, CCheckedStream& out);
	e_command_result sv_logmovedir(void*, CArgParser& args, CCheckedStream& out);
}