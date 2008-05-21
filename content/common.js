var dialogs;
var current_dialog = null;

function show_dialog( e )
{
	dialogs.not( e ).hide();
	e.show();
	
	var w = window.innerWidth;
	var h = window.innerHeight;
	
	if (w == null) w = document.body.clientWidth;
	if (h == null) h = document.body.clientHeight;
	
	e.css("left", (w - e.width()) / 2);
	e.css("top", (h - e.height()) / 2);
	
	current_dialog = e;
}

function do_ajax(url, f)
{
	var g = function() {do_ajax(url, f);};
	$.ajax({
		type: "GET",
		url: url,
		timeout: 4000,
		success: f,
		error: function(xhr, status, e)
		{
			show_dialog( $("#error") );
		},
		complete: function() { setTimeout( g, 2000 ); }
	});
}