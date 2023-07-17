const http = require('http');
const child_process = require('child_process');
const fs = require('fs');
const repl = require('repl');
const homepage = fs.readFileSync("wikirouteclient.html");
let instancesByLang = {};
let errCount = 0;

function createInstance(lang, nodeFile, titleFile, logFile) {
    let me = {queue:[],lingeringLine:""};
    me.child = child_process.spawn("./route", [nodeFile, titleFile], {}, (error, stdout, stderr)=>{console.error(stdout,stderr,error);throw new Error("Process ended")})
    me.child.stdout.resume();
    me.child.stdout.setEncoding("utf-8");
    me.processLine = function(line) { console.error("Unexpected line at instance init:"+JSON.stringify(line)); };
    me.child.stdout.on('data', function(chunk) {
        lines = chunk.split("\n");
        lines[0] = me.lingeringLine + lines[0];
        me.lingeringLine = lines.pop();
        lines.forEach(line=>me.processLine(line));
    });
    me.child.stdout.on('end', restart);
    me.child.stderr.pipe(process.stderr);
    me.logStream = fs.createWriteStream(logFile, {flags:"a"})
    function handleQueue() {
        try {
            let queue = me.queue;
            let it = queue[0];
            if (queue.length===0) return;
            while (queue.length>0&&it.shouldAbort()) {
                it.reject(new Error("aborted"));
                it = queue.shift();
            }
            me.processLine = function(line) {
                me.processLine = function() { handleErr(new Error("Unexpected line")) };
                it.resolve(line);
                queue.shift();
                process.stderr.write(queue.length+"\n");
                if (queue.length>0) {
                    setTimeout(handleQueue);
                }
            }
            me.child.stdin.write(it.inp);
        } catch (err) {
            handleErr(err);
        }
    }
    function handleErr(err) {
        console.error("handleErr:\n"+err.stack);
        me.child.kill()
    }
    function restart() {
        throw new Error("Tried to restart, but blocked in config");
        return;
        console.log("Restarting due to error");
        errCount++;
        setTimeout(()=>errCount--,60*1000);
        if (errCount>5) {
            throw new Error("Error limit reached");
        }
        createInstance(lang, nodeFile, titleFile, logFile);
        if (errCount<3) {
            if (me.queue[0]) {
                me.queue.shift().reject(new Error("Removed due to error"));
            }
            instancesByLang[lang].queue = me.queue;
            instancesByLang[lang].handleQueue();
        } else {
            me.queue.map(a=>a.reject(new Error("Removed due to multiple errors")));
        }
    }
    me.handleQueue = handleQueue;
    instancesByLang[lang] = me;
}
function getData(req) {
    req.setEncoding('utf-8');
    let data ='';
    req.on('data',d=>data+=d);
    return new Promise((resolve,reject)=>{
        req.on('end',()=>resolve(data));
        req.on('error',e=>reject(e));
    });
}
function wikiroute(lang, inp, shouldAbort) {
    return new Promise((resolve, reject) => {
        let instance = instancesByLang[lang];
        instance.logStream.write(inp);
        let queue = instance.queue;
        queue.push({inp, shouldAbort, resolve, reject});
        process.stderr.write(lang+": "+queue.length+"\n");
        if (queue.length===1) instance.handleQueue();
    });
}

createInstance("en", "en.nodes.bin", "en.titles.txt",  "wikiroute.log");
//createInstance("en-text", "en-text.nodes.bin", "en-text.titles.txt",  "wikiroute-text.log");
//createInstance("nl", "nl.nodes.bin", "nl.titles.txt", "nl.wikiroute.log");

const server = http.createServer(async (req, res) => {
    try {
    let [path, search] = req.url.split("?")
    search = (search||"").split("&");
    if (path==="/wikiroute") {
        let sources, dests;
        let lang = "en";
        let ip = req.socket.remoteAddress;
/*        if (ratelimit.ratelimitin(ip)) {
            res.writeHead(429, {"Content-Type": "application/json"});
            res.end('{"error":"Too many requests"}');
            return;
        }*/
        try {
        res.setHeader("Access-Control-Allow-Origin","*");
        res.setHeader("Content-Type","application/json");
        for (let i=0;i<search.length;i++) {
            let [key, value] = search[i].split("=", 2).map(decodeURIComponent);
            if (key==="source") sources = value.split("|");
            if (key==="dest") dests = value.split("|");
            if (key=="lang") lang = value;
        }

        if (req.method==="POST") {
            let data = JSON.parse(await getData(req));
            if (Array.isArray(data.sources)) sources = data.sources.map(a=>a+'');
            if (Array.isArray(data.sources)) dests = data.dests.map(a=>a+'');
        } else if (req.method==="OPTIONS") {
            res.removeHeader("Content-Type");
            res.writeHead(200, {"Access-Control-Allow-Methods":"GET, HEAD, POST, OPTIONS"});
            res.end();
            return;
        } else if (!(req.method==="GET"||req.method==="HEAD")) {
            res.writeHead(405, {"Allow":"GET, HEAD, POST, OPTIONS"})
            res.end('{"error":"Method not allowed"}');
            return;
        }

        if (sources===undefined||dests===undefined) {
            res.writeHead(400);
            res.end('{"error":"Sources or destinations not specified"}');
            return;
        }
        sources = sources.map(a=>a.split("\n").join("").split("\0").join(""));
        dests = dests.map(a=>a.split("\n").join("").split("\0").join(""));
        if (!instancesByLang[lang]) {
            res.end('{"error":"Language not found"}');
            console.log(lang);
            return;
        }
        let date = Date.now();
        let route = await wikiroute(lang, "A "+sources.join("\nA ")+"\nB "+dests.join("\nB ")+"\nR\n", ()=>req.socket.destroyed);
        process.stderr.write("Queue time: "+(Date.now()-date)+"\n");
        res.end(route);
        } catch(err) {
            res.end('{"error":"Internal Server Error"}');
            if (err.message==="aborted") {
                console.log("Error in wikiroute req:",err.stack);
            } else {
                console.log(err.stack);
            }
        }
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
