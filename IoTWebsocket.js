var connection = new WebSocket('ws://'+location.hostname+':81/', ['arduino']);
connection.onopen = function () {
    connection.send('Connect ' + new Date());
};
connection.onerror = function (error) {
    console.log('WebSocket Error ', error);
};
connection.onmessage = function (e) {
    console.log('Server: ', e.data);
    let msgObj = JSON.parse(e.data);
    document.getElementById('time').innerHTML = msgObj.time;
    document.getElementById('ambientTemp').innerHTML = msgObj.ambientTemp;
};
connection.onclose = function(){
    console.log('WebSocket connection closed');
};
