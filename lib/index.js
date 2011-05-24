var img = module.exports = exports = require('../build/default/img');
var Buffer = require('buffer').Buffer;
var fs = require('fs');

img.fromBuffer = function(buffer, callback) {
    return new img.Image().load(buffer, callback);
};

img.Image.prototype.toString = function() {
    return '[Image ' + this.width + 'x' + this.height + ']';
};

// Move to C++ land?
var overlay = img.Image.prototype.overlay;
img.Image.prototype.overlay = function(image) {
    // Allow passing in PNG buffers instead of image objects.
    if (Buffer.isBuffer(image)) {
        image = new img.Image().load(image);
    }

    // Begin processing again once the image is loaded.
    if (!image.width) image.once('load', this.process.bind(this));

    return overlay.apply(this, arguments);
};
