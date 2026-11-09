const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm'),path=require('node:path');
const sketch=fs.readFileSync(path.join(__dirname,'../Backload/Backload.ino'),'utf8');
const start=sketch.indexOf('let infoBusy=false;'),end=sketch.indexOf("if(document.body.classList.contains('history-page'))",start);
assert(start>0&&end>start);
const nodes=new Map();const node=id=>{if(!nodes.has(id))nodes.set(id,{textContent:'',className:''});return nodes.get(id);};
let data={chip:'ESP32-S3',revision:'0.2',cores:2,cpuMHz:240,uptimeSeconds:120,firmware:'2.6.0',arduinoCore:'2.0.13',idf:'4.4',flashBytes:16777216,heapBytes:327680,heapFreeBytes:80000,psramBytes:8388608,psramFreeBytes:7000000,runningSlot:null,updateSlot:null,
 wifi:{state:'connected',ssid:'Heimnetz',activeHostname:'heizung',mdnsReady:true,rssi:-62,ip:'192.168.1.7',pairing:false},wifiDetails:{mac:'aa:bb',bssid:'cc:dd',channel:6,auth:3,standards:'802.11g / 802.11n',mask:'255.255.255.0',gateway:'192.168.1.1',dns1:'192.168.1.1',dns2:'0.0.0.0',apActive:true,apSsid:'Notzugang',apIp:'192.168.4.1',apMac:'ee:ff',apClients:1}};
const ctx=vm.createContext({Intl,Number,Math,document:{getElementById:node,querySelectorAll:()=>[...nodes.values()]},request:async()=>data});
vm.runInContext(sketch.slice(start,end),ctx);
(async()=>{
 await ctx.loadDeviceInfo();assert.equal(node('info-wifi-ssid').textContent,'Heimnetz');assert.equal(node('info-wifi-rssi').textContent,'-62 dBm');
 assert.equal(node('info-wifi-gateway').textContent,'192.168.1.1');assert.equal(node('info-wifi-dns').textContent,'192.168.1.1');
 assert.equal(node('info-wifi-security').textContent,'WPA2');assert.equal(node('info-wifi-standards').textContent,'802.11g / 802.11n');
 assert.equal(node('info-wifi-ap').textContent,'Notzugang · Notzugang');assert.equal(node('info-wifi-clients').textContent,'1');
 data={...data,wifi:{...data.wifi,state:'retry',rssi:null,ip:''},wifiDetails:{...data.wifiDetails,apActive:false,auth:null,standards:'',mask:'',gateway:'',bssid:'',dns1:'',dns2:''}};
 await ctx.loadDeviceInfo();assert.match(node('info-wifi-state').textContent,/Nicht verbunden/);assert.equal(node('info-wifi-ip').textContent,'—');assert.equal(node('info-wifi-rssi').textContent,'—');assert.equal(node('info-wifi-security').textContent,'—');assert.equal(node('info-wifi-ap').textContent,'Aus');
 console.log('PASS: WLAN-Informationen bei Verbindung und Ausfall.');
})().catch(e=>{console.error(e);process.exit(1);});
