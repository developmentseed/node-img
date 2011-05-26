var assert = require('assert');
var Buffer = require('buffer').Buffer;
var fs = require('fs');
var img = require('..');


var images = [
    fs.readFileSync('test/fixture/1.png'),
    fs.readFileSync('test/fixture/2.png'),
    fs.readFileSync('test/fixture/3.png'),
    fs.readFileSync('test/fixture/4.png'),
    fs.readFileSync('test/fixture/5.png')
];

exports['test first argument bogus'] = function() {
    assert.throws(function() {
        img.blend(true);
    }, /First argument must be an array of Buffers/);
};

exports['test first argument empty'] = function() {
    assert.throws(function() {
        img.blend([]);
    }, /First argument must contain at least one Buffer/);
};

exports['test bogus elements in array'] = function() {
    assert.throws(function() {
        img.blend([1,2,3]);
    }, /All elements must be Buffers/);
};

exports['test blend function with topmost image having no alpha'] = function(beforeExit) {
    var completed = false;
    img.blend([ images[1], images[0] ], function(err, data) {
        completed = true;
        if (err) throw err;
        assert.deepEqual(images[0], data);
    });

    beforeExit(function() { assert.ok(completed); });
};

exports['test blend function'] = function(beforeExit) {
    var completed = false;

    img.blend(images, function(err, data) {
        completed = true;
        if (err) throw err;
        // assert.notDeepEqual(images[4], data);
        fs.writeFileSync('out.png', data);
    });

    beforeExit(function() {
        assert.ok(completed);
    });
};
