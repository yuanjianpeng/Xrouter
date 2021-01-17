
var cpu_total_prev = 0;
var cpu_idle_prev = 0;

function load_cpu_stat()
{

	function okcb(result) {
		var total = result.cpu.user + result.cpu.nice 
			+ result.cpu.system + result.cpu.idle + result.cpu.iowait
			+ result.cpu.irq + result.cpu.softirq;
		if (cpu_total_prev != 0) {
			var total_delta = total - cpu_total_prev;
			var idle_delta = result.cpu.idle - cpu_idle_prev;
			var per = (total_delta - idle_delta)/total_delta * 100;
		}
		else {
			var per = (total - result.cpu.idle)/total * 100;
		}
		cpu_total_prev = total;
		cpu_idle_prev = result.cpu.idle;
		$("#cpu").text(per.toFixed(2) + " %");
	}
	get_cgi("/cgi-bin/system/get_cpustat.cgi", null, okcb);
}

function load_uptime()
{
	function okcb(result) {
		$("#utime").text(result.uptime + " seconds");
	}
	get_cgi("/cgi-bin/system/get_uptime.cgi", null, okcb);
}

function load_memory_info()
{
	function okcb(result) {
		$("#mtotal").text(result.total + " kB");
		$("#mfree").text(result.free + " kB");
	}
	get_cgi("/cgi-bin/system/get_meminfo.cgi", null, okcb);
}

function load_phy_status()
{
	function okcb(result) {
		var html = "<table>";
		var lan = 1;
		for (var i = 0; i < result.status.length; i++) {
			if (result.status[i].link) {
				stat = "Up " + result.status[i].speed + "Mbps";
				if (result.status[i].duplex)
					stat += " Full Duplex";
				else
					stat += " Half Duplex";
			}
			else
				stat = "Down";
			if (result.status[i].type == "lan") {
				type = "LAN" + lan;
				lan++;
			}
			else
				type = "WAN";
			html += "<tr>";
			html += "<td>" + type + "</td>";
			html += "<td>" + stat + "</td>";
			html += "</tr>";
		}

		html += "</table>";
		$("#phystatus").html(html);
	}
	get_cgi("/cgi-bin/system/get_phy_status.cgi", null, okcb);
}

function fresh_status ()
{
	load_memory_info();
	load_uptime();
	load_cpu_stat();
	load_phy_status();
}

function init()
{
	fresh_status();
	setInterval(fresh_status, 2000);
}

$(document).ready(init);

