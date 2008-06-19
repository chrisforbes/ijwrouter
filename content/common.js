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

function do_ajax2(url, f, t0, t1)
{
	var g = function() {do_ajax2(url, f, t0, t1);};
	$.ajax({
		type: "GET",
		url: url,
		timeout: t0,
		success: f,
		error: function(xhr, status, e)
		{
			show_dialog( $("#error") );
		},
		complete: function() { setTimeout( g, t1 ); }
	});
}

function do_ajax(url, f)
{
	do_ajax2( url, f, 4000, 2000 );
}

function format_float(x,prec)
{	// todo: round appropriately, rather than the current (sucky) method
	var asString = x.toString();
	
	var k = asString.indexOf(".");
	if (k == 0) { asString = "0" + asString; ++k; }
	if (k >= 0 && k < asString.length - prec)
		asString = asString.slice(0, k + prec);
	
	return asString;
}

function format_amount(x)
{
	var units = [ "MB", "GB", "TB" ];
	var n = 0;
	x >>= 10;	// dont care about KB
	
	while( x > (1 << 20) * 9 / 10 )
		{ x >>= 10; n++; }
		
	return format_float(x / 1024.0, 2) + " " + units[n];
}