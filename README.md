[![DDraceNetwork](https://ddnet.tw/ddnet-small.png)](https://ddnet.tw) [![](https://github.com/ddnet/ddnet/workflows/Build/badge.svg)](https://github.com/ddnet/ddnet/actions?query=workflow%3ABuild+event%3Apush+branch%3Amaster)

Tater's custom ddnet client with some small modifications

Features
	**Display frozen tees in your team on your HUD** 
		cl_frozen_tees_display
			1 - on
			2 - on + show skin of frozen tees

		cl_max_frozen_display_rows
			(1 - 10) adjust the maximum number of tees displayed

		cl_frozen_tees_text
			1 - on minimal text version of frozen tee HUD

	**KoG cl_prediction_margin PATCH**
		default: 10
		cl_prediction_margin will work on KoG, however your hammer will be delayed more as you increase the margin
		set this between 15 and 25 if you are having lag, you can put it on a bind if it messes too much with your hammer

	**cl_run_on_join as console**
		cl_run_on_join_console
			0 - in chat (normal)
			1 - in console
			allows any console command to be using in cl_run_on_join 