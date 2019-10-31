[![DDraceNetwork](https://ddnet.tw/ddnet-small.png)](https://ddnet.tw) [![CircleCI Build Status](https://circleci.com/gh/ddnet/ddnet/tree/master.png)](https://circleci.com/gh/ddnet/ddnet) [![Travis CI Build Status](https://travis-ci.org/ddnet/ddnet.svg?branch=master)](https://travis-ci.org/ddnet/ddnet) [![AppVeyor Build Status](https://ci.appveyor.com/api/projects/status/foeer8wbynqaqqho?svg=true)](https://ci.appveyor.com/project/def-/ddnet)

Our own flavor of DDRace, a Teeworlds mod. See the [website](https://ddnet.tw) for more information.

Development discussions happen on #ddnet on Quakenet ([Webchat](http://webchat.quakenet.org/?channels=ddnet&uio=d4)) or on [Discord in the developer channel](https://discord.gg/xsEd9xu).

You can get binary releases on the [DDNet website](https://ddnet.tw/downloads/).

For ddnet origin client, please visit https://github.com/ddnet/ddnet

## This is a modified which can convert .demo file to mp4 file


[![Repository status](https://repology.org/badge/vertical-allrepos/ddnet.svg?header=)](https://repology.org/metapackage/ddnet/versions)
=======
- Want to add this function supported, open CMakeLists.txt, and change `option(FFMPEG "Enable video support via ffmpeg" OFF)` 'OFF' to 'ON', or use `-DFFMPEG=ON` when cmake.
- And then you can use command `start_video` and `stop_video` to start and stop conversion from demo to mp4.
- `cl_video_recorder_fps` to change video fps.
- or you can use `Record` button in demo menu, it will render the whole demo (you can stop recording by disconnect).
- `cl_video_showhud`is for whether show ingame HUD when rendering video, default is 0.
- `cl_video_showchat`is for whether show chat when rendering video, default is 1.
- `cl_video_showhookcollother`is for whether show other players' hook collision lines when rendering video, default is 0.
- `cl_video_showdirection`is for whether show other players' key presses when rendering video, default is 0.
- `cl_video_sound_enable` is for whether use sound when rendering video, default is 1.