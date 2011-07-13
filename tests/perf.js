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
	path: '/face/suggest/?q=h&n=10'
    };
    var req;

    function req_handler(res) {
	++ctr;
	req = http.get(opts, req_handler);
    }

    req = http.get(opts, req_handler);

    setInterval(print_stats, 1000);
}

main();
