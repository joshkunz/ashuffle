.PHONY: default clean

BIN = ashuffle

ashuffle:
	@cd src && $(MAKE) $(BIN) 
	@printf ' COPY src/$(BIN) .\n'; cp src/$(BIN) .

clean:
	@cd src && $(MAKE) clean
	rm $(BIN)

.DEFAULT:
	cd src && $(MAKE) $@
