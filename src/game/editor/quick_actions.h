#ifndef REGISTER_QUICK_ACTION 
#define REGISTER_QUICK_ACTION(name, text, callback, description)
#endif

REGISTER_QUICK_ACTION(AddGroup, "Add group", [&]() { QuickActionAddGroup(); }, "Adds a new group")
REGISTER_QUICK_ACTION(Refocus, "Refocus", [&]() { QuickActionRefocus(); }, "[HOME] Restore map focus")
REGISTER_QUICK_ACTION(Proof, "Proof", [&]() { QuickActionProof(); }, "[ctrl+p] Toggles proof borders. These borders represent what a player maximum can see.")

