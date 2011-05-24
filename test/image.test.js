var assert = require('assert');
var fs = require('fs');
var img = require('..');

exports['create empty image'] = function(beforeExit) {
    var completed = false;

    var image = new img.Image;
    assert.equal('' + image, '[Image 0x0]');
    var file = fs.readFileSync('test/fixture/1.png');
    image.load(file, function(err) {
        completed = true;
        if (err) throw err;
        assert.equal('' + image, '[Image 256x256]');
    });

    beforeExit(function() {
        assert.ok(completed);
    });
};