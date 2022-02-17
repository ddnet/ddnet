[![DDraceNetwork](https://ddnet.tw/ddnet-small.png)](https://ddnet.tw) [![](https://github.com/ddnet/ddnet/workflows/Build/badge.svg)](https://github.com/ddnet/ddnet/actions?query=workflow%3ABuild+event%3Apush+branch%3Amaster)

### Taters custom ddnet client with some small modifications

## Features:
**Display frozen tees in your team on your HUD** 
&nbsp;&nbsp;&nbsp;&nbsp;cl_frozen_tees_display
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;1 - on
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;2 - on + show skin of frozen tees

&nbsp;&nbsp;&nbsp;&nbsp;cl_max_frozen_display_rows
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;(1 - 10) adjust the maximum number of tees displayed

&nbsp;&nbsp;&nbsp;&nbsp;cl_frozen_tees_text
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;1 - on minimal text version of frozen tee HUD

**KoG cl_prediction_margin PATCH**
&nbsp;&nbsp;&nbsp;&nbsp;default: 10
&nbsp;&nbsp;&nbsp;&nbsp;cl_prediction_margin will work on KoG, however your hammer will be delayed more as you increase the margin
&nbsp;&nbsp;&nbsp;&nbsp;set this between 15 and 25 if you are having lag, you can put it on a bind if it messes too much with your hammer

**cl_run_on_join as console**
&nbsp;&nbsp;&nbsp;&nbsp;cl_run_on_join_console
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;0 - in chat (normal)
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;1 - in console
&nbsp;&nbsp;&nbsp;&nbsp;allows any console command to be using in cl_run_on_join 