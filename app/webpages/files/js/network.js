
function set_mode()
{
	var mode = $("#sel_mode").val();
	var data = { "mode": mode };

	function okcb(result)
	{
		$('#btn_mode').attr('disabled', null);
	}

	function errcb()
	{
		$('#btn_mode').attr('disabled', null);
	}

	$('#btn_mode').attr('disabled', 'disabled');
	post_cgi("/cgi-bin/network/set_mode.cgi", data, errcb, okcb);
}

function get_mode()
{
	function okcb(result)
	{
		$("#sel_mode").val(result.network.mode);
	}
	get_cgi("/cgi-bin/network/get_mode.cgi", null, okcb);
}

function init_mode()
{
	get_mode();
	$('#btn_mode').click(set_mode);
}

function set_wan()
{
	var data = {};

	wan_proto = $("#sel_wan_proto").val();
	data.proto = wan_proto;

	if (wan_proto == "static")
	{
		data.ip = $("#ipt_ip").val();
		data.netmask = $("#ipt_netmask").val();
		data.gateway = $("#ipt_gateway").val();
	}

	if (wan_proto == "pppoe")
	{
		data.user = $("#ipt_user").val();
		data.password = $("#ipt_password").val();
	}
	
	if (wan_proto != "none")
	{
		data.dns1 = $("#ipt_dns1").val();
		data.dns2 = $("#ipt_dns2").val();
		data.nat = $("#chb_nat").prop("checked") ? "enable" : "disable";

		if (wan_proto == "pppoe")
			data.mtu_pppoe = $("#ipt_mtu_pppoe").val();
		else
			data.mtu = $("#ipt_mtu").val();
	}

	function okcb(result)
	{
		$('#btn_wan').attr('disabled', null);
	}

	function errcb()
	{
		$('#btn_wan').attr('disabled', null);
	}

	$('#btn_wan').attr('disabled', 'disabled');
	post_cgi("/cgi-bin/network/set_wan.cgi", data, errcb, okcb);
}

function change_wan_proto()
{
	wan_proto = $("#sel_wan_proto").val();

	if (wan_proto == "pppoe") {
		$("#ipt_mtu").css('display','none'); 
		$("#ipt_mtu_pppoe").css('display','inline-block'); 
	}
	else {
		$("#ipt_mtu_pppoe").css('display','none'); 
		$("#ipt_mtu").css('display','inline-block'); 
	}

	if (wan_proto == "none") {
		$("#wan_static").css('display','none'); 
		$("#wan_pppoe").css('display','none'); 
		$("#wan_other").css('display','none'); 
	}
	else if (wan_proto == "static") {
		$("#wan_static").css('display','block'); 
		$("#wan_pppoe").css('display','none'); 
		$("#wan_other").css('display','block'); 
	}
	else if (wan_proto == "dhcp") {
		$("#wan_static").css('display','none'); 
		$("#wan_pppoe").css('display','none'); 
		$("#wan_other").css('display','block'); 
	}
	else if (wan_proto == "pppoe") {
		$("#wan_static").css('display','none'); 
		$("#wan_pppoe").css('display','block'); 
		$("#wan_other").css('display','block'); 
	}
}

function get_wan()
{
	function okcb(result)
	{
		$("#sel_wan_proto").val(result.wan.proto);
		$("#ipt_ip").val(result.wan.ip);
		$("#ipt_netmask").val(result.wan.netmask);
		$("#ipt_gateway").val(result.wan.gateway);
		$("#ipt_user").val(result.wan.user);
		$("#ipt_password").val(result.wan.password);
		$("#ipt_dns1").val(result.wan.dns1);
		$("#ipt_dns2").val(result.wan.dns2);
		$("#chb_nat").prop("checked", (result.wan.nat == "enable") ? true : false);
		$("#ipt_mtu").val(result.wan.mtu);
		$("#ipt_mtu_pppoe").val(result.wan.mtu_pppoe);
		change_wan_proto();
	}
	get_cgi("/cgi-bin/network/get_wan.cgi", null, okcb);
}

function init_wan()
{
	get_wan();
	$('#btn_wan').click(set_wan);
	$("#sel_wan_proto").on("change", change_wan_proto);
}

function set_lan()
{
	var data = {
		"ip" : $("#ipt_ip").val(),
		"netmask" : $("#ipt_netmask").val(),
	};

	function okcb(result) {
		$('#btn_lan').attr('disabled', null);
	}

	function errcb() {
		$('#btn_lan').attr('disabled', null);
	}

	$('#btn_lan').attr('disabled', 'disabled');
	post_cgi("/cgi-bin/network/set_lan.cgi", data, errcb, okcb);
}

function get_lan()
{
	function okcb(result)
	{
		$("#ipt_ip").val(result.lan.ip);
		$("#ipt_netmask").val(result.lan.netmask);
	}
	get_cgi("/cgi-bin/network/get_lan.cgi", null, okcb);
}

function init_lan()
{
	get_lan();
	$('#btn_lan').click(set_lan);
}

function set_dhcp()
{
	var data = {};

	data.dhcp_server = $("#chb_dhcp_server").prop("checked") ? "enable" : "disable";

	r1 = $("#ipt_dhcp_range1").val();
	r2 = $("#ipt_dhcp_range2").val();

	if (r1 == "" && r2 == "")
		data.dhcp_range = "";
	else
		data.dhcp_range = r1 + "-" + r2;


	function okcb(result) {
		$('#btn_dhcp').attr('disabled', null);
	}

	function errcb() {
		$('#btn_dhcp').attr('disabled', null);
	}

	$('#btn_dhcp').attr('disabled', 'disabled');
	post_cgi("/cgi-bin/network/set_dhcp.cgi", data, errcb, okcb);
}

function get_dhcp()
{
	function okcb(result)
	{
		$("#chb_dhcp_server").prop("checked", (result.lan.dhcp_server == "enable") ? true : false);
		range = result.lan.dhcp_range.split("-");
		$("#ipt_dhcp_range1").val(range[0]);
		$("#ipt_dhcp_range2").val(range[1]);
	}
	get_cgi("/cgi-bin/network/get_dhcp.cgi", null, okcb);
}

function init_dhcp()
{
	get_dhcp();
	$('#btn_dhcp').click(set_dhcp);
}

function set_wifi(radio)
{
	var data = {
		"radio": radio
	};

	data.enable = $("#chb_enable").prop("checked") ? "enable" : "disable";
	data.ssid = $("#ipt_ssid").val();
	data.encrypt = $("#sel_enc").val();
	data.psk = $("#ipt_psk").val();
	data.bandwidth = $("#sel_bw").val();
	data.channel = $("#sel_ch").val();
	data.txpower = $("#ipt_txpower").val();

	function okcb(result)
	{
		$('#btn_wifi').attr('disabled', null);
	}
	function errcb() {
		$('#btn_wifi').attr('disabled', null);
	}

	$('#btn_wifi').attr('disabled', "disabled");
	post_cgi("/cgi-bin/network/set_wifi.cgi", data, errcb, okcb);
}

function get_wifi(radio)
{
	var data = {
		"radio": radio
	};
	function okcb(result)
	{
		$("#chb_enable").prop("checked", (result[radio].enable == "enable") ? true : false);
		$("#ipt_ssid").val(result[radio].ssid);
		$("#sel_enc").val(result[radio].encrypt);
		$("#ipt_psk").val(result[radio].psk);
		$("#sel_bw").val(result[radio].bandwidth);
		$("#sel_ch").val(result[radio].channel);
		$("#ipt_txpower").val(result[radio].txpower);
	}
	post_cgi("/cgi-bin/network/get_wifi.cgi", data, null, okcb);
}

function init_wifi(radio)
{
	get_wifi(radio);
	$('#btn_wifi').click(function () {
		set_wifi(radio);
	});
}

