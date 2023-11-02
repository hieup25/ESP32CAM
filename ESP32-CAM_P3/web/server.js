const express = require('express');
const http = require('http');
var cors = require('cors');
const app = express();
app.use(cors());
app.use(express.static(__dirname)); // thay __dirname bang duong dan muon
app.use(express.json()) // for parsing application/json
app.use(express.urlencoded({ extended: true })) // for parsing application/x-www-form-urlencoded

var temp_status_trash = [];
app.get('/', function (req, res) {
   res.sendFile(__dirname + '/home.html');
});

var server = app.listen(9000, function () {
   var host = server.address().address;
   var port = server.address().port;
   console.log("Example app listening at http://%s:%s", host, port)
});