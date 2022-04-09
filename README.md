[![DDraceNetwork](https://ddnet.tw/ddnet-small.png)](https://ddnet.tw) 

### Taters custom ddnet client with some small modifications
Not guarenteed to be bug free, but I will try to fix them. 

If ddnet devs are reading this and want to steal my changes please feel free.  

# Installation:

Download the exe from https://github.com/sjrc6/ddnet/releases/, or build it using the build instuctions from the main repository. 

# Settings Page: 
![image](https://user-images.githubusercontent.com/22122579/160224795-7fe8e255-f599-4059-889b-b3f35e7cb6ee.png)

# Features:
### **Display frozen tees in your team on your HUD** 

&nbsp;&nbsp;&nbsp;&nbsp;**-tc_frozen_tees_hud**
```
0 - off
1 - on
2 - on + show skin of frozen tees
```
&nbsp;&nbsp;&nbsp;&nbsp;**-tc_frozen_tees_hud_skins**
```
0 - Use ninja for frozen tees
1 - Show darker+transparent version of tee skin when frozen
```
&nbsp;&nbsp;&nbsp;&nbsp;**-tc_frozen_tees_max_rows**
```
(1 - 10) adjust the maximum number of tees displayed
```
&nbsp;&nbsp;&nbsp;&nbsp;**-tc_frozen_tees_only_inteam**
```
0 - always display (team 0) 
1 - only display frozen hud after you join a team
```
&nbsp;&nbsp;&nbsp;&nbsp;**-tc_frozen_tees_text**
```
0 - off
1 - on minimal text version of frozen tee HUD
```

### **Show outlines around tiles** 

&nbsp;&nbsp;&nbsp;&nbsp;**-tc_outline**
```
0 - off (all outline rendering is off)
1 - on (render any enabled outlines)
```

&nbsp;&nbsp;&nbsp;&nbsp;**-tc_outline_freeze**
```
Outlines freeze tiles
```
&nbsp;&nbsp;&nbsp;&nbsp;**-tc_outline_solid**
```
Outlines hook and unhook tiles
```
&nbsp;&nbsp;&nbsp;&nbsp;**-tc_outline_tele**
```
Outlines tele tiles
```
&nbsp;&nbsp;&nbsp;&nbsp;**-tc_outline_unfreeze**
```
Outlines unfreeze and undeep tiles
```
&nbsp;&nbsp;&nbsp;&nbsp;**-tc_outline_width**
```
(1-16) Outline width
```
&nbsp;&nbsp;&nbsp;&nbsp;**-tc_outline_alpha**
```
(0-100) Outline alpha
```
&nbsp;&nbsp;&nbsp;&nbsp;**-tc_outline_alpha_solid**
```
(0-100) Outline alpha for hook and unhook
```
&nbsp;&nbsp;&nbsp;&nbsp;**-tc_outline_color_freeze**
```
Freeze outline color. 
```
&nbsp;&nbsp;&nbsp;&nbsp;**-tc_outline_color_solid**
```
Hook/Unhook outline color. 
```
&nbsp;&nbsp;&nbsp;&nbsp;**-tc_outline_color_tele**
```
Tele outline color. 
```
&nbsp;&nbsp;&nbsp;&nbsp;**-tc_outline_color_unfreeze**
```
Unfreeze/Undeep outline color. 
```

### **KoG cl_prediction_margin PATCH**
```
default: 10
cl_prediction_margin will work on KoG, however your hammer will be delayed more as you increase the margin
set this between 15 and 25 if you are having lag, you can put it on a bind if it messes too much with your hammer
```

### **cl_run_on_join works with console**

&nbsp;&nbsp;&nbsp;&nbsp;**-tc_run_on_join_console**
```
0 - output command to chat (normal)
1 - output command to console
```

### **Display screen center for lineups**
&nbsp;&nbsp;&nbsp;&nbsp;**-tc_show_center**
```
0 - off
1 - on
```

### **Ping Circles Next to nameplates**
&nbsp;&nbsp;&nbsp;&nbsp;**-tc_nameplate_ping_circle**
```
0 - off
1 - on
```

### **KoG Spectator skin fix**
&nbsp;&nbsp;&nbsp;&nbsp;**-tc_kog_spec_skin**
```
0 - off
1 - on
```

### **Enter freeze delay fix (WIP)**
&nbsp;&nbsp;&nbsp;&nbsp;**-tc_freeze_update_fix**
```
0 - normal
1 - on, you will change to ninja skin as soon as you enter freeze
```
### **Render colored all feet with maximum brightness**
&nbsp;&nbsp;&nbsp;&nbsp;**-tc_white_feet**
```
0 - off
1 - on
```
### **Anti Latency Tools for gores**
&nbsp;&nbsp;&nbsp;&nbsp;**-tc_pred_remove**
```
0 - off
1 - on, removes prediction margin while in freeze. 
```
&nbsp;&nbsp;&nbsp;&nbsp;**-tc_pred_negative**
```
(0-40) Negative prediction margin while in freeze, can improve jitter but may cause buggy inputs after being saved. 
```
&nbsp;&nbsp;&nbsp;&nbsp;**-tc_remove_anti**
```
0 - off
1 - on, enable removing prediction and antiping while in freeze. 
```
&nbsp;&nbsp;&nbsp;&nbsp;**-tc_remove_anti_ticks**
```
(0-10) how many ticks of prediction/antiping are removed while in freeze
```
&nbsp;&nbsp;&nbsp;&nbsp;**-tc_remove_anti_delay_ticks**
```
(0-200) how many ticks it takes for all of your configured preidction/antiping to be removed
```
