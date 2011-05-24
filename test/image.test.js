var assert = require('assert');
var Buffer = require('buffer').Buffer;
var fs = require('fs');
var img = require('..');

exports['create empty image'] = function(beforeExit) {
    var completed = false;

    var image = new img.Image;
    assert.equal('' + image, '[Image 0x0]');
    assert.equal(image.data, undefined);
    var file = fs.readFileSync('test/fixture/3.png');
    image.load(file, function(err) {
        if (err) throw err;
        assert.equal('' + image, '[Image 256x256]');
        assert.ok(Buffer.isBuffer(image.data));
        assert.equal(image.data.length, 4 * 256 * 256);
        assert.equal(image.width, 256);
        assert.equal(image.height, 256);
        image.asPNG({}, function(err, data) {
            completed = true;
            if (err) throw err;
            assert.equal(data.length, 9560);
        });
    });

    beforeExit(function() {
        assert.ok(completed);
    });
};

exports['try setting data'] = function(beforeExit) {
    var image = new img.Image;
    image.data = 42;
    assert.equal(image.data, undefined);
};

exports['test delayed overlay loading'] = function(beforeExit) {
    var completed = false;
    var image2 = new img.Image();
    setTimeout(function() {
        image2.load(fs.readFileSync('test/fixture/2.png'));
    }, 100);

    img.fromBuffer(fs.readFileSync('test/fixture/1.png'))
        .overlay(image2)
        .overlay(fs.readFileSync('test/fixture/3.png'))
        .asPNG({}, function(err, data) {
            completed = true;
            assert.ok(data.length >= 100000);
        });

    beforeExit(function() {
        assert.ok(completed);
    });
};

exports['test delayed asPNG'] = function(beforeExit) {
    var completed = false;
    var image = new img.Image();
    image.load(fs.readFileSync('test/fixture/4.png'));
    image.asPNG({}, function(err, data) {
        completed = true;
        assert.equal(data[0], 0x89);
        assert.equal(data[1], 0x50);
        assert.equal(data[2], 0x4E);
        assert.ok(data.length > 4000 && data.length < 5000);
    });

    beforeExit(function() {
        assert.ok(completed);
    });
};
