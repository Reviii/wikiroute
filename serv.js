const http = require('http');
const child_process = require('child_process');
const fs = require('fs');
const repl = require('repl');
const homepage = fs.readFileSync("wikirouteclient.html");
let etag = '"'+Math.floor(Date.now()/1000)
let etag_br = etag+'-br"'
let etag_gz = etag+'-gz"'
etag += '"'
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
            if (queue.length===0) return;
            let it = queue[0];
            while (queue.length>1&&it.shouldAbort()) { // setting queue length to 0 causes problems, as other parts of the code will think handleQueue is inactive
                it.reject(new Error("aborted"));
                queue.shift();
                it = queue[0] // important
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
function wikiroute(lang, sources, dests, exclude, shouldAbort) {
    sources = sources.map(a=>"A "+a.split("\n").join("").split("\0").join("")+"\n");
    dests = dests.map(a=>"B "+a.split("\n").join("").split("\0").join("")+"\n");
    exclude = exclude.map(a=>"E "+a.split("\n").join("").split("\0").join("")+"\n");
    let inp = sources.join("")+dests.join("\n")+exclude.join("")+"R\n";
    inp=inp.split("\x1b").join("")
    return new Promise((resolve, reject) => {
        let instance = instancesByLang[lang];
        instance.logStream.write(inp);
        let queue = instance.queue;
        queue.push({inp, shouldAbort, resolve, reject});
        process.stderr.write(lang+": "+queue.length+"\n");
        if (queue.length===1) instance.handleQueue();
    });
}
function getData(req,maxLen) {
    req.setEncoding('utf-8');
    let data ='';
    req.on('data',d=>{
        if (data!==null) {
            data+=d;
            if (data.length>maxLen) data=null
        }
    });
    return new Promise((resolve,reject)=>{
        req.on('end',()=>resolve(data));
        req.on('error',e=>reject(e));
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
        let exclude = [];
        let lang = "en";
        let ip = req.socket.remoteAddress;
        try {
        res.setHeader("Access-Control-Allow-Origin","*");
        res.setHeader("Content-Type","application/json");
        for (let i=0;i<search.length;i++) {
            let [key, value] = search[i].split("=", 2).map(decodeURIComponent);
            if (key==="source") sources = value.split("|");
            if (key==="dest") dests = value.split("|");
            if (key==="exclude") exclude = value.split("|");
            if (key==="lang") lang = value;
        }

        if (req.method==="POST") {
            let data = await getData(req,10E6);
            if (data===null) {
                res.writeHead(413)
                res.end('{"error":"Request content too large"}');
                return;
            }
            try {
                data = JSON.parse(data);
            } catch(e) {
                res.writeHead(400)
                res.end(JSON.stringify({error: "Unable to parse JSON: "+e.message}));
                return;
            }
            if (Array.isArray(data.sources)) sources = data.sources.map(a=>a+'');
            if (Array.isArray(data.dests)) dests = data.dests.map(a=>a+'');
            if (Array.isArray(data.exclude)) exclude = data.exclude.map(a=>a+'');
        } else if (req.method==="OPTIONS") {
            res.removeHeader("Content-Type");
            res.writeHead(200, {"Access-Control-Allow-Methods":"GET, HEAD, POST, OPTIONS"});
            res.end();
            return;
        } else if (req.method==="GET"||req.method==="HEAD") {
            if (req.headers["if-none-match"]===etag||req.headers["if-none-match"]===etag_br||req.headers["if-none-match"]===etag_gz) {
                res.setHeader("ETag", etag)
                res.writeHead(304);
                res.end();
                return;
            }
        } else {
            res.writeHead(405, {"Allow":"GET, HEAD, POST, OPTIONS"})
            res.end('{"error":"Method not allowed"}');
            return;
        }

        if (sources===undefined||dests===undefined) {
            res.writeHead(400);
            res.end('{"error":"Sources or destinations not specified"}');
            return;
        }
        if (!Object.hasOwn(instancesByLang,lang)) {
            res.writeHead(404);
            res.end('{"error":"Language not found"}');
            console.log(lang);
            return;
        }
        let date = Date.now();
        let len = instancesByLang[lang].queue.length+1;
        let route = await wikiroute(lang, sources, dests, exclude, ()=>req.socket.destroyed);
        process.stderr.write("Queue time: "+(Date.now()-date)+"/"+len+"\n");
        if (req.method==="GET"||req.method==="HEAD") res.setHeader("ETag",etag);
        res.end(route);
        } catch(err) {
            res.writeHead(500);
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
