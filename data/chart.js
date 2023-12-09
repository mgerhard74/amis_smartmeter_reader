var dynamicChart;
var channelsLoaded = 0;
var channelKeys =[];
var delta = 0;
var myOffset = new Date().getTimezoneOffset();
var nResults=2880;

myOffset *= 60*1000;

// converts date format from JSON
function getChartDate(d) {
    var date=new Date(d);
    return date.getTime()-myOffset;
}

function HideAll(){
  for (var index=0; index<dynamicChart.series.length; index++)  // iterate through each series
  {
    if (dynamicChart.series[index].name == 'Navigator')
      continue;
    dynamicChart.series[index].hide();
  }
}

String.prototype.replaceAt=function(index, replacement) {
    return this.substr(0, index) + replacement+ this.substr(index + replacement.length);
}

function save_visibility(idx,vis) {
  var str = readCookie(config_general.devicename);
  str=str.replaceAt(idx,vis);
  createCookie(config_general.devicename,str,1000);
}

function createCookie(name,value,days) {
  if (days) {
    var date = new Date();
    date.setTime(date.getTime()+(days*24*60*60*1000));
    var expires = "; expires="+date.toGMTString();
  }
  else var expires = "";
  //document.cookie = name+"="+value+expires+"; path=/";
  // Because most browsers don't store cookies for local scripts, we use an other appraoch
  localStorage.setItem('cookie',name+"="+value+expires+"; path=/");
}

function readCookie(name) {
  //var ca = document.cookie.split('=');
  try {
    var ca = localStorage.getItem('cookie').split('=');
  }
  catch (err) {
    return '000010000000010000';
  }
  if (ca.length<2) {
    createCookie(config_general.devicename,'000010000000010000',1000);
    return '000010000000010000';
  }
  else {
    return ca[1];
  }
}

//  This is where the chart is generated.
function onload2() {
  nResults = Math.round(1440 * 60 / config_general.thingspeak_iv) ;
  if (!isFinite(nResults)) nResults=2880;
  channelKeys.push({channelId:config_general.channel_id, key:config_general.read_api_key,
                   fieldList:[{field:1,axis:'Wh'},{field:2,axis:'Wh'},{field:3,axis:'varh'},{field:4,axis:'varh'},
                              // {field:5,axis:'W'},{field:6,axis:'W'},{field:7,axis:'var'},{field:8,axis:'var'}]});
                              {field:5,axis:'W'},{field:6,axis:'W'},{field:7,axis:'var'},{field:8,axis:'var'},{field:9,axis:'W',name:'Saldo P+ P-'}]});
  var ch2=config_general.channel_id2;
  if (ch2 !=0)
    channelKeys.push({channelId:ch2, key:config_general.read_api_key2,
                     fieldList:[{field:1,axis:'Wh'},{field:2,axis:'Wh'},{field:3,axis:'varh'},{field:4,axis:'varh'},
                                {field:5,axis:'W'},{field:6,axis:'W'},{field:7,axis:'var'},{field:8,axis:'var'}]});
  var last_date;
  var seriesCounter=0
  for (var channelIndex=0; channelIndex<channelKeys.length; channelIndex++) {                      // iterate through each channel
    for (var fieldIndex=0; fieldIndex<channelKeys[channelIndex].fieldList.length; fieldIndex++){  // iterate through each channel
        channelKeys[channelIndex].fieldList[fieldIndex].series = seriesCounter;
        seriesCounter++;
    }
  }
  for (var channelIndex=0; channelIndex<channelKeys.length; channelIndex++) {      // iterate through each channel
    channelKeys[channelIndex].loaded = false;
    loadThingSpeakChannel(channelIndex,channelKeys[channelIndex].channelId,channelKeys[channelIndex].key,channelKeys[channelIndex].fieldList);
  }

  function loadThingSpeakChannel(sentChannelIndex,channelId,key,sentFieldList) {
    var fieldList= sentFieldList;
    var channelIndex = sentChannelIndex;
    var len= fieldList.length;
    if (channelIndex===0) len--;        // calc. Saldo   Spline Nr. 9
    var s=('http://api.thingspeak.com/channels/'+channelId+'/feed.json?offset=0&results=300&api_key='+key);
    $.getJSON(s, function(data) {
      for (var fieldIndex = 0; fieldIndex < len; fieldIndex++) {  // iterate through each field
        fieldList[fieldIndex].data = [];
        for (var h = 0; h < data.feeds.length; h++) {  // iterate through each feed (data point)
          var p = [];
          var fieldStr = "data.feeds[" + h + "].field" + fieldList[fieldIndex].field;
          var v = eval(fieldStr);
          p[0] = getChartDate(data.feeds[h].created_at);
          p[1] = parseFloat(v);
          fieldList[fieldIndex].data.push(p);
        }
        fieldList[fieldIndex].name = eval("data.channel.field" + fieldList[fieldIndex].field);
      }
      if (channelIndex===0) {                         // calc. Saldo
        fieldList[8].data=[];                         // Saldo P+P-
        fieldList[4].data.forEach((d,i) => {          // P+
          var p=[];
          p[0]=d[0];
          p[1]=(d[1]-fieldList[5].data[i][1]);       // P-
          fieldList[8].data.push(p);                        }
        )
      }
      //Add Channel Load Menu
      var menu = document.getElementById("ChannelSelect");
      var menuOption = new Option(data.channel.name, channelIndex);
      menu.options.add(menuOption, channelIndex);
      channelKeys[channelIndex].fieldList = fieldList;
      channelKeys[channelIndex].loaded = true;
      channelsLoaded++;
      if (channelsLoaded == channelKeys.length) {
        createChart();
      }
    })
    .fail(function() { alert('Datenabruf mit getJSON fehlgeschlagen.\n\
Abgesehen von fehlerhaften Einstellungen, z.B. Read Api Key, tritt dieses Problem\
nur mit manchen Android-Handys auf. Hinweise zur Behebung sind erwünscht!');
    return;
    });
  }

  function createChart() {
  var save_flag = false;
  var chartOptions = {
    chart:{
      renderTo: 'chart-container',
      zoomType:'x',
      events:{
        load: function() {
          setInterval(function()  {
            if (document.getElementById("Update").checked)  {
              for (var channelIndex=0; channelIndex<channelKeys.length; channelIndex++) {  // iterate through each channel
                (function(channelIndex) {
                  var p8=[];
                  $.getJSON('https://api.thingspeak.com/channels/'+channelKeys[channelIndex].channelId+'/feed/last.json?offset=0&location=false&api_key='+channelKeys[channelIndex].key, function(data) {
                    for (var fieldIndex=0; fieldIndex < channelKeys[channelIndex].fieldList.length; fieldIndex++) {
                      var fieldStr = "data.field"+channelKeys[channelIndex].fieldList[fieldIndex].field;
                      var chartSeriesIndex=channelKeys[channelIndex].fieldList[fieldIndex].series;
                      if  (chartSeriesIndex !== 8) {    // calc. Saldo   Spline Nr. 9
                        if (data && eval(fieldStr)) {
                          var p = [];                   //new Highcharts.Point();
                          var v = eval(fieldStr);
                          p[0] = getChartDate(data.created_at);
                          p[1] = parseFloat(v);
                          if (chartSeriesIndex === 4) {
                            p8[1] = p[1];
                          }
                          if (chartSeriesIndex === 5) {
                            p8[0] = p[0];
                            p8[1] -= p[1];                 // calc. Saldo   Spline Nr. 9
                          }
                          if (dynamicChart.series[chartSeriesIndex].data.length > 0) {
                            last_date = dynamicChart.series[chartSeriesIndex].data[dynamicChart.series[chartSeriesIndex].data.length - 1].x;
                          }
                          if (p[0] != last_date) {
                            dynamicChart.series[chartSeriesIndex].addPoint(p, false); // do NOT redraw now
                          }
                        }
                      }
                      else {                                // Saldo   Spline Nr. 9
                        dynamicChart.series[8].addPoint(p8, false); // do NOT redraw now
                        //console.log('P8: '+p8)
                      }
                    }
                  });
                })(channelIndex);
              }
              dynamicChart.redraw();
            }
          }, config_general.thingspeak_iv*1000);
        },
      }
    },
    rangeSelector: {  // Zoom top left
      buttons: [{
          count: 30,
          type: 'minute',
          text: '30M'
      }, {
          count: 3,
          type: 'hour',
          text: '3H'
      }, {
          count: 12,
          type: 'hour',
          text: '12H'
      }, {
          count: 1,
          type: 'day',
          text: 'D'
      }, {
          count: 1,
          type: 'week',
          text: 'W'
      }, {
          type: 'all',
          text: 'All'
      }],
      inputEnabled: false,  // Datepicker top right
      selected: 3           // Zoom-button D
    },
    title: {text:''},
    plotOptions: {
      line: {gapSize:5},
      series: {
        events: {
          mouseOut: function() {},
          legendItemClick: function () {
              save_flag = true;
          },
          show: function() {
            if (save_flag) {
              save_visibility(this._i,'1');  // this.index has missing numbers!!!
              save_flag=false;
            }
          },
          hide: function() {
            if (save_flag) {
              save_visibility(this._i,'0');
              save_flag=false;
            }
          }
        },
        marker: {radius: 2},
        animation: true,
        step: false,
        //turboThreshold:1000,
        borderWidth: 0
      }
    },
    xAxis: {
      //ordinal:false , //Leerraeume bei fehlenden Daten erzeugen hohe Renderlast
      type:'datetime',
      //title: {text: 'Date'}
    },
    yAxis: [
      // {title: {align:'high',text:'Wh'},opposite:true,id: 'Wh'},
      // {title: {align:'high',text:'varh'},opposite:true,id: 'varh'},
      // {title: {align:'high',text:'W'},   opposite:false,id: 'W'},
      // {title: {align:'high',text:'var'}, opposite:false,id: 'var'}],
      {title: {text: ''},id: 'Wh'},
      {title: {text: ''},opposite: true, id: 'varh'},
      {title: {text: ''},opposite: false,id: 'W'},
      {title: {text: ''},opposite: false,id: 'var'}],
    exporting: {enabled: false},
    legend: {enabled: true},
    navigator: {        // below chart time line
      enabled: false
      // baseSeries: 4,    // series used
      // series: {
      //   includeInCSVExport: false
      // }
    },
    credits: {
      enabled: false
    },
    tooltip: {
      // enabled: true ,
      valueDecimals: 1,
      //xDateFormat: '%b %e, %H:%M.%S'
      xDateFormat: '%Y-%m-%d %H:%M.%S'
      // split: false,  // option shared in multi-channel needs split false
      // shared: true
    },
    series: []
  }; // chartOptions

  var str = readCookie(config_general.devicename);
  for (var channelIndex=0; channelIndex < channelKeys.length; channelIndex++) { // iterate through each channel
    for (var fieldIndex=0; fieldIndex<channelKeys[channelIndex].fieldList.length; fieldIndex++) {   // add each field
      var lineIndex=channelKeys[channelIndex].fieldList[fieldIndex].series;
      var v= 1;
      try { v=(str[lineIndex] =='1') }
      catch(e) {}
      chartOptions.series.push({data:channelKeys[channelIndex].fieldList[fieldIndex].data,
                                index:lineIndex,
                                yAxis:channelKeys[channelIndex].fieldList[fieldIndex].axis,
                                tooltip:{valueSuffix: " "+channelKeys[channelIndex].fieldList[fieldIndex].axis},     // Einheiten-Bezeichner Tooltip
                                name: channelKeys[channelIndex].fieldList[fieldIndex].name,
                                visible: v });
    }
  }
  chartOptions.series[8].color='#55BF3B';                                   // calc. Saldo

  dynamicChart = new Highcharts.StockChart(chartOptions);

  for (var channelIndex=0; channelIndex<channelKeys.length; channelIndex++) {                       // iterate through each channel
    for (var fieldIndex=0; fieldIndex<channelKeys[channelIndex].fieldList.length; fieldIndex++)  {  // and each field
      for (var seriesIndex=0; seriesIndex<dynamicChart.series.length; seriesIndex++) {              // compare each series name
        if (dynamicChart.series[seriesIndex].name == channelKeys[channelIndex].fieldList[fieldIndex].name) {
          channelKeys[channelIndex].fieldList[fieldIndex].series = seriesIndex;
        }
      }
    }
  }
  for (var channelIndex=0; channelIndex<channelKeys.length; channelIndex++) { // iterate through each channel
    (function(channelIndex) {
        loadChannelHistory(channelIndex,channelKeys[channelIndex].channelId,channelKeys[channelIndex].key,channelKeys[channelIndex].fieldList,0,1);
      }
    )(channelIndex);
  }
 }
} // onload2

function loadOneChannel(){         // Button "Daten nachladen"
  var selectedChannel=document.getElementById("ChannelSelect");
  var maxLoads=document.getElementById("Loads").value ;
  var channelIndex = selectedChannel.selectedIndex;
  loadChannelHistory(channelIndex,channelKeys[channelIndex].channelId,channelKeys[channelIndex].key,channelKeys[channelIndex].fieldList,0,maxLoads);
}

// load next [results] points from a ThingSpeak channel and addPoints to a series
function loadChannelHistory(sentChannelIndex,channelId,key,sentFieldList,sentNumLoads,maxLoads) {
  // console.log(sentChannelIndex,channelId,key,sentFieldList,sentNumLoads,maxLoads)
  var numLoads=sentNumLoads
  var fieldList= sentFieldList;
  var channelIndex = sentChannelIndex;
  var len= fieldList.length;
  if (channelIndex===0) len--;        // calc. Saldo   Spline Nr. 9
  var first_Date = new Date();
  if (typeof fieldList[0].data[0] != undefined) {
    first_Date.setTime(fieldList[0].data[0][0]+(myOffset));  // Früheste Zeit der bisherigen Daten = Endwert neue Daten
  }
  var end = first_Date.toJSON();
  $.getJSON('https://api.thingspeak.com/channels/'+channelId+'/feed.json?offset=0&start=2018-01-01T00:00:00&end='+end+'&results='+nResults+'&api_key='+key, function(data){
  // Data is limited to 8000 points by ThingSpeak
     for (var fieldIndex = 0; fieldIndex < len; fieldIndex++) {  // iterate through each field
       var arr = [];
       for (var h = 0; h < data.feeds.length; h++) {                            // iterate through each feed (data point)
         var p = [];
         var fieldStr = "data.feeds[" + h + "].field" + fieldList[fieldIndex].field;
         var v = eval(fieldStr);
         p[0] = getChartDate(data.feeds[h].created_at);
         p[1] = parseFloat(v);
         arr.push(p);
       }
       fieldList[fieldIndex].data = arr.concat(fieldList[fieldIndex].data);
       dynamicChart.series[fieldList[fieldIndex].series].setData(fieldList[fieldIndex].data, false);
     }

     if (channelIndex===0) {                         // calc. Saldo
       var arr=[];
       for (var h = 0; h < data.feeds.length; h++) {
         var p = [];
         p[0] = fieldList[4].data[h][0];
         p[1] = (fieldList[4].data[h][1] - fieldList[5].data[h][1]);       // P-
         arr.push(p);
       }
       fieldList[8].data=arr.concat(fieldList[8].data)
       dynamicChart.series[fieldList[8].series].setData(fieldList[8].data, false);
     }
     channelKeys[channelIndex].fieldList = fieldList;
     dynamicChart.redraw();
     numLoads++;
     if (numLoads < maxLoads) {
       loadChannelHistory(channelIndex, channelId, key, fieldList, numLoads, maxLoads);
     }
  });
}

function highchartDestroy (reload) {
  try {
    if (dynamicChart) {
      dynamicChart.destroy();
    }
    channelsLoaded = 0;
    channelKeys = [];
    $('#ChannelSelect').empty();  //select box
    if (reload) onload2();
  }
  catch(e){};
}

window.onorientationchange=function(event){
  try {
    if (dynamicChart) {
      let b = (event.target.screen.orientation.angle == 0);
      if (!b) {     //landscape
        $(".chart-below").hide();
        $(".chart").height("100vh");
      }
      else {
        $(".chart-below").show();
        $(".chart").height("85vh");
      }
    }
  }
  catch(e){};
}