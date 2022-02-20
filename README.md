[![DDraceNetwork](https://ddnet.tw/ddnet-small.png)](https://ddnet.tw) [![](https://github.com/ddnet/ddnet/workflows/Build/badge.svg)](https://github.com/ddnet/ddnet/actions?query=workflow%3ABuild+event%3Apush+branch%3Amaster)

### Taters custom ddnet client with some small modifications

# Features:
### **Display frozen tees in your team on your HUD** 

&nbsp;&nbsp;&nbsp;&nbsp;**-tc_frozen_tees_display**
```
0 - off
1 - on
2 - on + show skin of frozen tees
```

&nbsp;&nbsp;&nbsp;&nbsp;**-tc_max_frozen_display_rows**
```
(1 - 10) adjust the maximum number of tees displayed
```

&nbsp;&nbsp;&nbsp;&nbsp;**-tc_frozen_tees_text**
```
0 - off
1 - on minimal text version of frozen tee HUD
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


# Installation:

Download the exe from https://github.com/sjrc6/ddnet/releases/, or build it using the build instuctions from the main repository. 
