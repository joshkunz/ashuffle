.PHONY: default clean ashuffle

BIN = ashuffle
SRC = ./src
BINPATH = $(SRC)/$(BIN)

$(BIN):
	@cd $(SRC) && $(MAKE) $(BIN) 
	@printf ' COPY $(BINPATH) .\n'; cp $(BINPATH) .

clean:
	@cd $(SRC) && $(MAKE) clean
	rm $(BIN)

.DEFAULT:
	cd $(SRC) && $(MAKE) $@
