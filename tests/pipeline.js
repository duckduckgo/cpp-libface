/* Manually testing pipelined HTTP/1.1 requests */
var net = require('net');

function main() {
    var opts = {
	host: 'localhost',
	port: 6767
    };
    var nreq = 4;
    var conn;
    var req_str = "GET /face/suggest/?q=h&n=1 HTTP/1.1\r\n" + 
        "Connection: Keep-Alive\r\n\r\n";
    var req_data = req_str + req_str + req_str + req_str;

    function on_connect(res) {
        console.log("CONNECTED");
        conn.write(req_data);
    }

    conn = net.connect(opts.port, opts.host, on_connect);
    conn.on('data', function(data) {
        // console.log('GOT:', String(data));
        console.log("Got", data.length, "bytes");
        if (nreq < 9 || 1 == 1) {
            conn.write(req_str + req_str);
            nreq += 2;
        }
    });
}

main();
