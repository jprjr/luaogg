.PHONY: release clean github-release

PKGCONFIG = pkg-config
CFLAGS = -Wall -Wextra
CFLAGS += $(shell $(PKGCONFIG) --cflags ogg)
CFLAGS += $(shell $(PKGCONFIG) --cflags lua)

LDFLAGS = $(shell $(PKGCONFIG) --libs ogg)

VERSION = $(shell LUA_CPATH="./csrc/?.so" lua -e 'print(require("luaogg")._VERSION)')

lib: csrc/luaogg.so

csrc/luaogg.so: csrc/luaogg.c
	$(CC) -shared $(CFLAGS) -o $@ $^ $(LDFLAGS)

github-release: lib
	source $(HOME)/.github-token && github-release release \
	  --user jprjr \
	  --repo luaogg \
	  --tag v$(VERSION)
	source $(HOME)/.github-token && github-release upload \
	  --user jprjr \
	  --repo luaogg \
	  --tag v$(VERSION) \
	  --name luaogg-$(VERSION).tar.gz \
	  --file dist/luaogg-$(VERSION).tar.gz
	source $(HOME)/.github-token && github-release upload \
	  --user jprjr \
	  --repo luaogg \
	  --tag v$(VERSION) \
	  --name luaogg-$(VERSION).tar.xz \
	  --file dist/luaogg-$(VERSION).tar.xz

release: lib
	rm -rf dist/luaogg-$(VERSION)
	rm -rf dist/luaogg-$(VERSION).tar.gz
	rm -rf dist/luaogg-$(VERSION).tar.xz
	mkdir -p dist/luaogg-$(VERSION)/csrc
	rsync -a csrc/ dist/luaogg-$(VERSION)/csrc/
	rsync -a CMakeLists.txt dist/luaogg-$(VERSION)/CMakeLists.txt
	rsync -a LICENSE dist/luaogg-$(VERSION)/LICENSE
	rsync -a README.md dist/luaogg-$(VERSION)/README.md
	sed 's/@VERSION@/$(VERSION)/g' < rockspec/luaogg-template.rockspec > dist/luaogg-$(VERSION)/luaogg-$(VERSION)-1.rockspec
	cd dist && tar -c -f luaogg-$(VERSION).tar luaogg-$(VERSION)
	cd dist && gzip -k luaogg-$(VERSION).tar
	cd dist && xz luaogg-$(VERSION).tar

clean:
	rm csrc/luaogg.so
