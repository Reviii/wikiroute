const http = require('http');
const child_process = require('child_process');
const fs = require('fs')
const homepage = fs.readFileSync("wikirouteclient.html")

let queue = [];
let queuedIps = new Set();
function wikiroute(inp) {
    return new Promise((resolve, reject) => {
        queue.push({inp, resolve, reject});
        process.stderr.write(queue.length+"\n");
        if (queue.length===1) handleQueue();
    });
}

let child = child_process.spawn("./route", ["nodes.bin", "titles.txt"], {}, (error, stdout, stderr)=>{console.error(stdout,stderr,error);throw new Error("Process ended")})
child.stdout.resume();
child.stdout.setEncoding("utf-8");
console.log(child.stdout.readableHighWaterMark);
var lingeringLine = "";
child.stdout.on('readable', function() {
    let chunk = child.stdout.read(child.stdout.readableLength)
//    console.log("Read "+chunk.length+" bytes\n");
    lines = chunk.split("\n");
    lines[0] = lingeringLine + lines[0];
    lingeringLine = lines.pop();
    lines.forEach(line=>global.processLine(line));
});
child.stdout.on('end', function() {
    global.processLine(lingeringLine);
});
child.stderr.pipe(process.stderr);

function handleQueue() {
    let it = queue[0];
    global.processLine = function(line) {
        global.processLine = function() { throw new Error("Unexpected line") };
        it.resolve(line);
        queue.shift();
//        console.log("Queue left: "+queue.length);
        process.stderr.write(queue.length+"\n");
        if (queue.length>0) {
            setTimeout(handleQueue);
        }
    }
    child.stdin.write(it.inp);
}
const server = http.createServer(async (req, res) => {
    try {
    let ip;
    let [path, search] = req.url.split("?")
    search = (search||"").split("&");
    if (path==="/wikiroute") {
        let source, dest;
        ip = req.socket.localAddress;
//        process.stderr.write(ip+" "+req.url+"\n");
        if (queuedIps.has(ip)) {
            res.writeHead(429, {"Content-Type": "application/json"});
            res.end('{"error":"Too many requests"}');
            return;
        }
        queuedIps.add(ip);
        for (let i=0;i<search.length;i++) {
            let [key, value] = search[i].split("=", 2).map(decodeURIComponent);
            if (key==="source") source = value;
            if (key==="dest") dest = value;
        }
        if (source===undefined||dest===undefined) {
            res.writeHead(400, {"Content-Type": "text/plain"});
            res.end("Invalid query params");
            return;
        }
        source = source.split("\n").join("").split("\0").join("");
        dest = dest.split("\n").join("").split("\0").join("");
        res.writeHead(200, {"Content-Type": "application/json"});
        let date = Date.now();
        let route = await wikiroute("A "+source+"\nB "+dest+"\nR\n");
        process.stderr.write("Queue time: "+(Date.now()-date)+"\n");
        queuedIps.delete(ip); // maybe change to req.socket.localAddress for auto banning
        res.end(route);
    } else if (path==="/") {
        res.writeHead(200, {"Content-Type": "text/html"});
        res.end(homepage);
    } else {
        res.writeHead(404, {"Content-Type": "text/plain"});
        res.end("Not Found");
    }
    } catch(err) {
        res.writeHead(500, {"Content-Type": "text/plain"});
        res.end("Internal Server Error");
        //if (ip) queuedIps.delete(ip);
        console.error(err.stack);
    }
})
server.listen(8080);
