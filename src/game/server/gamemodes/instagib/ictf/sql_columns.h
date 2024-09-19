// This file can be included several times.

#ifndef MACRO_ADD_COLUMN
#error "The column macros must be defined"
#define MACRO_ADD_COLUMN(name, sql_name, sql_type, bind_type, default, merge_method) ;
#endif

MACRO_ADD_COLUMN(Wallshots, "wallshots", "INTEGER", Int, "0", Add)

MACRO_ADD_COLUMN(FlagGrabs, "flag_grabs", "INTEGER", Int, "0", Add)
MACRO_ADD_COLUMN(FlagCaptures, "flag_captures", "INTEGER", Int, "0", Add)
MACRO_ADD_COLUMN(FlaggerKills, "flagger_kills", "INTEGER", Int, "0", Add)
