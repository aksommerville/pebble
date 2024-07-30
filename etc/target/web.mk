web-all:
web_MIDDIR:=mid/web
web_OUTDIR:=out/web

web_SRCFILES:=$(filter src/web/%,$(SRCFILES))

# pebble.html: Web app with friendly UI that lets you play a local ROM file.
web_PEBBLE:=$(web_OUTDIR)/pebble.html
#web-all:$(web_PEBBLE)
$(web_PEBBLE):$(web_SRCFILES);$(PRECMD) echo "TODO pack web app" ; exit 1

# template.html: Iframable web app with placeholders to insert the ROM file.
web_TEMPLATE:=$(web_OUTDIR)/template.html
#web-all:$(web_TEMPLATE)
$(web_TEMPLATE):$(web_SRCFILES);$(PRECMD) echo "TODO pack web app" ; exit 1

# web-serve: Dumb HTTP server to deliver the generic web runtime.
# Do it this way if you're not using node, but do have some trivial HTTP server (NB `http-server` uses node too).
#web-serve:$(demo_ROM);http-server -c-1 src/web

# web-serve: Our live Node server. Doing it this way lets you trigger builds from the web app.
web-serve:$(demo_ROM);node src/web/server.js --rom=$(demo_ROM) --make --htdocs=src/web
