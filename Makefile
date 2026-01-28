# Athanasios Karakantas 2023

sampleratechanger: main.c
	@cc -O2 -Wall -Wextra -std=c11 -D_FILE_OFFSET_BITS=64 main.c -lm -o sampleratechanger
	@echo compiling is complete
	@echo
	@echo "please use command ./sampleratechanger <filenames> <target sample rate> to run the program"
	@echo for more options type make help
	@echo
clean:
	@rm -f sampleratechanger
help:
	@echo
	@echo available arguments in order:
	@echo "./sampleratechanger -v (optional) <filenames> <target sample rate>"
	@echo
	@echo -v stands for verbose, extra info will be printed on screen
	@echo
	@echo up to 2048 files, this can change be editing line 15 of main.c
	@echo compatible files are pcm wav and aif files without encoding
	@echo
	@echo target sample rate can be up to 192000
	@echo
	@echo Have fun!


