use ddnet_base::ColorRGBA;
use ddnet_base::UserPtr;

pub use self::ffi::CreateConsole;
pub use self::ffi::IConsole;
pub use self::ffi::IConsole_IResult;

/// Command callback for `IConsole`.
///
/// See [`IConsole::Register`] for an example.
#[allow(non_camel_case_types)]
#[repr(transparent)]
pub struct IConsole_FCommandCallback(pub extern "C" fn(result: &IConsole_IResult, user: UserPtr));

unsafe impl cxx::ExternType for IConsole_FCommandCallback {
    type Id = cxx::type_id!("IConsole_FCommandCallback");
    type Kind = cxx::kind::Trivial;
}

#[cxx::bridge]
mod ffi {
    unsafe extern "C++" {
        include!("base/rust.h");
        include!("engine/console.h");
        include!("engine/rust.h");

        type ColorRGBA = ddnet_base::ColorRGBA;
        type ColorHSLA = ddnet_base::ColorHSLA;
        type StrRef<'a> = ddnet_base::StrRef<'a>;
        type UserPtr = ddnet_base::UserPtr;
        type IConsole_FCommandCallback = super::IConsole_FCommandCallback;

        /// Represents the arguments to a console command for [`IConsole`].
        ///
        /// You can only obtain this type in the command callback
        /// [`IConsole_FCommandCallback`] specified in [`IConsole::Register`].
        type IConsole_IResult;

        /// Get the n-th parameter of the command as an integer.
        ///
        /// If the index is out of range, this returns 0. If the parameter
        /// cannot be parsed as an integer, this also returns 0.
        ///
        /// # Examples
        ///
        /// ```
        /// # extern crate ddnet_test;
        /// # use ddnet_base::UserPtr;
        /// # use ddnet_base::s;
        /// # use ddnet_engine::CreateConsole;
        /// # use ddnet_engine::IConsole;
        /// # use ddnet_engine::IConsole_FCommandCallback;
        /// # use ddnet_engine::IConsole_IResult;
        /// # use ddnet_engine_shared::CFGFLAG_SERVER;
        /// #
        /// # let mut console = CreateConsole(CFGFLAG_SERVER);
        /// # let mut executed = false;
        /// # console.pin_mut().Register(s!("command"), s!("sss"), CFGFLAG_SERVER, IConsole_FCommandCallback(callback), UserPtr::from(&mut executed), s!(""));
        /// # console.pin_mut().ExecuteLine(s!(r#"command "1337" abc -7331def"#), -1, true);
        /// # extern "C" fn callback(result_param: &IConsole_IResult, mut user: UserPtr) {
        /// # unsafe { *user.cast_mut::<bool>() = true; }
        /// let result: &IConsole_IResult /* = `command "1337" abc -7331def` */;
        /// # result = result_param;
        /// assert_eq!(result.GetInteger(0), 1337);
        /// assert_eq!(result.GetInteger(1), 0); // unparsable
        /// assert_eq!(result.GetInteger(2), -7331); // parsable start
        /// assert_eq!(result.GetInteger(3), 0); // out of range
        /// # }
        /// # assert!(executed);
        /// ```
        pub fn GetInteger(self: &IConsole_IResult, Index: u32) -> i32;

        /// Get the n-th parameter of the command as a floating point number.
        ///
        /// If the index is out of range, this returns 0.0. If the parameter
        /// cannot be parsed as a floating point number, this also returns 0.0.
        ///
        /// # Examples
        ///
        /// ```
        /// # extern crate ddnet_test;
        /// # use ddnet_base::UserPtr;
        /// # use ddnet_base::s;
        /// # use ddnet_engine::CreateConsole;
        /// # use ddnet_engine::IConsole;
        /// # use ddnet_engine::IConsole_FCommandCallback;
        /// # use ddnet_engine::IConsole_IResult;
        /// # use ddnet_engine_shared::CFGFLAG_SERVER;
        /// #
        /// # let mut console = CreateConsole(CFGFLAG_SERVER);
        /// # let mut executed = false;
        /// # console.pin_mut().Register(s!("command"), s!("sss"), CFGFLAG_SERVER, IConsole_FCommandCallback(callback), UserPtr::from(&mut executed), s!(""));
        /// # console.pin_mut().ExecuteLine(s!(r#"command "13.37" abc -73.31def"#), -1, true);
        /// # extern "C" fn callback(result_param: &IConsole_IResult, mut user: UserPtr) {
        /// # unsafe { *user.cast_mut::<bool>() = true; }
        /// let result: &IConsole_IResult /* = `command "13.37" abc -73.31def` */;
        /// # result = result_param;
        /// assert_eq!(result.GetFloat(0), 13.37);
        /// assert_eq!(result.GetFloat(1), 0.0); // unparsable
        /// assert_eq!(result.GetFloat(2), -73.31); // parsable start
        /// assert_eq!(result.GetFloat(3), 0.0); // out of range
        /// # }
        /// # assert!(executed);
        /// ```
        pub fn GetFloat(self: &IConsole_IResult, Index: u32) -> f32;

        /// Get the n-th parameter of the command as a string.
        ///
        /// If the index is out of range, this returns the empty string `""`.
        ///
        /// # Examples
        ///
        /// ```
        /// # extern crate ddnet_test;
        /// # use ddnet_base::UserPtr;
        /// # use ddnet_base::s;
        /// # use ddnet_engine::CreateConsole;
        /// # use ddnet_engine::IConsole;
        /// # use ddnet_engine::IConsole_FCommandCallback;
        /// # use ddnet_engine::IConsole_IResult;
        /// # use ddnet_engine_shared::CFGFLAG_SERVER;
        /// #
        /// # let mut console = CreateConsole(CFGFLAG_SERVER);
        /// # let mut executed = false;
        /// # console.pin_mut().Register(s!("command"), s!("sss"), CFGFLAG_SERVER, IConsole_FCommandCallback(callback), UserPtr::from(&mut executed), s!(""));
        /// # console.pin_mut().ExecuteLine(s!(r#"command "I'm in space" '' "\"\\Escapes\?\"\n""#), -1, true);
        /// # extern "C" fn callback(result_param: &IConsole_IResult, mut user: UserPtr) {
        /// # unsafe { *user.cast_mut::<bool>() = true; }
        /// let result: &IConsole_IResult /* = `command "I'm in space" '' "\"\\Escapes\?\"\n"` */;
        /// # result = result_param;
        /// assert_eq!(result.GetString(0), "I'm in space");
        /// assert_eq!(result.GetString(1), "''");
        /// assert_eq!(result.GetString(2), r#""\Escapes\?"\n"#); // only \\ and \" escapes
        /// assert_eq!(result.GetString(3), ""); // out of range
        /// # }
        /// # assert!(executed);
        /// ```
        pub fn GetString(self: &IConsole_IResult, Index: u32) -> StrRef<'_>;

        /// Get the n-th parameter of the command as a color.
        ///
        /// If the index is out of range, this returns black. If the parameter
        /// cannot be parsed as a color, this also returns black.
        ///
        /// It supports the following formats:
        /// - `$XXX` (RGB, e.g. `$f00` for red)
        /// - `$XXXX` (RGBA, e.g. `$f00f` for red with maximum opacity)
        /// - `$XXXXXX` (RGB, e.g. `$ffa500` for DDNet's logo color)
        /// - `$XXXXXXXX` (RGBA, e.g. `$ffa500ff` for DDNet's logo color with maximum opacity)
        /// - base 10 integers (24/32 bit HSL in base 10, e.g. `0` for black)
        /// - the following color names: `red`, `yellow`, `green`, `cyan`,
        /// `blue`, `magenta`, `white`, `gray`, `black`.
        ///
        /// The `Light` parameter influences the interpretation of base 10
        /// integers, if it is set, the lightness channel is divided by 2 and
        /// 0.5 is added, making 0.5 the darkest and 1.0 the lightest possible
        /// value.
        ///
        /// # Examples
        ///
        /// ```
        /// # extern crate ddnet_test;
        /// # use ddnet_base::ColorHSLA;
        /// # use ddnet_base::UserPtr;
        /// # use ddnet_base::s;
        /// # use ddnet_engine::CreateConsole;
        /// # use ddnet_engine::IConsole;
        /// # use ddnet_engine::IConsole_FCommandCallback;
        /// # use ddnet_engine::IConsole_IResult;
        /// # use ddnet_engine_shared::CFGFLAG_SERVER;
        /// #
        /// # let mut console = CreateConsole(CFGFLAG_SERVER);
        /// # let mut executed = false;
        /// # console.pin_mut().Register(s!("command"), s!("ssssssss"), CFGFLAG_SERVER, IConsole_FCommandCallback(callback), UserPtr::from(&mut executed), s!(""));
        /// # console.pin_mut().ExecuteLine(s!(r#"command "$f00" $ffa500 $12345 shiny cyan -16777216 $f008 $00ffff80"#), -1, true);
        /// # extern "C" fn callback(result_param: &IConsole_IResult, mut user: UserPtr) {
        /// # unsafe { *user.cast_mut::<bool>() = true; }
        /// let result: &IConsole_IResult /* = `command "$f00" $ffa500 $12345 shiny cyan -16777216 $f008 $00ffff80` */;
        /// # result = result_param;
        /// assert_eq!(result.GetColor(0, 0.0), ColorHSLA { h: 0.0, s: 1.0, l: 0.5, a: 1.0 }); // red
        /// assert_eq!(result.GetColor(1, 0.0), ColorHSLA { h: 0.10784314, s: 1.0, l: 0.5, a: 1.0 }); // DDNet logo color
        /// assert_eq!(result.GetColor(2, 0.0), ColorHSLA { h: 0.0, s: 0.0, l: 0.0, a: 1.0 }); // cannot be parsed => black
        /// assert_eq!(result.GetColor(3, 0.0), ColorHSLA { h: 0.0, s: 0.0, l: 0.0, a: 1.0 }); // unknown color name => black
        /// assert_eq!(result.GetColor(4, 0.0), ColorHSLA { h: 0.5, s: 1.0, l: 0.5, a: 1.0 }); // cyan
        /// assert_eq!(result.GetColor(5, 0.0), ColorHSLA { h: 0.0, s: 0.0, l: 0.0, a: 1.0 }); // black
        /// assert_eq!(result.GetColor(6, 0.0), ColorHSLA { h: 0.0, s: 1.0, l: 0.5, a: 0.53333336 }); // red with alpha specified
        /// assert_eq!(result.GetColor(7, 0.0), ColorHSLA { h: 0.5, s: 1.0, l: 0.5, a: 0.5019608 }); // cyan with alpha specified
        /// assert_eq!(result.GetColor(8, 0.0), ColorHSLA { h: 0.0, s: 0.0, l: 0.0, a: 1.0 }); // out of range => black
        ///
        /// assert_eq!(result.GetColor(0, 0.5), result.GetColor(0, 0.0));
        /// assert_eq!(result.GetColor(1, 0.5), result.GetColor(1, 0.0));
        /// assert_eq!(result.GetColor(2, 0.5), result.GetColor(2, 0.0));
        /// assert_eq!(result.GetColor(3, 0.5), result.GetColor(3, 0.0));
        /// assert_eq!(result.GetColor(4, 0.5), result.GetColor(4, 0.0));
        /// assert_eq!(result.GetColor(5, 0.5), ColorHSLA { h: 0.0, s: 0.0, l: 0.5, a: 1.0 }); // black, but has the `Light` parameter set
        /// assert_eq!(result.GetColor(6, 0.5), result.GetColor(6, 0.0));
        /// assert_eq!(result.GetColor(7, 0.5), result.GetColor(7, 0.0));
        /// assert_eq!(result.GetColor(8, 0.5), result.GetColor(8, 0.0));
        /// # }
        /// # assert!(executed);
        /// ```
        pub fn GetColor(self: &IConsole_IResult, Index: u32, DarkestLighting: f32) -> ColorHSLA;

        /// Get the number of parameters passed.
        ///
        /// This is mostly important for commands that have optional parameters
        /// (`?`) and thus support variable numbers of arguments.
        ///
        /// See [`IConsole::Register`] for more details.
        ///
        /// # Examples
        ///
        /// ```
        /// # extern crate ddnet_test;
        /// # use ddnet_base::ColorHSLA;
        /// # use ddnet_base::UserPtr;
        /// # use ddnet_base::s;
        /// # use ddnet_engine::CreateConsole;
        /// # use ddnet_engine::IConsole;
        /// # use ddnet_engine::IConsole_FCommandCallback;
        /// # use ddnet_engine::IConsole_IResult;
        /// # use ddnet_engine_shared::CFGFLAG_SERVER;
        /// #
        /// # let mut console = CreateConsole(CFGFLAG_SERVER);
        /// # let mut executed: u32 = 0;
        /// # console.pin_mut().Register(s!("command"), s!("s?ss"), CFGFLAG_SERVER, IConsole_FCommandCallback(callback), UserPtr::from(&mut executed), s!(""));
        /// # console.pin_mut().ExecuteLine(s!(r#"command one two three"#), -1, true);
        /// # console.pin_mut().ExecuteLine(s!(r#"command "one two" "three four""#), -1, true);
        /// # extern "C" fn callback(result_param: &IConsole_IResult, mut user: UserPtr) {
        /// # let executed;
        /// # unsafe { executed = *user.cast_mut::<u32>(); *user.cast_mut::<u32>() += 1; }
        /// # if executed == 0 {
        /// let result: &IConsole_IResult /* = `command one two three` */;
        /// # result = result_param;
        /// assert_eq!(result.NumArguments(), 3);
        ///
        /// # } else if executed == 1 {
        /// let result: &IConsole_IResult /* = `command "one two" "three four"` */;
        /// # result = result_param;
        /// assert_eq!(result.NumArguments(), 2);
        /// # }
        /// # }
        /// # assert!(executed == 2);
        /// ```
        pub fn NumArguments(self: &IConsole_IResult) -> i32;

        /// Get the value of the sole victim (`v`) parameter.
        ///
        /// This is mostly important for commands that have optional parameters
        /// and thus support variable numbers of arguments.
        ///
        /// See [`IConsole::Register`] for more details.
        ///
        /// # Examples
        ///
        /// ```
        /// # extern crate ddnet_test;
        /// # use ddnet_base::UserPtr;
        /// # use ddnet_base::s;
        /// # use ddnet_engine::CreateConsole;
        /// # use ddnet_engine::IConsole;
        /// # use ddnet_engine::IConsole_FCommandCallback;
        /// # use ddnet_engine::IConsole_IResult;
        /// # use ddnet_engine_shared::CFGFLAG_SERVER;
        /// #
        /// # let mut console = CreateConsole(CFGFLAG_SERVER);
        /// # let mut executed: u32 = 0;
        /// # console.pin_mut().Register(s!("command"), s!("v"), CFGFLAG_SERVER, IConsole_FCommandCallback(callback), UserPtr::from(&mut executed), s!(""));
        /// # console.pin_mut().ExecuteLine(s!(r#"command me"#), 33, true);
        /// # console.pin_mut().ExecuteLine(s!(r#"command string"#), 33, true);
        /// # console.pin_mut().ExecuteLine(s!(r#"command 42"#), 33, true);
        /// # console.pin_mut().ExecuteLine(s!(r#"command all"#), 33, true);
        /// # extern "C" fn callback(result_param: &IConsole_IResult, mut user: UserPtr) {
        /// # let executed;
        /// # unsafe { executed = *user.cast_mut::<u32>(); *user.cast_mut::<u32>() += 1; }
        /// # match executed {
        /// # 0 => {
        /// let result: &IConsole_IResult /* = `command me` */;
        /// # result = result_param;
        /// // Assume the executing client ID is 33.
        /// assert_eq!(result.GetVictim(), 33);
        ///
        /// # }
        /// # 1 => {
        /// let result: &IConsole_IResult /* = `command string` */;
        /// # result = result_param;
        /// assert_eq!(result.GetVictim(), 0);
        ///
        /// # }
        /// # 2 => {
        /// let result: &IConsole_IResult /* = `command 42` */;
        /// # result = result_param;
        /// assert_eq!(result.GetVictim(), 42);
        ///
        /// # }
        /// # 3 => {
        /// let result: &IConsole_IResult /* = `command all` first invocation */;
        /// # result = result_param;
        /// assert_eq!(result.GetVictim(), 0);
        /// # }
        /// # 4 => {
        /// let result: &IConsole_IResult /* = `command all` second invocation */;
        /// # result = result_param;
        /// assert_eq!(result.GetVictim(), 1);
        /// # }
        /// // …
        /// # 66 => {
        /// let result: &IConsole_IResult /* = `command all` last invocation */;
        /// # result = result_param;
        /// assert_eq!(result.GetVictim(), 63);
        /// # }
        /// # _ => {}
        /// # }
        /// # }
        /// # assert!(executed == 67);
        /// ```
        pub fn GetVictim(self: &IConsole_IResult) -> i32;

        /// Console interface, consists of logging output and command execution.
        ///
        /// This is used for the local console in the client and the remote
        /// console of the server. It handles commands, their inputs and
        /// outputs, command completion and also command and log output.
        ///
        /// Call [`CreateConsole`] to obtain an instance.
        type IConsole;

        /// Execute a command in the console.
        ///
        /// Commands can be separated by semicolons (`;`), everything after a
        /// hash sign (`#`) is treated as a comment and discarded. Parameters
        /// are separated by spaces (` `). By quoting arguments with double
        /// quotes (`"`), the special meaning of the other characters can be
        /// disabled. Double quotes can be escaped as `\"` and backslashes can
        /// be escaped as `\\` inside double quotes.
        ///
        /// When `InterpretSemicolons` is `false`, semicolons are not
        /// interpreted unless the command starts with `mc;`.
        ///
        /// The `ClientID` parameter defaults to -1, `InterpretSemicolons` to
        /// `true` in C++.
        pub fn ExecuteLine(
            self: Pin<&mut IConsole>,
            pStr: StrRef<'_>,
            ClientID: i32,
            InterpretSemicolons: bool,
        );

        /// Log a message.
        ///
        /// `Level` is one of
        /// - [`IConsole_OUTPUT_LEVEL_STANDARD`](constant.IConsole_OUTPUT_LEVEL_STANDARD.html)
        /// - [`IConsole_OUTPUT_LEVEL_ADDINFO`](constant.IConsole_OUTPUT_LEVEL_ADDINFO.html)
        /// - [`IConsole_OUTPUT_LEVEL_DEBUG`](constant.IConsole_OUTPUT_LEVEL_DEBUG.html)
        ///
        /// `pFrom` is some sort of module name, e.g. for code in the console,
        /// it is usually "console". Other examples include "client",
        /// "client/network", …
        ///
        /// `pStr` is the actual log message.
        ///
        /// `PrintColor` is the intended log color.
        ///
        /// `PrintColor` defaults to [`gs_ConsoleDefaultColor`] in C++.
        pub fn Print(
            self: &IConsole,
            Level: i32,
            pFrom: StrRef<'_>,
            pStr: StrRef<'_>,
            PrintColor: ColorRGBA,
        );

        /// Register a command.
        ///
        /// This function needs a command name, a parameter shortcode, some
        /// flags that specify metadata about the command, a callback function
        /// with associated `UserPtr` and a help string.
        ///
        /// `Flags` must be a combination of
        /// - [`CFGFLAG_SAVE`](../ddnet_engine_shared/constant.CFGFLAG_SAVE.html)
        /// - [`CFGFLAG_CLIENT`](../ddnet_engine_shared/constant.CFGFLAG_CLIENT.html)
        /// - [`CFGFLAG_SERVER`](../ddnet_engine_shared/constant.CFGFLAG_SERVER.html)
        /// - [`CFGFLAG_STORE`](../ddnet_engine_shared/constant.CFGFLAG_STORE.html)
        /// - [`CFGFLAG_MASTER`](../ddnet_engine_shared/constant.CFGFLAG_MASTER.html)
        /// - [`CFGFLAG_ECON`](../ddnet_engine_shared/constant.CFGFLAG_ECON.html)
        /// - [`CMDFLAG_TEST`](../ddnet_engine_shared/constant.CMDFLAG_TEST.html)
        /// - [`CFGFLAG_CHAT`](../ddnet_engine_shared/constant.CFGFLAG_CHAT.html)
        /// - [`CFGFLAG_GAME`](../ddnet_engine_shared/constant.CFGFLAG_GAME.html)
        /// - [`CFGFLAG_NONTEEHISTORIC`](../ddnet_engine_shared/constant.CFGFLAG_NONTEEHISTORIC.html)
        /// - [`CFGFLAG_COLLIGHT`](../ddnet_engine_shared/constant.CFGFLAG_COLLIGHT.html)
        /// - [`CFGFLAG_INSENSITIVE`](../ddnet_engine_shared/constant.CFGFLAG_INSENSITIVE.html)
        ///
        /// # Parameter shortcode
        ///
        /// The following parameter types are supported:
        ///
        /// - `i`, `f`, `s`: **I**nteger, **f**loat, and **s**tring parameter,
        ///   respectively.  Since they're not type-checked, they're all the
        ///   same, one word delimited by whitespace or any quoted string.
        ///   Examples: `12`, `example`, `"Hello, World!"`.
        /// - `r`: **R**est of the command line, possibly multiple word, until
        ///   the next command or the end of line. Alternatively one quoted
        ///   parameter. Examples: `multiple words even without quotes`,
        ///   `"quotes are fine, too"`.
        /// - `v`: **V**ictim client ID for this command. Supports the special
        ///   parameters `me` for the executing client ID, and `all` to target
        ///   all players. Examples: `0`, `63`, `words` (gets parsed as `0`),
        ///   `me`, `all`.
        ///
        /// The parameter shortcode is now a string consisting of any number of
        /// these parameter type characters, a question mark `?` to mark the
        /// beginning of optional parameters and `[<string>]` help strings after
        /// each parameter that can have a short explanation.
        ///
        /// Examples:
        ///
        /// - `echo` has `r[text]`: Takes the whole command line as the first
        ///   parameter. This means we get the following arguments:
        ///   ```text
        ///   ## "1"
        ///   echo 1
        ///
        ///   ## "multiple words"
        ///   echo multiple words
        ///
        ///   ## error: missing argument
        ///   echo
        ///
        ///   ## "multiple"
        ///   echo "multiple" "quoted" "arguments"
        ///
        ///   ## "comments"
        ///   echo comments # work too
        ///
        ///   ## "multiple"; "commands"
        ///   echo multiple; echo commands
        ///   ```
        /// - `muteid` has `v[id] i[seconds] ?r[reason]`. Assume the local
        ///   player has client ID 33, then we get the following
        ///   arguments:
        ///   ```text
        ///   ## "33" "60"
        ///   muteid me 60
        ///
        ///   ## "12" "120" "You're a wonderful person"
        ///   muteid 12 120 You're a wonderful person
        ///
        ///   ## "0" "180"; "1" "180"; …, "63" "180"
        ///   muteid all 180
        ///   ```
        ///
        /// # Examples
        ///
        /// ```
        /// # extern crate ddnet_test;
        /// use ddnet_base::UserPtr;
        /// use ddnet_base::s;
        /// use ddnet_engine::CreateConsole;
        /// use ddnet_engine::IConsole;
        /// use ddnet_engine::IConsole_FCommandCallback;
        /// use ddnet_engine::IConsole_IResult;
        /// use ddnet_engine::IConsole_OUTPUT_LEVEL_STANDARD;
        /// use ddnet_engine::gs_ConsoleDefaultColor;
        /// use ddnet_engine_shared::CFGFLAG_SERVER;
        ///
        /// extern "C" fn print_callback(_: &IConsole_IResult, user: UserPtr) {
        ///     let console: &IConsole = unsafe { user.cast() };
        ///     console.Print(IConsole_OUTPUT_LEVEL_STANDARD, s!("example"), s!("print callback"), gs_ConsoleDefaultColor);
        /// }
        ///
        /// let mut console = CreateConsole(CFGFLAG_SERVER);
        /// let user = (&*console).into();
        /// console.pin_mut().Register(s!("example"), s!(""), CFGFLAG_SERVER, IConsole_FCommandCallback(print_callback), user, s!("Example command outputting a single line into the console"));
        /// console.pin_mut().ExecuteLine(s!("example"), -1, true);
        /// ```
        pub fn Register(
            self: Pin<&mut IConsole>,
            pName: StrRef<'_>,
            pParams: StrRef<'_>,
            Flags: i32,
            pfnFunc: IConsole_FCommandCallback,
            pUser: UserPtr,
            pHelp: StrRef<'_>,
        );

        /// Create a console instance.
        ///
        /// It comes with a few registered commands by default. Only the
        /// commands sharing a bit with the `FlagMask` parameter are considered
        /// when executing commands.
        ///
        /// # Examples
        ///
        /// ```
        /// # extern crate ddnet_test;
        /// use ddnet_base::s;
        /// use ddnet_engine::CreateConsole;
        /// use ddnet_engine_shared::CFGFLAG_SERVER;
        ///
        /// let mut console = CreateConsole(CFGFLAG_SERVER);
        /// console.pin_mut().ExecuteLine(s!("cmdlist; echo hi!"), -1, true);
        /// ```
        pub fn CreateConsole(FlagMask: i32) -> UniquePtr<IConsole>;
    }
}

/// Default console output color. White.
///
/// Corresponds to the `gs_ConsoleDefaultColor` constant of the same name in
/// C++.
///
/// Used as a last parameter in [`IConsole::Print`].
///
/// It is treated as "no color" in the console code, meaning that the default
/// color of the output medium isn't overridden.
#[allow(non_upper_case_globals)]
pub const gs_ConsoleDefaultColor: ColorRGBA = ColorRGBA {
    r: 1.0,
    g: 1.0,
    b: 1.0,
    a: 1.0,
};

/// Default console output level.
///
/// Corresponds to the `IConsole::OUTPUT_LEVEL_STANDARD` enum value in C++.
///
/// Used as a first parameter in [`IConsole::Print`].
///
/// This is the only level that is shown by default.
#[allow(non_upper_case_globals)]
pub const IConsole_OUTPUT_LEVEL_STANDARD: i32 = 0;
/// First more verbose console output level.
///
/// Corresponds to the `IConsole::OUTPUT_LEVEL_ADDINFO` enum value in C++.
///
/// Used as a first parameter in [`IConsole::Print`].
///
/// This output level is not shown by default.
#[allow(non_upper_case_globals)]
pub const IConsole_OUTPUT_LEVEL_ADDINFO: i32 = 1;
/// Most verbose console output level.
///
/// Corresponds to the `IConsole::OUTPUT_LEVEL_DEBUG` enum value in C++.
///
/// Used as a first parameter in [`IConsole::Print`].
///
/// This output level is not shown by default.
#[allow(non_upper_case_globals)]
pub const IConsole_OUTPUT_LEVEL_DEBUG: i32 = 2;
