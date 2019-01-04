.PHONY: default clean ashuffle install uninstall

BIN = ashuffle
SRC = ./src
BINPATH = $(SRC)/$(BIN)

$(BIN):
	@cd $(SRC) && $(MAKE) $(BIN) 
	@printf ' COPY $(BINPATH) .\n'; cp $(BINPATH) .

clean:
	@cd $(SRC) && $(MAKE) clean
	rm $(BIN)

prefix = /usr/local

install: $(BIN)
	-mkdir -p $(prefix)/bin
	install $< $(prefix)/bin/$<

uninstall:
	rm $(prefix)/bin/$(BIN)

.DEFAULT:
	cd $(SRC) && $(MAKE) $@
