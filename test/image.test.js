var assert = require('assert');
var img = require('..');

exports['create empty image'] = function() {
    var image = new img.Image;
    assert.equal('' + image, '[Image 0x0]');
};