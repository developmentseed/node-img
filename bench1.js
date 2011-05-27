var fs = require('fs');
var img = require('./');
var util = require('util');
var EventEmitter = require('events').EventEmitter;

function Queue(callback, concurrency) {
    this.callback = callback;
    this.concurrency = concurrency || 10;
    this.next = this.next.bind(this);
    this.invoke = this.invoke.bind(this);
    this.queue = [];
    this.running = 0;
}
util.inherits(Queue, EventEmitter);

Queue.prototype.add = function(item, start) {
    this.queue.push(item);
    if (this.running < this.concurrency && start !== false) {
        this.running++;
        this.next();
    }
};

Queue.prototype.start = function() {
    while (this.running < this.concurrency) {
        this.running++;
        this.next();
    }
};

Queue.prototype.invoke = function() {
    if (this.queue.length) {
        this.callback(this.queue.shift(), this.next);
    } else {
        this.next();
    }
};

Queue.prototype.next = function(err) {
    if (this.queue.length) {
        process.nextTick(this.invoke);
    } else {
        this.running--;
        if (!this.running) {
            this.emit('empty');
        }
    }
};




// Actual benchmarking code:
var iterations = 200;
var concurrency = 10;


var images = [
    fs.readFileSync('test/fixture/1.png'),
    fs.readFileSync('test/fixture/2.png'),
    fs.readFileSync('test/fixture/3.png'),
    fs.readFileSync('test/fixture/4.png'),
    fs.readFileSync('test/fixture/5.png')
];

var written = false;
var queue = new Queue(function(i, done) {
    img.fromBuffer(images[0])
        .overlay(images[1])
        .overlay(images[2])
        .overlay(images[3])
        .overlay(images[4])
        .asPNG({}, function(err, data) {
            if (!written) {
                fs.writeFileSync('./out.png', data);
                written = true;
            }
            done();
        });
}, concurrency);

queue.on('empty', function() {
    var msec = Date.now() - start;
    console.warn('Iterations: %d', iterations);
    console.warn('Concurrency: %d', concurrency);
    console.warn('Per second: %d', iterations / (msec / 1000));
});

for (var i = 1; i <= iterations; i++) {
    queue.add(i, false);
}

var start = Date.now();
queue.start();
