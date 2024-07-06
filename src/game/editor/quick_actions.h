#ifndef REGISTER_QUICK_ACTION 
#define REGISTER_QUICK_ACTION(name, text, callback, description)
#endif

REGISTER_QUICK_ACTION(AddGroup, "Add group", [&]() { AddGroup(); }, "Adds a new group")
REGISTER_QUICK_ACTION(Refocus, "Refocus", [&]() { MapView()->Focus(); }, "[HOME] Restore map focus")
REGISTER_QUICK_ACTION(Proof, "Proof", [&]() { MapView()->ProofMode()->Toggle(); }, "[ctrl+p] Toggles proof borders. These borders represent what a player maximum can see.")

