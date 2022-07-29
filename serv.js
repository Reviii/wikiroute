const http = require('http');
const child_process = require('child_process');
const fs = require('fs');
const repl = require('repl');
//const ratelimit = require('./ratelimit.js');
const homepage = fs.readFileSync("wikirouteclient.html");

let queue = [];

let logStream = fs.createWriteStream("wikiroute.log", {flags:"a"})
function wikiroute(inp, shouldAbort) {
    logStream.write(inp)
    return new Promise((resolve, reject) => {
        queue.push({inp, shouldAbort, resolve, reject});
        process.stderr.write(queue.length+"\n");
        if (queue.length===1) handleQueue();
    });
}

let child = child_process.spawn("./route", ["nodes.bin", "titles.txt"], {}, (error, stdout, stderr)=>{console.error(stdout,stderr,error);throw new Error("Process ended")})
child.stdout.resume();
child.stdout.setEncoding("utf-8");
console.log(child.stdout.readableHighWaterMark);
var lingeringLine = "";
child.stdout.on('data', function(chunk) {
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
    while (queue.length>0&&it.shouldAbort()) {
        it.reject("aborted");
        it = queue.shift();
    }
    if (queue.length===0) return;
    global.processLine = function(line) {
        global.processLine = function() { throw new Error("Unexpected line") };
        it.resolve(line);
        queue.shift();
        process.stderr.write(queue.length+"\n");
        if (queue.length>0) {
            setTimeout(handleQueue);
        }
    }
    child.stdin.write(it.inp);
}
const server = http.createServer(async (req, res) => {
    try {
    let [path, search] = req.url.split("?")
    search = (search||"").split("&");
    if (path==="/wikiroute") {
        let source, dest;
        let ip = req.socket.remoteAddress;
/*        if (ratelimit.ratelimitin(ip)) {
            res.writeHead(429, {"Content-Type": "application/json"});
            res.end('{"error":"Too many requests"}');
            return;
        }*/
        try {
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
        let route = await wikiroute("A "+source+"\nB "+dest+"\nR\n", ()=>req.socket.destroyed);
        process.stderr.write("Queue time: "+(Date.now()-date)+"\n");
        res.end(route);
        } catch(err) {
            res.end('{"error":"Internal Server Error"}');
            console.log(err.stack);
        }
//        ratelimit.ratelimitout(ip);
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
        console.error(err.stack);
    }
})
server.listen(8080);
/*let replctx = repl.start('> ').context;
replctx.ratelimit = ratelimit
replctx.queue = queue
replctx.child = child
replctx.server = server*/
