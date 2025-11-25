class Stdio {
    constructor(onStdout, onStderr) {
        this.stdinBuffer = []; // Comes from JS layer
        this.stdinBufferIdx = 0; // Position in the stdin buffer
        this.stdoutBuffer = ""; // Comes from native layer
        this.stderrBuffer = ""; // Comes from native layer

        // Callbacks
        this.onStdout = onStdout;
        this.onStderr = onStderr;
    }

    puts(data) { // Writes to native stdin
        for (const c of data) {
            this.stdinBuffer.push(c.charCodeAt(0));
        }
    }

    flush() { // Flushes to native stdin
        if (this._resolve) {
            this._resolve();
            this._resolve = null;
        }
    }

    _flushstdin() { // Invoked from native
        return new Promise((resolve) => {
            this._resolve = resolve;
        });
    }

    _flushstdout() { // Invoked from native
        this.onStdout(this.stdoutBuffer);
        this.stdoutBuffer = "";
    }

    _flushstderr() { // Invoked from native
        this.onStderr(this.stderrBuffer);
        this.stderrBuffer = "";
    }

    _handlestdin() {
        if (this.stdinBufferIdx >= this.stdinBuffer.length) {
            return null;
        }
        let ret = this.stdinBuffer[this.stdinBufferIdx];
        this.stdinBufferIdx++;
        if (this.stdinBufferIdx >= this.stdinBuffer.length) {
            // Resetting buffer
            this.stdinBuffer.length = 0;
            this.stdinBufferIdx = 0;
        }
        return ret
    }

    _handlestdout(c) {
        this.stdoutBuffer += (c == 10) ? "\r\n" : String.fromCharCode(c); // For xterm compatibility
		if (c == 10)
			this._flushstdout();
    }

    _handlestderr(c) {
        this.stderrBuffer += (c == 10) ? "\r\n" : String.fromCharCode(c); // For xterm compatibility
    }

    init() {
        window._STDIO = this;
        FS.init(this._handlestdin.bind(this), this._handlestdout.bind(this), this._handlestderr.bind(this));
    }
}

const stdio = new Stdio();

(function (stdio) {
    var theme = {
        foreground: "#eff0eb",
        background: "#282a36",
        selection: "#97979b33",
        black: "#282a36",
        brightBlack: "#686868",
        red: "#ff5c57",
        brightRed: "#ff5c57",
        green: "#5af78e",
        brightGreen: "#5af78e",
        yellow: "#f3f99d",
        brightYellow: "#f3f99d",
        blue: "#57c7ff",
        brightBlue: "#57c7ff",
        magenta: "#ff6ac1",
        brightMagenta: "#ff6ac1",
        cyan: "#9aedfe",
        brightCyan: "#9aedfe",
        white: "#f1f1f0",
        brightWhite: "#eff0eb"
    };
    var term = new Terminal({
        fontFamily: '"Cascadia Code", Menlo, monospace',
        theme: theme,
        cursorBlink: false,
        allowProposedApi: false
    });
    const fitAddon = new FitAddon.FitAddon();
    term.loadAddon(fitAddon);
    term.open(document.getElementById("terminal"));
    fitAddon.fit();

    window.addEventListener("resize", () => {
        fitAddon.fit();
    });

    let lineBuffer = "";

    term.onData(e => {
        switch (e) {
            case "\r": // Enter
                term.write("\r\n");
                processLine(lineBuffer + "\n");
                lineBuffer = "";
                break;
            case "\u007F": // Backspace (DEL)
                if (lineBuffer.length > 0) {
                    term.write("\b \b");
                    if (lineBuffer.length > 0) {
                        lineBuffer = lineBuffer.substring(0, lineBuffer.length - 1);
                    }
                }
                break;
            default:
                if (e >= String.fromCharCode(0x20) && e <= String.fromCharCode(0x7E) || e >= "\u00a0") {
                    lineBuffer += e;
                    term.write(e);
                }
        }
    });
    term.focus();

    function processLine(text) {
        stdio.puts(text);
        stdio.flush();
    }

    stdio.onStdout = function (text) {
        term.write(text);
    }

    stdio.onStderr = function (text) {
        console.log(text);
    }
})(stdio);

const queryString = window.location.search;
const urlParams = new URLSearchParams(queryString);
const file = urlParams.get('file')
var cmdline_args = [];
if (file){
	cmdline_args = [file+".t3"];
}


var Module = {
	arguments: cmdline_args,
    "preRun": [() => {
        stdio.init();
    }
	]
};

document.write("<script src='./" + file + ".js'></script>");

