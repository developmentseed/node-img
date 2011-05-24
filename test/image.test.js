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
            if (err) throw new Error(err);
            fs.writeFileSync('./out.png', data);
            completed = true;
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