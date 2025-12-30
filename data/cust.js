var websock = null;
var wsUri = "";
var loginUri = "";
var UpdateUri = "";
const wday=["Montag","Dienstag","Mittwoch","Donnerstag","Freitag","Samstag","Sonntag"];
var ws_pingpong;
var logpage=1;
var logpages;
var e180,e280;
var yestd_in=null;
var yestd_out;
var monthlist;
var weekdata;
var restoreData;
var g_lastDT = new Date(); // letzte erhaltene Zeit
var wsTimer;
var wsActiveFlag=true;

var config_general = {
    "devicetype": "AMIS-Reader",
    "devicename": "Amis-1",
    "auth_passwd": "admin",
    "auth_user": "admin",
    "use_auth": false,
    "log_sys": false,
    "amis_key": "0",
    "thingspeak_aktiv": false,
    "channel_id": 0,
    "write_api_key":"",
    "read_api_key":"",
    "thingspeak_iv":30,
    "channel_id2":"",
    "read_api_key2":"",
    "rest_var": 0,
    "rest_ofs": 0,
    "rest_neg": false,
    "smart_mtr":false,
    "developerModeEnabled": false,
    "webserverTryGzipFirst": true, // diese Einstellung wird vom AmisLeser beim Lesen der Konfig nicht beachtet
    "switch_on": 0,
    "switch_off": 0,
    "switch_url_on": "",
    "switch_url_off": "",
    "switch_intervall": 60,
    "command": "/config_general"
};

var config_wifi= {
    "ssid": "",
    "wifipassword": "",
    "dhcp": false,
    "ip_static": "192.168.",
    "ip_netmask": "255.255.255.0",
    "ip_gateway": "192.168.",
    "ip_nameserver": "192.168.",
    "rfpower": 21,
    "mdns": false,
    "allow_sleep_mode": true,
    "pingrestart_do": false,
    "pingrestart_ip": "192.168.",
    "pingrestart_interval": 60,
    "pingrestart_max": 3,
    "command": "/config_wifi"
};
var config_mqtt={
    "mqtt_enabled": 0,
    "mqtt_broker": "192.168.",
    "mqtt_port": 1883,
    "mqtt_user": "",
    "mqtt_password": "",
    "mqtt_clientid": "",
    "mqtt_qos": 0,
    "mqtt_retain": false,
    "mqtt_keep": 30,
    "mqtt_pub":"amis/out",
    "mqtt_will": "",
    "mqtt_ha_discovery": true,
    "command": "/config_mqtt"
};

function toNumberString(value, numberOfDecimals) {
    return value.toFixed(numberOfDecimals).replace('.',',');
}

function UTCDate(d) {
    let r = new Date();
    r.setUTCMonth(1); // prevent any possible exception setting 29.02.XXXX
    r.setUTCFullYear(d.getUTCFullYear());
    r.setUTCMonth(d.getUTCMonth());
    r.setUTCDate(d.getUTCDate());
    r.setUTCHours(d.getUTCHours());
    r.setUTCMinutes(d.getUTCMinutes());
    r.setUTCSeconds(d.getUTCSeconds());
    r.setUTCMilliseconds(d.getUTCMilliseconds());
    return r;
}

function adjustDays(date, days) {
    var r = new Date(date.valueOf());
    r.setDate(r.getDate() + days);
    return r;
}

function secsSinceMidnight(dt) {
  // Anzahl der Sekunden seit Mitternacht berechnen
  // Sommer/Winterzeit wird berücksichtigt sofern im Browser/OS richtig eingestellt
  // Bsp: 2024/03/31 01:59:59 = 7199
  //      2024/03/31 02:00:00 = 7200 - dürfte es aber ja eigentlich nie geben (wäre falsche Eingabe)!
  //      2024/03/31 03:00:00 = 7200
  let da = new Date(dt.getFullYear(), dt.getMonth(), dt.getDate(),
                    dt.getHours(), dt.getMinutes(), dt.getSeconds());
  da = UTCDate(da);

  let dayBegin = new Date(dt.getFullYear(), dt.getMonth(), dt.getDate(),
                          0, 0, 0);
  dayBegin = UTCDate(dayBegin);

  let r = ((da.getTime() - dayBegin.getTime()) / 1000) | 0;
  return r;
}

function secsWholeDay(date) {
  // Anzahl der Sekunden eines ganzen Tages
  // Sommer/Winterzeit wird berücksichtigt sofern im Browser/OS richtig eingestellt
  // Bsp f Österreich (CET/CEST):
  //      2024/03/30 xx:xx:xx = 86400
  //      2024/03/31 xx:xx:xx = 82800
  //      2024/04/01 xx:xx:xx = 86400
  //      2024/10/26 xx:xx:xx = 86400
  //      2024/10/27 xx:xx:xx = 90000
  //      2024/10/28 xx:xx:xx = 86400
  let dayStart = new Date(date.getFullYear(), date.getMonth(), date.getDate(),
                          0, 0, 0);
  dayStart = UTCDate(dayStart);

  let dayEnd = new Date(date.getFullYear(), date.getMonth(), date.getDate(),
                        23, 59, 59);
  dayEnd = UTCDate(dayEnd);
  let r = ((dayEnd.getTime() - dayStart.getTime()) / 1000) | 0;
  return r + 1; // Die Sekunde von 23:59:59 auf 00:00:00 auch noch dazuzählen !
}

function timeDecoder(tc) {
  let hi = Number(tc.slice(0,4));
  let lo = Number("0x"+tc.slice(4,12));

  let secs = lo & 0x3f;
  lo >>= 8;
  let mins = lo & 0x3f;
  lo >>= 8;
  let hrs = lo & 0x1f;
  lo >>= 8;
  let day = lo & 0x1f;
  lo >>= 5;
  let month = hi & 0x0f;
  let year = lo & 0x07;
  year |= (hi & 0xf0) >> 1;
  year += 2000;

  g_lastDT = new Date(year, month-1, day, hrs, mins, secs);

  return year + '/' + zeroPad(month,2) + '/' + zeroPad(day,2) + '&nbsp;' +
         zeroPad(hrs,2) + ':' + zeroPad(mins,2) + ':' + zeroPad(secs,2);
}

function setAvgItem(basitemname, totalValue, numberOfSeconds, setcolorNegPos) {
    let fieldname = "#avg_" + basitemname;

    if (setcolorNegPos) {
        if (totalValue > 0) $(fieldname).css({'color':'#FF0000'});
        else                $(fieldname).css({'color':'#0000FF'});
    }
    $(fieldname).html("Ø " + toNumberString((totalValue * 3600) / numberOfSeconds / 1000, 3));
}

function updateElements(obj) {
  let pre;
  let post;
  let div;
  for (let [key, value] of Object.entries(obj)) {
    //console.log(key,value);
    if (key==='command') {
      switch (value) {
        case "/config_general":
          config_general=obj;
          if (config_general.thingspeak_aktiv) $(".menu-graf").show();
          else                                 $(".menu-graf").hide();
          if (config_general.developerModeEnabled) $(".menu-developer").show();
          else                                     $(".menu-developer").hide();
          break;
        case "/config_wifi":
          config_wifi=obj;
          break;
        case "/config_mqtt":
          config_mqtt=obj;
          break;
      }
    }
    else if (key==='now') {
      let v=Number(value);
      switch (v) {
        case 0:
          value = 'Warte auf Zählerdaten...';
          break;
        case 1:
        case 2:
        case 3:
        case 4:
          value = 'Amis-Key ungültig?';
          break;
        default:
          value = timeDecoder((value));
      }
    }
    /*
    else if (key==='serialnumber') {
      // nothing to do as 'key' matches the 'name' of the div element
    }
    */
    else if (key==='today_in') {   // Nur 1x nach dem Start
      let secsDayStart = secsSinceMidnight(g_lastDT);
      yestd_in=obj.yestd_in;
      yestd_out=obj.yestd_out;
      $("#tdy_in"  ).html(toNumberString((value-yestd_in)/1000, 3));
      setAvgItem("tdy_in", value-yestd_in, secsDayStart, false);
      $("#tdy_out" ).html(toNumberString((obj.today_out-yestd_out)/1000, 3));
      setAvgItem("tdy_out", obj.today_out-yestd_out, secsDayStart, false);
      var diff=(value-yestd_in)-(obj.today_out-yestd_out);
      if (diff >0) $("#tdy_diff").css({'color':'#FF0000'});
      else         $("#tdy_diff").css({'color':'#0000FF'});
      $("#tdy_diff").html(toNumberString(diff / 1000, 3));
      setAvgItem("tdy_diff", diff, secsDayStart, true);

      if (diff >0) $("#perhour_tdy").css({'color':'#FF0000'});
      else         $("#perhour_tdy").css({'color':'#0000FF'});
      if (secsDayStart > 0) {
        $("#perhour_tdy").html(toNumberString((diff * 3600) / secsDayStart / 1000, 3));
      } else {
        $("#perhour_tdy").html(toNumberString(diff / 1000, 3));
      }

      for (let i=0;i<7;i++ ) {
        let datax = "data" + i;
        let date = adjustDays(g_lastDT, -1 - i);
        if (datax in obj) {
          let secsDay = secsWholeDay(date);
          datax = obj[datax];
          if (i === 0) $("#wd0").html('Gestern');
          else $("#wd"+i).html(wday[datax[0]]);
          $("#wd_in" +i).html((datax[1] / 1000).toFixed(3).replace('.',','));
          setAvgItem("wd_in"+i, datax[1], secsDay, false);
          $("#wd_out"+i).html((datax[2] / 1000).toFixed(3).replace('.',','));
          setAvgItem("wd_out"+i, datax[2], secsDay, false);
          diff=datax[1]-datax[2];
          if (diff >0) $("#wd_diff"+i).css({'color':'#FF0000'});
          else         $("#wd_diff"+i).css({'color':'#0000FF'});
          $("#wd_diff"+i).html(toNumberString(diff / 1000, 3));
          setAvgItem("wd_diff"+i, diff, secsDay, true);

          if (diff >0) $("#perhour_"+i).css({'color':'#FF0000'});
          else         $("#perhour_"+i).css({'color':'#0000FF'});
          $("#perhour_"+i).html(toNumberString((diff * 3600) / secsDay / 1000, 3));
        }
      }
      continue;
    }
    else if (key==='1_7_0') {
      diff=value-obj['2_7_0'];
      if (diff >0) $("#saldo").css({'color':'#FF0000'});
      else         $("#saldo").css({'color':'#0000FF'});
      $("#saldo").html((diff / 1000).toFixed(3).replace('.',',')+' kW');
    }
    else if (key==='1_8_0') {
      if (e180 != value) {          // Energie-Tabelle auch aktualisieren
        $("#tdy_in").html(((value-yestd_in) / 1000).toFixed(3).replace('.',','));
        e180=value;
      }
    }
    else if (key==='2_8_0') {
      if (e280 != value) {          // Energie-Tabelle auch aktualisieren
        $("#tdy_out").html(((value-yestd_out) / 1000).toFixed(3).replace('.',','));
        e280 = value;
      }
      $("#tdy_diff").html((((e180-yestd_in)-(e280-yestd_out)) / 1000).toFixed(3).replace('.',','));
    }
    else if (key==='things_up') {
       if (value) value=timeDecoder(value);
       else value="";
    }
    else if (key==='page') {             // Logpanel
      logpage=obj["page"];
      logpages=obj["pages"];
      value="Seite "+value+" von "+logpages;
    }
    else if (key==='list') {             // Logpanel
      let tab='<table class="pure-table pure-table-striped" width="100%"><thead><tr><th>Zeit</th><th>Typ</th><th>Src</th><th>Inhalt</th><th>Daten</th></tr></thead><Tbody>';
      for (let i=0;i<value.length;i++ ) {
        let line=JSON.parse(value[i]);
        let t='- - -';
        if (line.time) {
          t=timeDecoder(line.time);
        }
        tab += '<tr><td>'+t+'</td><td>'+line.type+'</td><td>'+line.src+'</td><td>'+line.desc+'</td><td>'+line.data+'</td></tr>';
      }
      tab+='</tbody></table>';
      value=tab;
    }
    else if (key==='monthlist') {
      monthlist={command:"monthlist",month:value};
      if (yestd_in===null) continue;
      let t='<table class="pure-table pure-table-striped" width="100%">';
      t+='<thead><tr><th>Monat</th><th align="right">Bezug</th><th align="right">Lfrg.</th><th align="right">Diff.</th></tr></thead>';
      t+='<tbody>';
      let monatsEndBzg=e180;
      let monatsEndLfg=e280;                  // für den laufenden Monat die Top-Werte
      for (let i=value.length-1;i>=0;i-- ) {  // Tabelle bottom-up
        let arr = value[i].split(" ");
        let datum='20'+arr[0].slice(0,2)+'/'+arr[0].slice(2);
        let bzg=(monatsEndBzg-Number(arr[1]))/1000;
        let lfg=(monatsEndLfg-Number(arr[2]))/1000;
        let diff=(bzg-lfg);
        let c= '#0000FF';           // color for diff
        if (diff>0) c='#FF0000';
        t += '<tr><td>' + datum + '</td><td align="right">' + bzg.toFixed(3).replace('.',',') + '</td>';
        t += '<td align="right">' + lfg.toFixed(3).replace('.',',') + '</td>';
        t += '<td align="right" style="color:'+c+'">'+diff.toFixed(3).replace('.',',')+'</td></tr>';
        monatsEndBzg=arr[1];  // die Topwerte für den vorhergehenden Monat in der nächsten for-loop
        monatsEndLfg=arr[2];
      }
      t+='</tbody>';
      t+='</table>';
      $("#month_table").html(t);
    }
    else if (key==='uptime') {
      let uptime  = parseInt(value, 10);
      let seconds = uptime % 60;
      uptime = parseInt(uptime / 60, 10);
      let minutes = uptime % 60;
      uptime = parseInt(uptime / 60, 10);
      let hours   = uptime % 24;
      uptime = parseInt(uptime / 24, 10);
      value = uptime + "d " + hours + "h " + zeroPad(minutes, 2) + "m " + zeroPad(seconds, 2) + "s";
    }
    else if (key==='weekdata') {
      weekdata={command:"weekfiles",week:value};
    }
    else if (key==='ls') {
      for (let i=0; i < value;i++) {
        let s=String(i);
        console.log(obj[s]);
      }
    }
    else if (key === 'flashmode') {
      switch (value) {
        case '0': value = 'QIO';break;
        case '1': value = 'QOUT';break;
        case '2': value = 'DIO';break;
        case '3': value = 'DOUT';break;
        case '4': value = 'FAST-READ';break;
        case '5': value = 'SLOW-READ';break;
        case '0xff': value = 'unknown';break;
      }
    }
    else if (key === 'stations') {        // WiFi-Scan
      let tab = '<table class="pure-table pure-table-striped"><thead><tr><th>SSID</th><th>RSSI</th><th>Channel</th><th>Encryption</th></tr></thead><Tbody>';
      for (let i = 0; i < value.length; i++) {
        let station = JSON.parse(value[i]);
        let encr="Unknown";
        switch (station.encrpt) {
          case '7': encr="Open";break;
          case '5': encr="WEP";break;
          case '2': encr="WPA_PSK";break;
          case '4': encr="WPA2_PSK";break;
          case '8': encr="auto";break;
        }
        tab+='<tr><td>'+station.ssid+'</td><td>'+station.rssi+' dB</td><td>'+station.channel+'</td><td>'+encr+'</td></tr>'
      }
      tab += '</tbody></table>';
      value = tab;
    }

    // Look for INPUTs
    let input = $("input[name='" + key + "']");
    if (input.length > 0) {
      //console.log(input)
      if (input.attr("type") === "checkbox") {
        input.prop("checked", value);
      }
      else if (input.attr("type") === "radio")
        input.val([value]);
      else {
        pre = input.attr("pre") || "";
        post = input.attr("post") || "";
        input.val(pre + value + post);
      }
    }
    // Look for SPANs
    let span = $("span[name='" + key + "']");
    pre = span.attr("pre") || "";
    post = span.attr("post") || "";
    div = span.attr("div") || 0;
    if (div) {
      value = value / div;
      value=value.toFixed(3).replace('.',',');
    }
    span.html(pre + value + post);

    // Look for DIVs
    let divt = $("div[name='" + key + "']");
    pre = divt.attr("pre") || "";
    post = divt.attr("post") || "";
    div = divt.attr("div") || 0;
    if (div) {
      value = value / div;
      value=value.toFixed(3).replace('.',',');
    }
    divt.html(pre + value + post);

    // Look for SELECTs
    let select = $("select[name='" + key + "']");
    if (select.length > 0)
//      console.log(select)
      select.val(value);
  }
}

function socketMessageListener(evt) {   // incomming from ESP
  try {
    var obj = JSON.parse(evt.data);
  }
  catch (e) {                   // Debug-Ausgaben
    console.log("%c"+evt.data,"color:blue");
    return;
  }
  try {
    if ("devicetype" in obj) {
      let title = obj.devicetype;
      if ("devicename" in obj) title = obj.devicename + " - " + title;
      document.title = title;
    }
  }
  catch (e) {                   // Debug-Ausgaben
    console.log(e);
    console.log(obj);
    return;
  }
  updateElements(obj);
}

function connectWS() {
  clearInterval(wsTimer);
  websock = new WebSocket(wsUri);
  websock.addEventListener("message", socketMessageListener);
  websock.onopen = function(evt) {
    $("#panel-home").show();
    //$("#panel-graf").show();
    websock.send('{"command":"getconf"}');
    setTimeout( function(){
      websock.send('{"command":"energieWeek"}');
    },1200);
    setTimeout( function(){
      websock.send('{"command":"energieMonth","jahr":22}');
    },2500);
    //console.log("connectws")
    ws_pingpong = setInterval(function() {
      websock.send('{"command":"ping"}');
    }, 3000);
  }
  websock.onclose = function(evt) {
    clearInterval(ws_pingpong);
    wsTimer=setInterval(function() {
      if (wsActiveFlag) {
        connectWS();
      }
    }, 3000);
  }
}

function createCheckboxes() {
  $("input[type='checkbox']").each(function() {
    if($(this).prop("name")) $(this).prop("id", $(this).prop("name"));
    $(this).parent().addClass("toggleWrapper");
    $(this).after('<label for="' + $(this).prop("name") + '" class="toggle"><span class="toggle__handler"></span></label>')
  });
}

function zeroPad(number, positions) {
  return number.toString().padStart(positions, "0");
}

function loadTimeZones() {
  var time_zones = [
    -720, -660, -600, -570, -540,-480, -420, -360, -300, -240,-210, -180, -120, -60, 0,60, 120, 180, 210, 240,
    270, 300, 330, 345, 360,390, 420, 480, 510, 525,540, 570, 600, 630, 660, 720, 765, 780, 840 ];
  for(var i in time_zones) {
    var tz = time_zones[i];
    var offset = tz >= 0 ? tz : -tz;
    var text = "GMT" + (tz >= 0 ? "+" : "-") +
      zeroPad(parseInt(offset / 60, 10), 2) + ":" +
      zeroPad(offset % 60, 2);
    $("select[name='ntp_offset']").append(
      $("<option></option>")
      .attr("value", tz)
      .text(text)
    );
  }
}

function toggleMenu() {               // Hamburger Button
  $("#layout").toggleClass("active");
  $("#menu").toggleClass("active");     //???
  $("#menuLink").toggleClass("active"); //???
}

function beforHC () {
  if(window.confirm("Die nachfolgende Grafik wird mit einer Bibliothek von Highcharts.com erzeugt.\n\
Nur der private Gebrauch ist gestattet."))
  onload2();
}
function showPanel() {              // menu click select panel
  try {highchartDestroy(false);}
  catch(e) {};
  $(".panel").hide();
  if($("#layout").hasClass("active")) {
    toggleMenu();
  }
  $("#" + $(this).attr("data")).show();
  if ($(this).attr('data')=='panel-home') {
    if ($('#timing').prop('checked') == false) $(".home_details").hide();
  }
  if ($(this).attr('data')=='panel-graf') {
    try {
      onload2();    // create chart
    }
    catch (e) {     // dyn. load js-files
      jQuery.getScript('chart.js', function(){
        try {
          let test=Highcharts.version;
        }
        catch (e) {
          //console.log(e);
          // jQuery.getScript('https://cdnjs.cloudflare.com/ajax/libs/highstock/5.0.14/highstock.js', function(){
          jQuery.getScript('https://cdnjs.cloudflare.com/ajax/libs/highstock/6.0.3/highstock.js', function(){
            beforHC();
          });
          return;
        }
        beforHC();
      });
    }
  }
  if ($(this).attr('data')=='panel-general') {
    if ($('#use_auth').prop('checked') == false) $(".auth_details").hide();
    if ($('#thingspeak_aktiv').prop('checked') == false) $(".things_details").hide();
  }
  else if ($(this).attr('data')=='panel-mqtt') {
    if ($('#mqtt_enabled').prop('checked') == false) $(".mqtt_details").hide();
  }
  else if ($(this).attr('data')=='panel-status') {
    websock.send('{"command":"status"}');
  }
  else if ($(this).attr('data')=='panel-wifi') {
    if ($('#dhcp').prop('checked') == true) $(".wifi_details").hide();
    websock.send('{"command":"scan_wifi"}');
  }
  else if ($(this).attr('data')=='panel-log') {
    websock.send('{"command":"geteventlog","page":'+logpage+'}');
  }
  else if ($(this).attr('data')=='panel-update') {
    websock.send('{"command":"weekdata"}');
    greyButtons();
  }
}

function progressAnimate(id,time) {
  var pbar=$("#"+id);
  pbar.prop('max',time);
  pbar.val(0);
  pbar.show();
  var tm=time / (time/100);
  var iv=tm;
  var ticker=setInterval(function(){
    pbar.val(tm);
    tm+=iv;
    if (tm>time) {
      pbar.hide();
      clearInterval(ticker);
    }
  },tm);
}

function doUpdateGeneral() {                 // button save config
  progressAnimate('prgbar_general',300);
  //let boot=($("#use_auth").prop("checked")!=config_general.use_auth);
  $(".general").each(function () {
    //console.log($(this).prop('type'),this.name,this.value)
    if ($(this).prop('type') == 'checkbox') config_general[this.name] = $(this).prop('checked');
    else config_general[this.name] = this.value;
  });
  if (config_general.thingspeak_aktiv) $(".menu-graf").show();
  else $(".menu-graf").hide();
  if (config_general.developerModeEnabled) {
    $(".menu-developer").show();
  } else {
    $(".menu-developer").hide();
  }
  websock.send(JSON.stringify(config_general));
  //if (boot) doReboot("Wenn die Authentifizierung ein- oder ausgeschaltet wurde, muss neu gebootet werden.\n")
}
function doUpdateWiFi() {
  progressAnimate('prgbar_wifi',300);
  $(".wifi").each(function () {
    if ($(this).prop('type') == 'checkbox') config_wifi[this.name] = $(this).prop('checked');
    else if ($(this).prop('type') == 'text') {
      if (this.name!='ssid' && this.name!='wifipassword')  {
        config_wifi[this.name] = this.value.replaceAll(" ", "");
      }else config_wifi[this.name] = this.value;
    }
    else config_wifi[this.name] = this.value;
  })
  websock.send(JSON.stringify(config_wifi));
}
function doUpdateMQTT() {
  progressAnimate('prgbar_mqtt',300);
  $(".mqtt").each(function () {
    if ($(this).prop('type') == 'checkbox') config_mqtt[this.name] = $(this).prop('checked');
    else if ($(this).prop('type') == 'text')config_mqtt[this.name] = this.value;          // Keine Leerzeichen ersetzen ....= this.value.replaceAll(" ","");
    else config_mqtt[this.name] = this.value;
  })
  websock.send(JSON.stringify(config_mqtt));
}

function doReboot(msg) {
  if(window.confirm(msg+"Neustart mit OK bestätigen, dann 25s warten...")) {
    config_general.webserverTryGzipFirst = true;
    websock.send('{"command":"restart"}');
    doReload(25000);
  }
}

function doReload(milliseconds) {
  websock.close();
  $("#layout").hide();
  setTimeout(function() {
    window.location.reload();
  }, parseInt(milliseconds, 10));
}

function doUpgrade () {               // firmware update
  var file = $("input[name='upgrade']")[0].files[0];
  if(typeof file === "undefined") {
    alert("Zuerst muss eine lokale Datei gewählt werden!");
    return false;
  }
  if (file.name.endsWith("amis")) {
    amisRestore(file);
    return false;
  }
  var data = new FormData();
  data.append("update",file,file.name);       // www.mediaevent.de/javascript/ajax-2-xmlhttprequest.html
  var xhr = new XMLHttpRequest();     // https://javascript.info/xmlhttprequest
  var msg_ok = "Firmware geladen, Gerät wird neu gestartet. Die Verbindung wird in 25 Sekunden neu aufgebaut.";
  var msg_err = "Fehler beim Laden der Datei. Bitte wiederholen. ";

  var network_error = function(e) {
    alert(msg_err + " xhr request " + e.type);
  };
  xhr.addEventListener("error", network_error, false);
  xhr.addEventListener("abort", network_error, false);
  xhr.addEventListener("load", function(e) {
    if(xhr.status===200) {
        if (file.name.startsWith("firmware") || file.name.startsWith("littlefs")) {
          const milliseconds_s = Date.now();
          $("#prgbar_update").hide();
          alert(msg_ok);
          const alertTimeNeededMs = Date.now() - milliseconds_s;
          if (alertTimeNeededMs >= 25000) {
              doReload(0);
          } else {
              doReload(25000 - alertTimeNeededMs);
          }
        }
    }
    else alert(msg_err + xhr.status.toString() + " " + xhr.statusText + ", " + xhr.responseText);
  }, false);
  // xhr.upload.onprogress liefert keine vernünftigne Daten im Kurzzeitbereich
  xhr.open("POST",UpdateUri);
  xhr.send(data);
  if (file.name.startsWith("firmware") || file.name.startsWith("littlefs"))
    progressAnimate('prgbar_update',15000);
  else progressAnimate('prgbar_update',400);
}

function mqttDetails() {  // display settings only if mqtt active
  if ($(this).prop('checked')) $(".mqtt_details").show();
  else $(".mqtt_details").hide();
}

function wifiDetails() {  // display settings only if dhcp active
  if ($(this).prop('checked')) $(".wifi_details").hide();
  else $(".wifi_details").show();
}

function thingsDetails() {  // display settings only if thingspeak active
  if ($(this).prop('checked')) $(".things_details").show();
  else $(".things_details").hide();
}

function authDetails() {  // display settings only if auth active
  if ($(this).prop('checked')) $(".auth_details").show();
  else $(".auth_details").hide();
}

function developerModeEnabled() {  // display settings only if auth active
  if ($(this).prop('checked')) {
    $(".menu-developer").show();
    //websock.send('{"command":"set-developer-mode", "value":"on"}');
  } else {
    $(".menu-developer").hide();
    //websock.send('{"command":"set-developer-mode", "value":"off"}');
  }
}

function webserverTryGzipFirst() {  // display settings only if auth active
  if ($(this).prop('checked')) {
    websock.send('{"command":"set-webserverTryGzipFirst", "value":"on"}');
  } else {
    websock.send('{"command":"set-webserverTryGzipFirst", "value":"off"}');
  }
}

function sel_api(i) {
 config_general.rest_var=i;
}

function login() {
  var xhr = new XMLHttpRequest();
  xhr.onload = function (e) {
    if (xhr.readyState === 4) {
      if (xhr.status === 200) {
        //console.log("login")
        connectWS();
      }
    }
  }
  xhr.open("get", loginUri);
  xhr.send(null);
}

function toggleVisiblePassword() {
  var elem = this.previousElementSibling;
  if(elem.type === "password") {
    elem.type = "text";
  }
  else {
    elem.type = "password";
  }
  return false;
}

function doLogNext() {
  // refresh current page or goto next
  if (++logpage > logpages) {
    logpage = logpages;
  }
  websock.send('{"command":"geteventlog","page":'+logpage+'}');
}

function doLogPrev() {
  if (--logpage < 1) {
    logpage = 1;
  }
  websock.send('{"command":"geteventlog","page":'+logpage+'}');
}

function doLogClear () {
  if(window.confirm("Log-Datei löschen. Sicher?")) {
    websock.send('{"command":"clearevent"}');
    logpage=1;
    doLogPrev();
  }
}

function test () {
  websock.send('{"command":"test"}');
}

function clear () {
  websock.send('{"command":"clear"}');
}

function storeToFile() {
  var filename = $("input[name='konfigname']")[0].value;
  if (!filename.endsWith("amis")) {
    alert("Muss *.amis heißen!");
    return false;
  }
  greyButtons();
  /* save as blob */
  var textToSave = JSON.stringify({general:config_general,wifi:config_wifi,mqtt:config_mqtt,month:monthlist,week:weekdata},null,2);
  var textToSaveAsBlob = new Blob([textToSave], {
    type: "text/plain"
  });
  var textToSaveAsURL = window.URL.createObjectURL(textToSaveAsBlob);
  /* download without button hack */
  var downloadLink = document.createElement("a");
  downloadLink.download = filename;
  downloadLink.innerHTML = "Download File";
  downloadLink.href = textToSaveAsURL;
  downloadLink.onclick = function () {
    document.body.removeChild(event.target);
  };
  downloadLink.style.display = "none";
  document.body.appendChild(downloadLink);
  downloadLink.click();
}

function doRestoreGeneral() {
  if ($(".button-restore-general").hasClass('button-green')) {
    progressAnimate('prgbar_update',400);
    websock.send(JSON.stringify(restoreData['general']));
    updateElements(restoreData['general']);
  }
}
function doRestoreWifi() {
  if ($(".button-restore-wifi").hasClass('button-green')) {
    progressAnimate('prgbar_update',400);
    websock.send(JSON.stringify(restoreData['wifi']));
    updateElements(restoreData['wifi']);
  }
}
function doRestoreMqtt() {
  if ($(".button-restore-mqtt").hasClass('button-green')) {
    progressAnimate('prgbar_update',400);
    websock.send(JSON.stringify(restoreData['mqtt']));
    updateElements(restoreData['mqtt']);
  }
}
function doRestoreMonth() {
  if ($(".button-restore-month").hasClass('button-green')) {
    progressAnimate('prgbar_update',400);
    websock.send(JSON.stringify(restoreData['month']));
  }
}
function doRestoreWeek() {
  if ($(".button-restore-week").hasClass('button-green')){
    progressAnimate('prgbar_update',400);
    websock.send(JSON.stringify(restoreData['week']));
  }
}

function greyButtons() {
  $(".button-restore-general").removeClass("button-green");
  $(".button-restore-wifi").removeClass("button-green");
  $(".button-restore-mqtt").removeClass("button-green");
  $(".button-restore-month").removeClass("button-green");
  $(".button-restore-week").removeClass("button-green");
}

function amisRestore(file) {
  greyButtons();
  restoreData=null;
  if (!file) return;
  var reader = new FileReader();
  reader.onload = function(e) {
    restoreData=JSON.parse(e.target.result);
    for (let [key, value] of Object.entries(restoreData)) {
       //console.log(key);
       switch (key) {
         case 'general':
            $(".button-restore-general").addClass("button-green");    // Green
            break;
         case 'wifi':
            $(".button-restore-wifi").addClass("button-green");    // Green
            break;
         case 'mqtt':
            $(".button-restore-mqtt").addClass("button-green");    // Green
            break;
         case 'month':
            $(".button-restore-month").addClass("button-green");    // Green
            break;
         case 'week':
            $(".button-restore-week").addClass("button-green");    // Green
            break;
       }
    }
  }
  reader.readAsText(file)
}

document.addEventListener("visibilitychange", function (event) {
  if (document.hidden) {
    //console.log("hidden");
    wsActiveFlag=false;
    websock.close();
  }
  else {
    //console.log("visible");
    wsActiveFlag=true;
  }
});

$(function() {            // main
  loadTimeZones();
  createCheckboxes();

  $(".password-reveal").on("click", toggleVisiblePassword);
  $("#menuLink").on("click", toggleMenu);
  $(".pure-menu-link").on("click", showPanel);
  $(".button-update-general").on("click", doUpdateGeneral);  // save config
  $(".button-update-wifi").on("click", doUpdateWiFi);  // save config
  $(".button-update-mqtt").on("click", doUpdateMQTT);  // save config
  //$(".button-update-ntp").on("click", doUpdateNTP);  // save config
  $(".button-log-prev").on("click", doLogPrev);  // Logpage
  $(".button-log-next").on("click", doLogNext);  // Logpage
  $(".button-log-clear").on("click", doLogClear);  // Logpage
  $(".button-hist_clear").on("click", function(){
    if(window.confirm("Wochentage Daten löschen. Sicher?")) {
      websock.send('{"command":"clearhist"}');
      for (let i=0;i<7;i++ ) {
        $("#wd"+i).html('');
        $("#wd_in" +i).html('');
        $("#wd_out"+i).html('');
        $("#wd_diff"+i).html('');
      }
    }
  });
  $(".button-hist_clear2").on("click", function(){
    if(window.confirm("Monatsdaten Liste löschen. Sicher?")) {
      websock.send('{"command":"clearhist2"}');
    }
  });
  $(".button-reboot").on("click", function () {
    doReboot("");
  });
  $(".button-graf").on("click", function (){
    highchartDestroy(true);
  });

  /* Development - Buttons -  Start */
  $(".button-dev-tools-button1").on("click", function () {
    websock.send('{"command":"dev-tools-button1"}');
  });
  $(".button-dev-tools-button2").on("click", function () {
    websock.send('{"command":"dev-tools-button2"}');
  });
  $(".button-dev-cmd-factory-reset-reboot").on("click", function () {
    if(window.confirm("Alle Daten werden gelöscht! Mit OK bestätigen, dann 25s warten...")) {
      config_general.webserverTryGzipFirst = true;
      websock.send('{"command":"factory-reset-reboot"}');
      doReload(25000);
    }
  });
  $(".button-dev-send-json").on("click", function () {
    var js = $('textarea#dev-text-json').val();
    websock.send(js);
  });
  $(".button-dev-cmd-test").on("click", function () {
    websock.send('{"command":"test"}');
  });
  $(".button-dev-cmd-ls").on("click", function () {
    websock.send('{"command":"ls"}');
  });
  $(".button-dev-cmd-clear").on("click", function () {
    websock.send('{"command":"clear"}');
  });
  /* Development - Buttons -  End */

  $("input[name='mqtt_enabled']").on("click", mqttDetails);
  $("input[name='dhcp']").on("click", wifiDetails);
  $("input[name='thingspeak_aktiv']").on("click", thingsDetails);
  $("input[name='developerModeEnabled']").on("click", developerModeEnabled);
  $("input[name='webserverTryGzipFirst']").on("click", webserverTryGzipFirst);
  //$("input[name='smart_aktiv']").on("click", smart_mtr);
  $("input[name='use_auth']").on("click", authDetails);
  $(".button-upgrade").on("click", doUpgrade);      // firmware update
  $(".button-upgrade-browse").on("click", function() {
    $("input[name='upgrade']")[0].click();  // unsichtbaren Button klicken!
    return false;
  });
  $(".button-savekonfig").on("click", storeToFile);
  $("input[name='upgrade']").change(function() {
    var file = this.files[0];               // copy Auswahl vom unsichtbaren auf sichtbaren Input
    $("input[name='filename']").val(file.name);
    if (file.name.endsWith("amis")) {
      amisRestore(file);
    }
    else greyButtons();
  });
  $(".button-restore-general").on("click", doRestoreGeneral);
  $(".button-restore-wifi").on("click", doRestoreWifi);
  $(".button-restore-mqtt").on("click", doRestoreMqtt);
  $(".button-restore-month").on("click", doRestoreMonth);
  $(".button-restore-week").on("click", doRestoreWeek);
  updateElements(config_general);     // vordefinierte configs übernehmen
  updateElements(config_wifi);
  updateElements(config_mqtt);
  let host=window.location.hostname;
  let proto=window.location.protocol;
  let wsProto="ws:";
  if (proto.endsWith("https:")) {
      wsProto="wss:";
  }
  wsUri = wsProto + "//" + host + "/ws";
  loginUri = proto + "//" + host + "/login";
  UpdateUri = proto + "//" + host + "/update";
  if (host=="localhost") {
    wsUri = "ws://192.168.2.20/ws";
    loginUri = "http://192.168.2.20/login";
    UpdateUri= "http://192.168.2.20/update"
  }
  login();
});
