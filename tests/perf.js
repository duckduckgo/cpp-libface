var http = require('http');
var pctr = 0;
var ctr = 0;

function print_stats() {
    console.log((ctr-pctr) + "req/sec");
    pctr = ctr;
}

function main() {
    var opts = {
	host: 'localhost',
	port: 6767,
	path: '/face/suggest/?q=h&n=10',
        headers: { "Connection": "Keep-Alive" }
    };
    var req;

    function req_handler(res) {
	++ctr;
	http.get(opts, req_handler);
    }

    var i;

    // If we set MAX to anything > 5 (say 50), that ensures that the
    // already open socket is used by the http agent, and req/sec goes
    // up from 600 to 10000!!
    var MAX = 10;
    for (i = 0; i < MAX; ++i) {
        http.get(opts, req_handler);
    }

    setInterval(print_stats, 1000);
}

main();
