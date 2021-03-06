'use strict'

const path = require('path');
const fs = require('fs');
const domain = require('domain');

// add builtin node_modules path to NODE_PATH to load silkedit module globally
if (process.env.NODE_PATH) {
  process.env.NODE_PATH = path.join(__dirname, 'node_modules') + ':' + process.env.NODE_PATH
} else {
  process.env.NODE_PATH = path.join(__dirname, 'node_modules')
}

require('module').Module._initPaths();

if (process.argv.length < 2) {
  console.log('missing argument.');
  return;
}

// cache silkedit module to share it globaly
const silkedit = require('silkedit');

// call init.js
const initPath = path.join(silkedit.Constants.silkHomePath, 'init.js');
fs.open(initPath, 'r', (err, fd) => {
  fd && fs.close(fd, (err) => {
    const d = domain.create();
    d.on('error', function(er) {
      console.error(er.stack);
    });
    d.run(function() {
      require(initPath);
    });
  });
});