// This file can be included several times.

#ifndef MACRO_ADD_COLUMN
#error "The column macros must be defined"
#define MACRO_ADD_COLUMN(name, sql_name, sql_type, bind_type, default, merge_method) ;
#endif

MACRO_ADD_COLUMN(Wallshots, "wallshots", "INTEGER", Int, "0", Add)

MACRO_ADD_COLUMN(Freezes, "freezes", "INTEGER", Int, "0", Add)

// normal spikes are kills
MACRO_ADD_COLUMN(GoldSpikes, "gold_spikes", "INTEGER", Int, "0", Add)
MACRO_ADD_COLUMN(GreenSpikes, "green_spikes", "INTEGER", Int, "0", Add)
MACRO_ADD_COLUMN(PurpleSpikes, "purple_spikes", "INTEGER", Int, "0", Add)

MACRO_ADD_COLUMN(BestMulti, "best_multi", "INTEGER", Int, "0", Highest)
MACRO_ADD_COLUMN(aMultis[0], "multi_x2", "INTEGER", Int, "0", Add)
MACRO_ADD_COLUMN(aMultis[1], "multi_x3", "INTEGER", Int, "0", Add)
MACRO_ADD_COLUMN(aMultis[2], "multi_x4", "INTEGER", Int, "0", Add)
MACRO_ADD_COLUMN(aMultis[3], "multi_x5", "INTEGER", Int, "0", Add)
MACRO_ADD_COLUMN(aMultis[4], "multi_x6", "INTEGER", Int, "0", Add)
MACRO_ADD_COLUMN(aMultis[5], "multi_x7", "INTEGER", Int, "0", Add)
MACRO_ADD_COLUMN(aMultis[6], "multi_x8", "INTEGER", Int, "0", Add)
MACRO_ADD_COLUMN(aMultis[7], "multi_x9", "INTEGER", Int, "0", Add)
MACRO_ADD_COLUMN(aMultis[8], "multi_x10", "INTEGER", Int, "0", Add)
MACRO_ADD_COLUMN(aMultis[9], "multi_x11", "INTEGER", Int, "0", Add)
MACRO_ADD_COLUMN(aMultis[10], "multi_x12", "INTEGER", Int, "0", Add)
