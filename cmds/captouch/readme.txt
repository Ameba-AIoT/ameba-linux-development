[Description]

This is Realtek Amebad2 captouch command bin, implemented as app module.
This app provides automatic captouch parameter calibration and drawing functions.

[SW setup]
1.Enable the UART node dedicated for Captouch calibration in DTS, e.g. UART2:
&uart2 { //dev/ttyRTK2
	pinctrl-names = "default";
	pinctrl-0 = <&uart2_pins>;
	status = "okay";
};
2.Configure the captouch init parameter in DTS as required:
&captouch {
	pinctrl-names = "default","sleep";
	pinctrl-0 = <&captouch_pins_b>;
	pinctrl-1 = <&captouch_pins_b>;
	rtk,ctc-diffthr = <800>, <800>, <800>, <800>, <1000>, <1000>, <1000>, <1000>, <1000>;
	rtk,ctc-mbias = <0x18>, <0x17>, <16>, <0x1B>, <0x00>, <0x00>, <0x00>, <0x00>, <0x00>;
	rtk,ctc-nnoise = <400>, <400>, <400>, <400>, <1000>, <1000>, <1000>, <1000>, <1000>;
	rtk,ctc-pnoise = <400>, <400>, <400>, <400>, <1000>, <1000>, <1000>, <1000>, <1000>;
	rtk,ctc-ch-status = <1>, <1>, <1>, <1>, <1>, <1>, <1>, <1>, <1>;
	status = "okay";
	wakeup-source;
};

Run:
1.Connect the corresponding serial port to the PC.
2.Execute command:  captouch <UART device node> &
e.g. captouch /dev/ttyRTK2 &
NOTE: <UART device node> is the dedicated UART device node specified in DTS for captouch calibration, not the LOGUART.
3.Open the Cap Test Tool on PC and do the calibration as per applicaiton note.