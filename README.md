# ChillerBot User Xp
<img src="https://github.com/chillerbot/chillerbot-ux/blob/chillerbot/chillerbot_ux.png" height="86" width="86">


Made for main client usage. Based on DDNet which is based on DDrace which is based on Teeworlds.

Features
--------

### Password manager

Go to your teeworlds/chillerbot directory and create the file ``chillpw_secret.txt``


Format is ``hostname:port,dummy,command``
* hostname:port
    - server ip and port
* dummy
    - 0 for main tee 1 for dummy
* command
    - local console command to execute

example:

~/.teeworlds/chillerbot/chillpw_secret.txt
```
# this is a comment
51.210.171.47:7303,0,say /login ChillerDragon password
51.210.171.47:7303,1,say /login ChillerDragon2 password2

# you can also omit the port like this
# it will try to login on all ports if the ip matches 51.210.171.47
# BUT BE CAREFUL!
# this might leak your password in shared hosting scenarios
# or might even send it to public chat if one hoster also has vanilla servers
# only omit the port if you know what you are doing
51.210.171.47,0,say /login ChillerDragon password
51.210.171.47,1,say /login ChillerDragon2 password2
```

### Render pictures

if ``cl_render_pic`` is set to ``1`` it renders images found in the ``playerpics/`` directory above the tees if their ingame name matches the image name.

### Rename on finish

if ``cl_finish_rename`` is set to ``1`` the client will automatically rename the player if a finish tile is near by.
The name is changed to ``cl_finish_name``.


### Spectate switch and tele tiles

In ddrace maps you can go to spectators or do /pause and then execute ``goto_switch i[number]`` to set your view to the first switcher with this number. Execute it again to view the next. Or provide a second argument to view a specific occurence at offset x. For example to find the 2nd switcher with the number 12 do:

    goto_switch 12 2

To iterate over all teleporters with number 3 (in, out, evil, weapons)

    goto_tele 3
    goto_tele 3
    goto_tele 3

### Chat bots

    say_format s[format]

Sends a message in chat supporting format strings.
Use %n to respond to the last ping in chat. Use %g to respond to the last greeting in chat.

    say_hi

Responds to last greeting in chat.

### Afk bots

    afk ?i[minutes]?r[message]

Set client to afk for x minutes (0=infinite). And respond with a custom message to pings while being afk.


    camp

Try to hold the current position by walking left and right when pulled out of the current position.
Intended to avoid getting dragged into the next part when being afk in ddrace.

### Remote control

    cl_remote_control 1
    cl_remote_control_token sample_password

Execute whisper messages in local console when prefixed with the correct token.
Example "/whisper user sample_password say hello". Can be used to mess around with friends or control clients running on other devices.

Cloning
-------

To clone this repository with full history and external libraries (~350 MB):

    git clone --recursive https://github.com/chillerbot/chillerbot-ux

To clone the libraries if you have previously cloned chillerbot without them:

    git submodule update --init --recursive

Dependencies on Linux
---------------------

You can install the required libraries on your system, `touch CMakeLists.txt` and CMake will use the system-wide libraries by default. You can install all required dependencies and CMake on Debian or Ubuntu like this:

    sudo apt install build-essential cmake git libcurl4-openssl-dev libssl-dev libfreetype6-dev libglew-dev libnotify-dev libogg-dev libopus-dev libopusfile-dev libpnglite-dev libsdl2-dev libsqlite3-dev libwavpack-dev python google-mock

Or on Arch Linux like this:

    sudo pacman -S --needed base-devel cmake curl freetype2 git glew libnotify opusfile python sdl2 sqlite wavpack gmock

There is an [AUR package for pnglite](https://aur.archlinux.org/packages/pnglite/). For instructions on installing it, see [AUR packages installation instructions on ArchWiki](https://wiki.archlinux.org/index.php/Arch_User_Repository#Installing_packages).

If you don't want to use the system libraries, you can pass the `-DPREFER_BUNDLED_LIBS=ON` parameter to cmake.

Building on Linux and macOS
---------------------------

To compile DDNet yourself, execute the following commands in the source root:

    mkdir build
    cd build
    cmake ..
    make


Building on windows
-------------------

    mkdir build
    cd build
    cmake ..
    cmake --build .

If you use MinGW as a compiler the client is in build/DDNet.exe if vs in build/Debug/DDNet.exe

## Development

There is a custom merge driver that auto solves some conflicts in CMakeLists.txt
when merging into ddnet. To set it up run this script before merging into ddnet.

Install the lib-teeworlds repo to get the `tw_cmake` tool

    mkdir -p ~/.lib-crash
    (cd ~/.lib-crash && git clone git@github.com:lib-crash/lib-teeworlds.git)

Add it to your PATH by adding this line to your `.bashrc` or `.bash_profile`

    export PATH="$PATH:/home/chiller/.lib-crash/lib-teeworlds/bin"

Then configure the git merge driver

    ./scripts/setup-merge-tools.sh
