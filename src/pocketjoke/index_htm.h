#pragma once

const char PORTAL_HTML[] PROGMEM = R"=====(

<html>
  <head>
    <title>AILite</title>
    <style>
      body {
        font-family: sans-serif;
      }
      fiedset > label {
        display: block;
        margin-bottom: 1em;
      }

      #cats {
        padding: 5px;
        margin-top: 5px;
        background: #eee;
        color: #333;
      }

      #cats.disabled {
        background: #ccc;
        color: #aaa;
      }
    </style>
    <script>
      function handleSelect(e) {
        // hide/show #ssid_custom based on ssid value
        const i = document.getElementById('ssid_custom')
        if (e.target.value === '') {
          i.style.display = 'inline-block'
        } else {
          i.style.display = 'none'
        }
      }

      function toggleCats(e) {
        // if Any, set #cats.disabled and disable all checkboxes in there
        // if Custom, remove .disabled from #cats, and undisable all checkboxes in there
        const c = document.getElementById('cats')
        if (e.target.value === 'any') {
          c.classList.add('disabled')
          for (const e of c.querySelectorAll('input')) {
            e.disabled = true
          }
        } else {
          c.classList.remove('disabled')
          for (const e of c.querySelectorAll('input')) {
            e.disabled = false
          }
        }
      }

      async function handleSubmit(e) {
        e.preventDefault()

        // build joke URL from form, and save it + wifi creds

        const c = {
          password: e.target.password.value
        }

        c.ssid = e.target.ssid.value === '' ? e.target.ssid_custom.value : e.target.ssid.value

        const cat = []

        if (document.getElementById('catSelectCustom').checked) {
          const clist = document.querySelectorAll('#cats input:checked')
          if (!clist.length) {
            cat.push('Any')
          }
          for (const e of clist) {
            cat.push(e.value)
          }
        } else {
          cat.push('Any')
        }
        c.url = `https://v2.jokeapi.dev/joke/${cat.join(',')}?format=json`

        const blist = document.querySelectorAll('#blocklist input:checked')
        if (blist.length) {
          b = []
          for (const e of blist) {
            b.push(e.value)
          }
          c.url += `&blacklistFlags=${b.join(',')}`
        }

        try {
          await fetch('/save', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(c) })
          alert('Settings Saved! Restarting AILite...')
        } catch (e) {
          alert('Transmission failed!')
        }
      }
    </script>
  </head>
  <body>
    <form onSubmit="handleSubmit(event)">
      <fieldset>
        <legend>Wifi</legend>
        <label>SSID
          <input type="text" id="ssid_custom" value="{{SSID}}" />
          <select id="ssid" value="{{SSID}}" onChange="handleSelect(event)">
            <option value="">Custom</option>
            {{WIFI_OPTIONS}}
          </select>
        </label>
        <label>Password
          <input type="text" id="password" value="{{PASS}}" />
        </label>
      </fieldset>

      <fieldset>
        <legend>Jokes</legend>

        <label>Any
          <input type="radio" name="catSelect" value="any" checked onClick="toggleCats(event)" />
        </label>

        <label>Custom
          <input type="radio" id="catSelectCustom" name="catSelect" value="custom" onClick="toggleCats(event)" />
          <div id="cats" class="disabled">
            <label><input type="checkbox" value="Programming" disabled /> Programming</label>
            <label><input type="checkbox" value="Misc" disabled /> Misc</label>
            <label><input type="checkbox" value="Dark" disabled /> Dark</label>
            <label><input type="checkbox" value="Pun" disabled /> Pun</label>
            <label><input type="checkbox" value="Spooky" disabled /> Spooky</label>
            <label><input type="checkbox" value="Christmas" disabled /> Christmas</label>
          </div>
        </label>

        <label style="display: block; margin-top: 20px">Blocklist
          <div id="blocklist">
            <label><input type="checkbox" value="nsfw" /> nsfw</label>
            <label><input type="checkbox" value="religious" /> religious</label>
            <label><input type="checkbox" value="political" /> political</label>
            <label><input type="checkbox" value="racist" /> racist</label>
            <label><input type="checkbox" value="sexist" /> sexist</label>
            <label><input type="checkbox" value="explicit" /> explicit</label>
          </div>
        </label>
      </fieldset>
      <input type="submit" value="save" style="margin-top: 10px" />
    </form>

    <script>
      const jokeUrl = new URL("{{URL}}");
      const flags = jokeUrl.searchParams.get("blacklistFlags")?.split(",") || [];
      for (const f of flags) {
        document.querySelector(`#blocklist input[value=${f}]`).checked = true;
      }
    </script>

  </body>
</html>

)=====";