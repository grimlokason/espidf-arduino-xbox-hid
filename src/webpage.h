#ifndef WEBPAGE_H
#define WEBPAGE_H

const char PAGE_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Fightstick Config</title>
<style>
body { font-family: sans-serif; background:#111; color:#eee; padding:20px; }
table { border-collapse: collapse; width:100%; max-width:600px; }
td, th { border:1px solid #444; padding:8px; text-align:left; }
.on { background:#2a6; font-weight:bold; }
.off { background:#333; }
select { background:#222; color:#eee; padding:4px; }
button { padding:8px 16px; margin-top:12px; cursor:pointer; }
input[type=color] { width:50px; height:30px; border:none; background:none; cursor:pointer; }
</style>
</head>
<body>
<h2>Etat des boutons</h2>
<table id="statusTable"></table>

<h2>Configuration des pins</h2>
<form id="configForm">
<table id="configTable"></table>
<button type="submit">Enregistrer</button>
</form>
<p id="msg"></p>

<h2>Configuration des LEDs</h2>
<form id="ledConfigForm">
<table id="ledConfigTable"></table>
<button type="submit">Enregistrer les LEDs</button>
</form>
<p id="ledMsg"></p>

<script>
let channelsData = [];
let allowedPins = [];
let ledChannelsData = [];

async function loadInitial() {
    const res = await fetch('/channels');
    const data = await res.json();
    channelsData = data.channels;
    allowedPins = data.allowedPins;
    buildConfigTable();
}

function buildConfigTable() {
    const table = document.getElementById('configTable');
    table.innerHTML = '<tr><th>Canal</th><th>Pin actuelle</th></tr>';
    channelsData.forEach((ch, i) => {
        const tr = document.createElement('tr');
        const tdName = document.createElement('td');
        tdName.textContent = ch.name;
        const tdSelect = document.createElement('td');
        const select = document.createElement('select');
        select.name = 'ch' + i;
        allowedPins.forEach(p => {
            const opt = document.createElement('option');
            opt.value = p;
            opt.textContent = 'GPIO ' + p;
            if (p === ch.pin) opt.selected = true;
            select.appendChild(opt);
        });
        tdSelect.appendChild(select);
        tr.appendChild(tdName);
        tr.appendChild(tdSelect);
        table.appendChild(tr);
    });
}

async function pollStatus() {
    try {
        const res = await fetch('/status');
        const data = await res.json();
        const table = document.getElementById('statusTable');
        table.innerHTML = '<tr><th>Canal</th><th>Pin</th><th>Etat</th></tr>';
        data.forEach(ch => {
            const tr = document.createElement('tr');
            tr.className = ch.pressed ? 'on' : 'off';
            tr.innerHTML = '<td>' + ch.name + '</td><td>GPIO ' + ch.pin + '</td><td>' + (ch.pressed ? 'APPUYE' : '-') + '</td>';
            table.appendChild(tr);
        });
    } catch(e) {}
    setTimeout(pollStatus, 200);
}

document.getElementById('configForm').addEventListener('submit', async (e) => {
    e.preventDefault();
    const formData = new FormData(e.target);
    const payload = {};
    for (const [key, value] of formData.entries()) {
        payload[key] = parseInt(value);
    }
    const res = await fetch('/config', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(payload)
    });
    const msg = document.getElementById('msg');
    if (res.ok) {
        msg.textContent = 'Configuration enregistree.';
        loadInitial();
    } else {
        msg.textContent = 'Erreur: ' + await res.text();
    }
});

async function loadLedConfig() {
    const res = await fetch('/ledchannels');
    ledChannelsData = await res.json();
    buildLedConfigTable();
}

function buildLedConfigTable() {
    const table = document.getElementById('ledConfigTable');
    table.innerHTML = '<tr><th>Bouton</th><th>Position LED (paire)</th><th>Couleur</th></tr>';
    ledChannelsData.forEach((ch, i) => {
        const tr = document.createElement('tr');

        const tdName = document.createElement('td');
        tdName.textContent = ch.name;

        const tdIndex = document.createElement('td');
        const select = document.createElement('select');
        select.name = 'idx' + i;
        for (let p = 0; p <= 14; p += 2) {
            const opt = document.createElement('option');
            opt.value = p;
            opt.textContent = 'LED ' + p + '-' + (p + 1);
            if (p === ch.ledIndex) opt.selected = true;
            select.appendChild(opt);
        }
        tdIndex.appendChild(select);

        const tdColor = document.createElement('td');
        if (ch.colorConfigurable) {
            const colorInput = document.createElement('input');
            colorInput.type = 'color';
            colorInput.name = 'col' + i;
            colorInput.value = ch.color;
            tdColor.appendChild(colorInput);
        } else {
            const swatch = document.createElement('div');
            swatch.style.width = '30px';
            swatch.style.height = '20px';
            swatch.style.background = ch.color;
            swatch.style.border = '1px solid #666';
            tdColor.appendChild(swatch);
        }

        tr.appendChild(tdName);
        tr.appendChild(tdIndex);
        tr.appendChild(tdColor);
        table.appendChild(tr);
    });
}

document.getElementById('ledConfigForm').addEventListener('submit', async (e) => {
    e.preventDefault();
    const formData = new FormData(e.target);
    const payload = {};
    for (const [key, value] of formData.entries()) {
        if (key.startsWith('idx')) payload[key] = parseInt(value);
        else payload[key] = value;
    }
    const res = await fetch('/ledconfig', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(payload)
    });
    const msg = document.getElementById('ledMsg');
    msg.textContent = res.ok ? 'LEDs enregistrees.' : 'Erreur: ' + await res.text();
    if (res.ok) loadLedConfig();
});

loadInitial();
pollStatus();
loadLedConfig();
</script>
</body>
</html>
)HTML";

#endif