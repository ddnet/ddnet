// This file can be included several times.

#ifndef MACRO_ADD_COLUMN
#error "The column macros must be defined"
#define MACRO_ADD_COLUMN(name, sql_name, sql_type, bind_type, default, merge_method) ;
#endif

#include <game/server/gamemodes/instagib/solofng/sql_columns.h>

MACRO_ADD_COLUMN(Unfreezes, "unfreezes", "INTEGER", Int, "0", Add)
MACRO_ADD_COLUMN(WrongSpikes, "wrong_spikes", "INTEGER", Int, "0", Add)

// MACRO_ADD_COLUMN(StealsSelf, "StealsSelf", "INTEGER", Int, "0", Add)
