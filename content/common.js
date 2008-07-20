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
		error: function() { show_dialog( $("#error") ); },
		complete: function() { setTimeout( g, t1 ); }
	});
}

function do_ajax(url, f)
{
	do_ajax2( url, f, 4000, 2000 );
}

function format_float(x,prec)
{
	var scale = Math.pow( 10, prec );
	x = Math.round( x * scale ) / scale;
	
	var val = Math.floor(x).toString();
	x -= Math.floor(x);
	
	if (prec)
		val += '.';
		while(prec--)
		{
			x *= 10;
			val += Math.floor(x).toString();
			x -= Math.floor(x);
		}
		
	return val;
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

function get_day_suffix(x)
{
	x = parseInt(x);
	switch( x )
	{
	case 1: case 21: case 31: return "st";
	case 2: case 22: return "nd";
	case 3: case 23: return "rd";
	default: return "th";
	}
}