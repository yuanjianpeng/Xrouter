
function post_cgi(url, data, errcb, okcb)
{
	var ajaxObj = {
		url: url,
		data: data,
		type: "POST",
		dataType: "json",
		error: errcb,
		success: okcb,
	};

	$.ajax(ajaxObj);
}

function get_cgi(url, errcb, okcb)
{
	var ajaxObj = {
		url: url,
		type: "GET",
		dataType: "json",
		error: errcb,
		success: okcb,
	};

	$.ajax(ajaxObj);
}


