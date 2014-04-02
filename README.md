aquosctl
========

Sharp Aquos television RS-232 control application.

    usage: ./aquosctl [ -h | -n | -p {port} | -v ] {command} [arg]
	    -h	Help
    	-n	Show commands being sent, but don't send them (No-send).
    	-p	Serial Port to use (default is /dev/ttyS0).
    	-v	Verbose mode.

    command    args
    --------------------
    poenable   { on | off }
               Enable/Disable power on command.

    power      { on | off }
               Turn TV on/off.

    input      [ tv | 1 - 7 ]
               Select TV, INPUT1-7; blank to toggle.

    avmode     [standard|movie|game|user|dyn-fixed|dyn|pc|xvycc]
               AV mode selection; blank to toggle.

    vol        { 0 - 60 }
               Set volume (0-60).

    hpos       <varies depending on View Mode or signal type>
               Horizontal Position. Ranges are on the position setting screen.

    vpos       <varies depending on View Mode or signal type>
               Vertical Position. Ranges are on the position setting screen.

    clock      { 0 - 180 }
               Only in PC mode.

    phase      { 1 - 40 }
               Only in PC mode.

    viewmode   {sidebar|sstretch|zoom|stretch|normal|zoom-pc|stretch-pc|dotbydot|full}
               View modes (vary depending on input signal type -- see manual).

    mute       [ on | off ]
               Mute on/off; blank to toggle.

    surround   [ on | off ]
               Surround on/off; blank to toggle.

    audiosel   <none>
               Audio selection toggle.

    sleep      { off or 0 | 30 | 60 | 90 | 120 }
               Sleep timer off or 30/60/90/120 minutes.

    achan      { 1 - 135 }
               Analog channel selection. Over-the-air: 2-69, Cable: 1-135.

    dchan      { xx.yy } or { xx } (xx=channel 1-99, yy=subchannel 1-99)
               Digital over-the-air channel selection.

    dcabl1     { xxx.yyy } or { xxx } (xxx=major ch. 1-999, yyy=minor ch. 0-999)
               Digital cable (type one).

    dcabl2     { 0 - 16383 }
               Digital cable (type two), channels 0-16383.

    chup       <none>
               Channel up. Will switch to TV input if not already selected.

    chdn       <none>
               Channel down. Will switch to TV input if not already selected.

    cc         <none>
               Closed Caption toggle.
