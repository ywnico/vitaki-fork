PlayStation Vita build instructions
===================================
Building the Vita VPK locally requires a VitaSDK installation with the
`VITASDK` environment variable pointing to its install location.

Alternatively you can run the build with Docker.

Build Project
-------------
```
bash scripts/vita/run-docker-build-chiaki.sh
```

Development Tool
----------------
Use of the tool requires a PS Vita running Henkaku with the following plugins
installed:

- [vitacompanion](https://github.com/devnoname120/vitacompanion)
- [kvdb](https://github.com/DaveeFTW/kvdb) and [vdbtcp](https://github.com/DaveeFTW/vdbtcp) (optional, if gdb debugging is needed)

**Build, upload, launch and show debug log:**
```
python3 ./scripts/vita/devtool.py -h $VITAIP run
```

**Fetch latest backtrace:**
```
python3 ./scripts/vita/devtool.py -h $VITAIP backtrace
```

If the `kvdb` and `vdbtcp` plugins are installed on the Vita, you can attach
to GDB on port `31337` to debug the app.

Chiaki Configuration File
-------------------------
The Vita version uses the same INI file format as the Desktop and Switch
versions, i.e. you can just use FTP to copy your existing config to
`ux0:/data/chiaki-vita/chiaki.conf`.
