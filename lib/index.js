var img = module.exports = exports = require('../build/default/img');
var fs = require('fs');

img.fromBuffer = function(buffer, callback) {
    return new img.Image().load(buffer, callback);
};
//
// img.fromFile = function(filename, callback) {
//     var image = new img.Image();
//     fs.readFile(filename, function(err, buffer) {
//         if (err) {
//             callback(err);
//         } else {
//             image.load(buffer, callback);
//         }
//     });
//     return image;
// };

img.Image.prototype.toString = function() {
    return '[Image ' + this.width + 'x' + this.height + ']';
};

// Move to C++ land?
var overlay = img.Image.prototype.overlay;
img.Image.prototype.overlay = function(image) {
    // Begin processing again once the image is loaded.
    if (!image.width) image.once('load', this.process.bind(this));

    return overlay.apply(this, arguments);
};

/*
Image()
Image#load(buffer)           // sets #data to a decoded RGBA buffer
                             // emits 'load' when done
Image#data GETTER instanceof Buffer // === RGBA buffer or null when no image is loaded
Image#width                  // getter for image width. returns 0 until the image has been loaded
Image#height                 // getter for image height. returns 0 until the image has been loaded
Image#asPNG(callback)        // function that takes callback which receives a PNG buffer
Image#asJPEG(callback)       //
Image#merge(img1, img2, callback?)      // merges img1, and img2
Image#merge([img1, 100, 200], img2, callback?)      // merges img1 at 100,200 and img2 at 0,0
*/

