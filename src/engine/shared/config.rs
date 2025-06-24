/// Config variable that is saved when the client is closed.
///
/// Has no effect on other commands.
pub const CFGFLAG_SAVE: i32 = 1 << 0;

/// Command that is available in the client.
pub const CFGFLAG_CLIENT: i32 = 1 << 1;

/// Command that is available on the server.
pub const CFGFLAG_SERVER: i32 = 1 << 2;

/// Command that is delayed in the execution until
/// `IConsole::StoreCommands(false)` is called.
pub const CFGFLAG_STORE: i32 = 1 << 3;

/// Command that is available in the master server.
pub const CFGFLAG_MASTER: i32 = 1 << 4;

/// Command that has something to do with the external console (econ).
pub const CFGFLAG_ECON: i32 = 1 << 5;

/// Command that can be used for testing or cheating maps. Only available if
/// `sv_test_cmds 1` is set.
pub const CMDFLAG_TEST: i32 = 1 << 6;

/// Command that can be used from the chat on the server.
pub const CFGFLAG_CHAT: i32 = 1 << 7;

/// Command that can be used from a map config.
///
/// Only commands that are not security sensitive should have this flag.
pub const CFGFLAG_GAME: i32 = 1 << 8;

/// Command that is not recorded into teehistorian.
///
/// This should only be set for security sensitive commands like passwords etc.
/// that should not be recorded.
pub const CFGFLAG_NONTEEHISTORIC: i32 = 1 << 9;

/// Color config variable that can only have lightness 0.5 to 1.0.
///
/// This is achieved by dividing the lightness channel by and adding 0.5, i.e.
/// remapping all the colors.
///
/// Has no effect on other commands or config variables.
pub const CFGFLAG_COLLIGHT: i32 = 1 << 10;

/// Color config variable that can only have lightness (61/255) to 1.0.
///
/// This is achieved by dividing the lightness channel by and adding (61/255), i.e.
/// remapping all the colors.
///
/// Has no effect on other commands or config variables.
pub const CFGFLAG_COLLIGHT7: i32 = 1 << 11;

/// Color config variable that includes an alpha (opacity) value.
///
/// Has no effect on other commands or config variables.
pub const CFGFLAG_COLALPHA: i32 = 1 << 12;

/// Config variable with insensitive data that can be included in client
/// integrity checks.
///
/// This should only be set on config variables the server could observe anyway.
pub const CFGFLAG_INSENSITIVE: i32 = 1 << 13;
