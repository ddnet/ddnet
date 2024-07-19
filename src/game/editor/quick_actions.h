// This file can be included several times.

#ifndef REGISTER_QUICK_ACTION
#define REGISTER_QUICK_ACTION(name, text, callback, disabled, active, description)
#endif

#define ALWAYS_FALSE []() -> bool { return false; }

REGISTER_QUICK_ACTION(
	AddGroup, "Add group", [&]() { AddGroup(); }, ALWAYS_FALSE, ALWAYS_FALSE, "Adds a new group")
REGISTER_QUICK_ACTION(
	Refocus, "Refocus", [&]() { MapView()->Focus(); }, ALWAYS_FALSE, ALWAYS_FALSE, "[HOME] Restore map focus")
REGISTER_QUICK_ACTION(
	Proof,
	"Proof",
	[&]() { MapView()->ProofMode()->Toggle(); },
	ALWAYS_FALSE,
	[&]() -> bool { return MapView()->ProofMode()->IsEnabled(); },
	"Toggles proof borders. These borders represent what a player maximum can see.")
REGISTER_QUICK_ACTION(
	AddTileLayer, "Add tile layer", [&]() { AddTileLayer(); }, ALWAYS_FALSE, ALWAYS_FALSE, "Creates a new tile layer.")
